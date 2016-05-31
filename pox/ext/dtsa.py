import hashlib
import struct

from pox.core import core
from protocol import dts_pb2, etcp_pb2, dtscp_pb2
from dts_wire import buffer_splitter
import pox.openflow.libopenflow_01 as of

log = core.getLogger()

class Entity(object):
    def __init__(self, title, switch_dpid, port):
        self.title = title
        self.switch_dpid = switch_dpid
        self.port = port
        self.workspaces = {}

# TODO: implement graph algorithms and make it work for more than one switch
class Workspace(object):
    class WPSwitch(object):
        def __init__(self, dpid):
            self.dpid = dpid
            self.used_ports = set()

    def __init__(self, title):
        self.title = title
        self.title_hash = hashlib.sha256(title).digest()[:12]
        self.entities = {}
        self.switches = {}

    def attach(self, entity):
        if entity.title in self.entities:
            return False

        self.entities[entity.title] = entity
        entity.workspaces[self.title] = self

        try:
            s = self.switches[entity.switch_dpid]
        except KeyError:
            s = self.WPSwitch(entity.switch_dpid)
            self.switches[s.dpid] = s

        # TODO: establish route between switches. In mean time,
        # only one switch will work.

        s.used_ports.add(entity.port)

        msg = of.ofp_flow_mod()
        msg.match.dl_dst = self.title_hash[:6]
        msg.match.dl_src = self.title_hash[6:]
        msg.match.dl_type = 0x0880
        msg.command = of.OFPFC_MODIFY_STRICT
        msg.actions = [of.ofp_action_output(port=p) for p in s.used_ports]
        core.openflow.sendToDPID(s.dpid, msg)

        return True

    def detach(self, entity):
        if entity.title not in self.entities:
            return False

        try:
            s = self.switches[entity.switch_dpid]
        except KeyError:
            return False

        s.used_ports.discard(entity.port)

        msg = of.ofp_flow_mod()
        msg.match.dl_dst = self.title_hash[:6]
        msg.match.dl_src = self.title_hash[6:]
        msg.match.dl_type = 0x0880

        if s.used_ports:
            msg.command = of.OFPFC_MODIFY_STRICT
            msg.actions = [of.ofp_action_output(port=p) for p in s.used_ports]
        else:
            msg.command = of.OFPFC_DELETE_STRICT
            del self.switches[s.dpid]

        core.openflow.sendToDPID(s.dpid, msg)

        del entity.workspaces[self.title]
        del self.entities[entity.title]

        return True

# TODO: Deal with circular reference... or don't, since we expect the
# DTSA instace to last for the whole lifetime of the application.
class DTSA(object):
    HEADER = "DTS\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x80"

    def __init__(self):
        self.entities = {}
        self.workspaces = {}
        core.openflow.addListeners(self)
        # TODO: insert flow that will redirect to here DTS addressed packages

    @staticmethod
    def reply_request(success, request_id, event):
        # Build the response
        resp_msg = dts_pb2.ControlResponse()
        resp_msg.status = resp_msg.SUCCESS if success else resp_msg.FAILURE
        if request_id != None:
            resp_msg.request_id = request_id
        resp_msg = resp_msg.SerializeToString()

        # Send response back to the entity
        resp = of.ofp_packet_out()
        resp.data = ''.join((DTSA.HEADER, struct.pack("<H", len(resp_msg)),
                             resp_msg))
        resp.actions.append(of.ofp_action_output(port = event.port))
        event.connection.send(resp)

    def on_entity_register(self, msg, request_id, event):
        success = False
        try:
            # Register entity
            e = Entity(msg.title, event.dpid, event.port)
            if msg.title not in self.entities:
                self.entities[msg.title] = e
                success = True
                log.info('Registered entity "{}".'.format(msg.title))
        except:
            pass

        self.reply_request(success, request_id, event)

    def on_entity_unregister(self, msg, *rest):
        success = False
        try:
            e = self.entities[msg.title]

            # Detach from any workspace it might be.
            # Use .values() instead of .itervalues() because the dictionary
            # will be modified by .detach(), so a copy is needed.
            for w in e.workspaces.values():
                w.detach(e)

            del self.entities[msg.title]
            log.info('Entity "{}" unregistered.'.format(msg.title))
            success = True
        except KeyError:
            pass

        self.reply_request(success, *rest)

    def on_workspace_create(self, msg, *rest):
        if ((msg.workspace_title in self.workspaces)
            or (msg.attach_too
                and (not msg.HasField('entity_title')
                     or msg.entity_title not in self.entities))):
            success = False
        else:                                    
            w = Workspace(msg.workspace_title)
            self.workspaces[msg.workspace_title] = w

            log.info('Registered workspace "{}".'.format(msg.workspace_title))

            if msg.attach_too:
                w.attach(self.entities[msg.entity_title])
                log.info('  and attached entity "{}".'.format(msg.entity_title))

            success = True

        self.reply_request(success, *rest)

    def on_workspace_attach(self, msg, *rest):
        success = False
        if (msg.workspace_title in self.workspaces
            and msg.entity_title in self.entities):

            w = self.workspaces[msg.workspace_title]
            if msg.entity_title not in w.entities:
                w.attach(self.entities[msg.entity_title])
                log.info('Attached entity "{}" to workspace "{}".'
                         .format(msg.entity_title, msg.workspace_title))
                success = True

        self.reply_request(success, *rest)

    def on_workspace_detach(self, msg, *rest):
        success = False
        if (msg.workspace_title in self.workspaces
            and msg.entity_title in self.entities):

            w = self.workspaces[msg.workspace_title]
            if msg.entity_title in w.entities:
                w.detach(self.entities[msg.entity_title])
                success = True
                log.info('Detached entity "{}" from workspace "{}".'
                         .format(msg.entity_title, msg.workspace_title))

        self.reply_request(success, *rest)

    def on_workspace_delete(self, msg, *rest):
        success = False
        try:
            w = self.workspaces[msg.title]

            # Can't delete if there are etities attached because there
            # is no way to signal them the workspace is no more...
            if not w.entities:
                del self.workspaces[msg.title]
                success = True
                log.info('Deleted workspace "{}".'.format(msg.title))
        except KeyError:
            pass

        self.reply_request(success, *rest)

    def on_workspace_lookup(self, msg, *rest):
        # TODO...
        pass

    def on_workspace_advertise(self, msg, *rest):
        # TODO...
        pass

    def on_dtsa_register(self, msg, *rest):
        # TODO...
        pass

    cr = dts_pb2.ControlRequest
    MSG_HANDLER = {
        cr.ETCP_ENTITY_REGISTER:
            (on_entity_register, etcp_pb2.EntityRegister),
        cr.ETCP_ENTITY_UNREGISTER:
            (on_entity_unregister, etcp_pb2.EntityUnregister),
        cr.ETCP_WORKSPACE_CREATE:
            (on_workspace_create, etcp_pb2.WorkspaceCreate),
        cr.ETCP_WORKSPACE_ATTACH:
            (on_workspace_attach, etcp_pb2.WorkspaceAttach),
        cr.ETCP_WORKSPACE_DETACH:
            (on_workspace_detach, etcp_pb2.WorkspaceDetach),
        cr.ETCP_WORKSPACE_DELETE:
            (on_workspace_delete, etcp_pb2.WorkspaceDelete),
        cr.DTSCP_WORKSPACE_LOOKUP:
            (on_workspace_lookup, dtscp_pb2.WorkspaceLookup),
        cr.DTSCP_WORKSPACE_ADVERTISE:
            (on_workspace_advertise, dtscp_pb2.WorkspaceAdvertise),
        cr.DTSCP_DTSA_REGISTER:
            (on_dtsa_register, dtscp_pb2.DTSARegister)
        }
    del cr

    def _handle_PacketIn(self, event):
        def cleanBuffer(buff):            # for working with certain type of switches' controllers
           while ord(buff[-1]) == 0:
               buff = buff[:-1]
           return buff

        if event.data[:14] != self.HEADER:
            # Unknown title and ethertype identifier; discard...
            log.debug('Discarding nos-DTSish message...')
            return

        # If the full message was not sent, but instead kept
        # in switch's buffer, request it entirely...
        if event.ofp.total_len != len(event.data):
            log.info('Partial package received. Requesting it fully...')
            msg = of.ofp_packet_out(in_port=of.OFPP_NONE)
            msg.actions.append(of.ofp_action_output(port = of.OFPP_CONTROLLER))
            msg.data = event.ofp

            event.connection.send(msg)
            return

        buff = cleanBuffer(event.data[14:])
        print 'El buffer recebido:', buff.encode('string-escape')
        request = dts_pb2.ControlRequest()
        buff_iter = buffer_splitter(buff)
        while True:
            try:
                request.ParseFromString(buff_iter.next())
            except StopIteration:
                break

            msg_buff = buff_iter.next()
            try:
                (handler, msg_class) = self.MSG_HANDLER[request.type]
                msg = msg_class()
                msg.MergeFromString(msg_buff)
                handler(self, msg, request.id, event)
            except KeyError:
                # TODO: deal with unknown message type
                pass

def launch():
    core.registerNew(DTSA)
