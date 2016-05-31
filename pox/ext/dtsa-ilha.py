# normal:  ./pox.py openflow.discovery samples.pretty_log dtsa-smart 
# debug:   ./pox.py log.level --DEBUG samples.pretty_log openflow.discovery --no_flow=true --explicit_drop=disabled dtsa-smart
# poxdesk: ./pox.py samples.pretty_log web messenger messenger.log_service messenger.ajax_transport openflow.of_service poxdesk openflow.discovery dtsa-smart
# poxdesk: http://127.0.0.1:8000/poxdesk
# Parameters for dtsa-final:
#   Name                      Default                   Active
#   ---------------           ---------------           ---------------
#   graph                     False                     False
#   queues                    False                     False
#   wifi                      False                     False
#   help                      False                     False
#   aggr                      False                     False

import hashlib
import struct
import threading
import time
import sys
import pox.openflow.libopenflow_01 as of
import libopenflow_qos_01 as ofq
import libopenflow_qos_01_2 as ofq2
import networkx as nx
import matplotlib.pyplot as plt
from pox.core import core
from pox.lib.addresses import EthAddr
from pox.lib.util import dpid_to_str, str_to_dpid
from protocol import dts_pb2, etcp_pb2, dtscp_pb2
from dts_wire import buffer_splitter

log     = core.getLogger()
graph   = nx.Graph()
ports   = {}
ingress = set()
egress  = set()
wks     = {}
workspaces_aggr = {}

class Entity(object):
    def __init__(self, title, switch_dpid, port):
        self.title       = title
        self.switch_dpid = switch_dpid
        self.port        = port
        self.workspaces  = {}

class Workspace(object):
    class WPSwitch(object):
        def __init__(self, dpid):
            self.dpid        = dpid
            self.used_ports  = set()

        def __repr__(self):
            return 'used ports: ' + str(self.used_ports)

    def __init__(self, title, wifi=False, addrResp=None, qos=False, bw=None, cos=None):
        self.title     = title
        self.wifi      = wifi
        self.addrResp  = addrResp
        self.qos       = qos
        self.bw        = bw
        self.cos       = cos
        self.entities  = {}
        self.switches  = {}
        self.switches2 = []
        self.path      = []
        if not wifi:
             self.title_hash = hashlib.sha256(title).digest()[:12]
        else:
             self.title_hash = hashlib.sha256(title).digest()[:6] + addrResp[:6]

        if not qos:
            log.info('Registered workspace "{}".'.format(title))
        else:
            log.info('Registered SMART QoS workspace "{}".'.format(title))

    def __repr__(self):
        return 'workspace ' + self.title

    def create(self, switch1, switch2):
        self.switches2 += [switch1, switch2]
        try:
            s = self.switches[switch1]
            s = self.switches[switch2]
        except KeyError:
            s = self.WPSwitch(switch1)
            self.switches[s.dpid] = s
            s = self.WPSwitch(switch2)
            self.switches[s.dpid] = s

        if self.qos:
            path = nx.shortest_path (graph, switch1, switch2, weight='cost')
            log.info('  -> path with minimum QoS cost between {} and {} is: {}'.format(switch1, switch2, path))
        else:
            path = nx.shortest_path (graph, switch1, switch2)
            log.info('  -> shortest path between {} and {} is: {}'.format(switch1, switch2, path))

        self.path = path

        msg = of.ofp_flow_mod()
        msg.command = of.OFPFC_MODIFY_STRICT      # of.OFPFC_MODIFY || of.OFPFC_ADD
        #msg.match.dl_type = 0x0880               # Causes conflicts with user-space switches
        msg.match.dl_dst = self.title_hash[:6]
        msg.match.dl_src = self.title_hash[6:]
        if path.__len__() > 1:
            if self.bw and self.cos:
                log.info("  -> enqueuing in queue 1, corresponding to CoS silver")
            for i in path:
                if i == path[-1]:
                    self.switches[i].used_ports.add(graph[i][path[path.index(i)-1]]['ports'][i])
                elif i == path[0]:
                    self.switches[i].used_ports.add(graph[i][path[path.index(i)+1]]['ports'][i])
                else:
                    try:
                        s = self.switches[i]
                    except KeyError:
                        s = self.WPSwitch(i)
                        self.switches[i] = s
                    self.switches[i].used_ports.add(graph[i][path[path.index(i)+1]]['ports'][i])
                    self.switches[i].used_ports.add(graph[i][path[path.index(i)-1]]['ports'][i])
                if self.bw and self.cos:
                    msg.actions = [of.ofp_action_enqueue(port=p, queue_id=0) for p in self.switches[i].used_ports] # ofp_action_enqueue(port=0, queue_id=0)
                else:
                    msg.actions = [of.ofp_action_enqueue(port=p, queue_id=1) for p in self.switches[i].used_ports]
                core.openflow.sendToDPID(i, msg)
        else:
            msg.actions = [of.ofp_action_output(port=p) for p in s.used_ports]
            core.openflow.sendToDPID(s.dpid, msg)

    def attach(self, entity, packet=None):
        if entity.title in self.entities:
            return False
        self.entities[entity.title] = entity
        entity.workspaces[self.title] = self
        self.switches2 += [entity.switch_dpid]
        try:
            s = self.switches[entity.switch_dpid]
        except KeyError:
            s = self.WPSwitch(entity.switch_dpid)
            self.switches[s.dpid] = s
        s.used_ports.add(entity.port)

        #print 'self.switches.keys', self.switches.keys()     #print graph.nodes()

        if self.qos:
            path = nx.shortest_path (graph, self.switches2[-1], self.switches2[0], weight='cost')
            log.info('  -> path with minimum QoS cost between {} and {} is: {}'.format(self.switches2[-1], self.switches2[0], path))
        else:
            path = nx.shortest_path (graph, self.switches2[-1], self.switches2[0])
            log.info('  -> shortest path between {} and {} is: {}'.format(self.switches2[-1], self.switches2[0], path))

        if not self.wifi:
            msg = of.ofp_flow_mod()
            #msg.match.dl_type = 0x0880               # Causes conflicts with user-space switchs
            msg.command = of.OFPFC_MODIFY_STRICT      # of.OFPFC_MODIFY || of.OFPFC_ADD
            msg.match.dl_dst = self.title_hash[:6]
            msg.match.dl_src = self.title_hash[6:]
            if path.__len__() > 1:
                if self.bw and self.cos:
                    log.info("  -> enqueuing in queue 1, corresponding to CoS silver")
                for i in path:
                    if i == path[-1]:
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)-1]]['ports'][i])
                    elif i == path[0]:
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)+1]]['ports'][i])
                    else:
                        try:
                            s = self.switches[i]
                        except KeyError:
                            s = self.WPSwitch(i)
                            self.switches[i] = s
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)+1]]['ports'][i])
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)-1]]['ports'][i])
                    if self.bw and self.cos:
                        msg.actions = [of.ofp_action_enqueue(port=p, queue_id=0) for p in self.switches[i].used_ports] # ofp_action_enqueue(port=0, queue_id=0)
                    else:
                        msg.actions = [of.ofp_action_enqueue(port=p, queue_id=1) for p in self.switches[i].used_ports]
                    core.openflow.sendToDPID(i, msg)
            else:
                #print 'output:', s.used_ports, 'dpid:', s.dpid
                #msg.actions = [of.ofp_action_output(port=p) for p in s.used_ports]
                if self.bw and self.cos:
                     msg.actions = [of.ofp_action_enqueue(port=p, queue_id=0) for p in s.used_ports]
                else:
                     msg.actions = [of.ofp_action_enqueue(port=p, queue_id=1) for p in s.used_ports]
                core.openflow.sendToDPID(s.dpid, msg)
        else: # ============================================================================================================================== wifi
            rule1 = of.ofp_flow_mod()
            rule1.match.dl_dst = self.title_hash[:6]
            #rule1.match.dl_type = 0x0880                 # Causes conflicts with user-space switch
            rule1.command = of.OFPFC_MODIFY_STRICT
            rule1.actions.append(of.ofp_action_dl_addr.set_src(self.title_hash[:6]))
            rule1.actions.append(of.ofp_action_dl_addr.set_dst(EthAddr("FF:FF:FF:FF:FF:FF")))
            rule2 = of.ofp_flow_mod()
            rule2.match.dl_dst = EthAddr("FF:FF:FF:FF:FF:FF")
            rule2.match.dl_src = self.title_hash[6:]
            #rule2.match.dl_type = 0x0880
            rule2.command = of.OFPFC_MODIFY_STRICT
            if path.__len__() > 1:
                for i in path:
                    if i == path[-1]:
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)-1]]['ports'][i])
                    elif i == path[0]:
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)+1]]['ports'][i])
                        for p in self.switches[i].used_ports:
                            rule1.actions.append(of.ofp_action_output(port=p))
                        core.openflow.sendToDPID(i, rule1)
                    else:
                        try:
                            s = self.switches[i]
                        except KeyError:
                            s = self.WPSwitch(i)
                            self.switches[i] = s
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)+1]]['ports'][i])
                        self.switches[i].used_ports.add(graph[i][path[path.index(i)-1]]['ports'][i])
                    if self.bw and self.cos:
                        # print 'enqueue'
                        rule2.actions = [of.ofp_action_enqueue(port=p) for p in self.switches[i].used_ports] # ofp_action_enqueue(port=0, queue_id=0)
                    else:
                        rule2.actions = [of.ofp_action_output(port=p)  for p in self.switches[i].used_ports]
                    core.openflow.sendToDPID(i, rule2)
            else:
                for p in s.used_ports:
                    rule1.actions.append(of.ofp_action_output(port=p))
                core.openflow.sendToDPID(s.dpid, rule1)
                rule2.actions = [of.ofp_action_output(port=p) for p in s.used_ports]
                core.openflow.sendToDPID(s.dpid, rule2)

        #print 'self.switches', self.switches    # for debug

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
        #msg.match.dl_type = 0x0880          # Causes conflicts with user-space switches

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

    def __init__(self, wifi=False, aggr=False):
        self.wifi        = wifi
        self.addrResp    = None
        self.aggr        = aggr
        self.flow        = 0
        self.entities    = {}
        self.workspaces  = {}
        def startup ():
            core.openflow.addListeners(self, priority=0)
        core.call_when_ready(startup, ('openflow'))

        # TODO: insert flow that will redirect to here DTS addressed packages

    #@staticmethod
    def reply_request(self, success, request_id, event, title_aggr=None, w = None):
        # Build the response
        resp_msg = dts_pb2.ControlResponse()
        resp_msg.status = resp_msg.SUCCESS if success else resp_msg.FAILURE
        if request_id != None:
            resp_msg.request_id = request_id

        if self.aggr and w:                            # reply for the second workspace attachment
            print 'title_aggr', title_aggr, 'flow', self.flow
            # Send FLOW_MOD to the swith that is ataching an entity
            title      = w.title
            if title_aggr in workspaces_aggr:
                w2 = workspaces_aggr[title_aggr]
            else:
                log.info('Unknown aggregated workspace title')
            title_hash      = hashlib.sha256(title).digest()[:12]
            title_hash_aggr = hashlib.sha256(title_aggr).digest()[:12]
            recvRule = of.ofp_flow_mod()
            recvRule.command = of.OFPFC_MODIFY_STRICT
            #recvRule.match.dl_type = 0x0880         # Causes conflicts with user-space switches
            recvRule.match.dl_dst = title_hash_aggr[:6]
            recvRule.match.dl_src = title_hash_aggr[6:]
            recvRule.match.dl_vlan = self.flow
            recvRule.actions.append(of.ofp_action_dl_addr.set_dst(title_hash[:6]))
            recvRule.actions.append(of.ofp_action_dl_addr.set_src(title_hash[6:]))
            recvRule.actions.append(of.ofp_action_enqueue(port=event.port, queue_id=1))
            core.openflow.sendToDPID(event.dpid, recvRule)
            try:
                s = w.switches[event.dpid]
            except KeyError:
                s = w.WPSwitch(event.dpid)
                w.switches[s.dpid] = s
            if event.dpid == w2.path[-1]:
                s.used_ports.add(graph[w2.path[-1]][w2.path[w2.path.index(w2.path[-1])-1]]['ports'][w2.path[-1]])
            else:
                s.used_ports.add(graph[w2.path[0]][w2.path[w2.path.index(w2.path[0])+1]]['ports'][w2.path[0]])
            sendRule = of.ofp_flow_mod()
            sendRule.command = of.OFPFC_MODIFY_STRICT
            #sendRule.match.dl_type = 0x0880        # Causes conflicts with user-space switches
            sendRule.match.dl_dst = title_hash[:6]
            sendRule.match.dl_src = title_hash[6:]
            sendRule.actions.append(of.ofp_action_dl_addr.set_dst(title_hash_aggr[:6]))
            sendRule.actions.append(of.ofp_action_dl_addr.set_src(title_hash_aggr[6:]))
            sendRule.actions.append(of.ofp_action_vlan_vid(vlan_vid=self.flow))
            for p in s.used_ports:
                sendRule.actions.append(of.ofp_action_enqueue(port=p, queue_id=1))
            core.openflow.sendToDPID(event.dpid, sendRule)
            # Send FLOW_MOD to the switch that created the workspace
            switch1 = wks[w.title]
            print 'OZUUUUUU1', switch1.dpid, switch1.port,
            try:
                s = w.switches[switch1.dpid]
            except KeyError:
                s = w.WPSwitch(switch1.dpid)
                w.switches[s.dpid] = s
            if switch1.dpid == w2.path[0]:
                s.used_ports.add(graph[w2.path[0]][w2.path[w2.path.index(w2.path[0])+1]]['ports'][w2.path[0]])
            else:
                s.used_ports.add(graph[w2.path[-1]][w2.path[w2.path.index(w2.path[-1])-1]]['ports'][w2.path[-1]])
            sendRule = of.ofp_flow_mod()
            sendRule.command = of.OFPFC_MODIFY_STRICT
            #sendRule.match.dl_type = 0x0880          # Causes conflicts with user-space switches
            sendRule.match.dl_dst = title_hash[:6]
            sendRule.match.dl_src = title_hash[6:]
            sendRule.actions.append(of.ofp_action_dl_addr.set_dst(title_hash_aggr[:6]))
            sendRule.actions.append(of.ofp_action_dl_addr.set_src(title_hash_aggr[6:]))
            sendRule.actions.append(of.ofp_action_vlan_vid(vlan_vid=self.flow))
            for p in s.used_ports:
                sendRule.actions.append(of.ofp_action_enqueue(port=p, queue_id=1))
            core.openflow.sendToDPID(switch1.dpid, sendRule)
            recvRule = of.ofp_flow_mod()
            recvRule.command = of.OFPFC_MODIFY_STRICT
            #recvRule.match.dl_type = 0x0880           # Causes conflicts with user-space switches
            recvRule.match.dl_dst = title_hash_aggr[:6]
            recvRule.match.dl_src = title_hash_aggr[6:]
            recvRule.match.dl_vlan = self.flow
            recvRule.actions.append(of.ofp_action_dl_addr.set_dst(title_hash[:6]))
            recvRule.actions.append(of.ofp_action_dl_addr.set_src(title_hash[6:]))
            recvRule.actions.append(of.ofp_action_enqueue(port=event.port, queue_id=1))
            core.openflow.sendToDPID(switch1.dpid, recvRule)

        resp_msg = resp_msg.SerializeToString()

        # Send response back to the entity
        resp = of.ofp_packet_out()
        if not self.wifi:             # not wifi
            resp.data = ''.join((DTSA.HEADER, struct.pack("<H", len(resp_msg)), resp_msg))
        else:
            resp.data = ''.join((self.addrResp, struct.pack("<H", len(resp_msg)), resp_msg))
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

    def on_workspace_create(self, msg, request_id, event):
        if ((msg.workspace_title in self.workspaces) or (msg.attach_too and (not msg.HasField('entity_title') or msg.entity_title not in self.entities))):
            success = False
        else:
            qos = None; bw = None; cos=None
            for i in range (0, msg.capabilities.__len__()):
                type = msg.capabilities[i].type
                value = msg.capabilities[i].value
                if   type == 1:       # qos_required_bool
                    qos = True
                elif type == 2:       # bw_required_int32
                    bw  = value
                elif type == 3:       # cos_required_string
                    cos = value
                else:
                    log.info('Unknown message type on workspace_create')

            w = Workspace(msg.workspace_title, self.wifi, self.addrResp, qos, bw, cos)
            self.workspaces[msg.workspace_title] = w
            wks[msg.workspace_title] = event    # aggr, en event.dpid esta el switch que hace la peticion

            if msg.attach_too:
                if not self.aggr:
                    w.attach(self.entities[msg.entity_title], event.parsed)
                log.info('  -> attached entity "{}".'.format(msg.entity_title))

            success = True

        self.reply_request(success, request_id, event)

    def on_workspace_attach(self, msg, request_id, event):
        success = False
        w = None
        title = None
        if (msg.workspace_title in self.workspaces and msg.entity_title in self.entities):
            w = self.workspaces[msg.workspace_title]
            if msg.entity_title not in w.entities:
                if not self.aggr:
                    w.attach(self.entities[msg.entity_title], event.parsed)
                log.info('Attached entity "{}" to workspace "{}".'.format(msg.entity_title, msg.workspace_title))
                success = True

        if msg.workspace_title in wks:
            switch1 = wks[msg.workspace_title]
            for i in workspaces_aggr:
                if ((workspaces_aggr[i].switches2[0] ==event.dpid and workspaces_aggr[i].switches2[-1]==switch1.dpid) or
                    (workspaces_aggr[i].switches2[-1]==event.dpid and workspaces_aggr[i].switches2[0] ==switch1.dpid)):
                    title = workspaces_aggr[i].title
                    break

        if title:
            log.info('  -> aggregated workspace "{}" selected to acommodate workspace "{}".'.format(title,msg.workspace_title))
            self.flow += 1
        self.reply_request(success, request_id, event, title, w)

    def on_workspace_detach(self, msg, *rest):
        success = False
        if (msg.workspace_title in self.workspaces and msg.entity_title in self.entities):
            w = self.workspaces[msg.workspace_title]
            if msg.entity_title in w.entities:
                w.detach(self.entities[msg.entity_title])
                success = True
                log.info('Detached entity "{}" from workspace "{}".'.format(msg.entity_title, msg.workspace_title))

        self.reply_request(success, *rest)

    def on_workspace_delete(self, msg, *rest):
        success = False
        try:
            w = self.workspaces[msg.title]
            # Can't delete if there are entities attached because there is no way to signal them the workspace is no more...
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
        def cleanBuffer(buff):          # for properlly working with certain type of user-space switches
           while ord(buff[-1]) == 0:
               buff = buff[:-1]
           return buff

        if not self.wifi:
            if event.data[:14] != self.HEADER:
                # Unknown title and ethertype identifier; discard...
                log.debug('Discarding nos-DTSish message...')
                return
        else:
            if event.data[:6] != self.HEADER[:6] or event.data[:14][-2:] != self.HEADER[-2:]:
                # Unknown title and ethertype identifier; discard...
                log.debug('Discarding nos-DTSish message...')
                return

        # If the full message was not sent, but instead kept in switch's buffer, request it entirely
        if event.ofp.total_len != len(event.data):
            log.info('Partial package received. Requesting it fully...')
            msg = of.ofp_packet_out(in_port=of.OFPP_NONE)
            msg.actions.append(of.ofp_action_output(port = of.OFPP_CONTROLLER))
            msg.data = event.ofp

            event.connection.send(msg)
            return

        if self.wifi:
            self.addrResp = event.data[:12][6:] + event.data[:12][:6] + '\x08\x80'   # print self.addrResp.encode('string-escape')

        buff = cleanBuffer(event.data[14:])
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
                log.info('Unknown message type')
                pass

    def _handle_ConnectionUp (self, event):
        log.debug("Switch %s has come up.", dpid_to_str(event.dpid))
        if event.dpid not in graph.nodes():
            graph.add_node(event.dpid)
        ports[event.dpid] = {}
        for i in event.ofp.ports:
            ports[event.dpid][i.port_no] = i.curr
        # AJUSTE MANUAL ------> BORRAR
        if dpid_to_str(event.dpid) == '00-00-00-00-00-02':
            ports[event.dpid][2] = 130
        if dpid_to_str(event.dpid) == '00-00-00-00-00-03':
            ports[event.dpid][3] = 130
        if   event.ofp.ports[-2].name[0] == 'I':
            ingress.add(event.dpid)
        elif event.ofp.ports[-2].name[0] == 'E':
            egress.add(event.dpid)

def addQueues():
    #print ports
    log.info('Starting queue configuration...')

    for i in graph.nodes():
        #print graph.nodes()[0]
        msg = ofq2.ofp_qos_msg()
        msg.ofp_header = ofq2.ofp_header()
        msg.ofp_header.length = 32
        msg.sched_type = ofq2.ofp_qos_type_rev_map['OFP_QOS_SCHED_HTB']
        msg.body = ofq2.ofp_qos_sqf()
        msg.body.ofp_qos_header = ofq2.ofp_qos_header()
        msg.body.ofp_qos_header.object_type = ofq2.ofp_qos_object_type_rev_map['OFP_QOS_CLASS']
        msg.body.ofp_qos_header.action_type = ofq2.ofp_qos_action_type_rev_map['OFP_QOS_ACTION_ADD']
        for p in [1,2]:
            msg.body.port_no = p
            msg.body.class_id = 1
            msg.body.rate = 1800000
            msg.body.ceil = 2000000
            msg.body.prio = 1
            packed = b""
            packed += msg.ofp_header.pack()
            packed += struct.pack('!BpppBBHIIII', msg.sched_type, '0', '0', '0',
                                  msg.body.ofp_qos_header.object_type, msg.body.ofp_qos_header.action_type,
                                  msg.body.port_no, msg.body.class_id, msg.body.rate, msg.body.ceil, msg.body.prio)
            core.openflow.sendToDPID(i, packed)
            msg.body.class_id = 2
            msg.body.rate = 80000
            msg.body.ceil = 1500000
            packed = b""
            packed += msg.ofp_header.pack()
            packed += struct.pack('!BpppBBHIIII', msg.sched_type, '0', '0', '0',
                                  msg.body.ofp_qos_header.object_type, msg.body.ofp_qos_header.action_type,
                                  msg.body.port_no, msg.body.class_id, msg.body.rate, msg.body.ceil, msg.body.prio)
            core.openflow.sendToDPID(i, packed)

    # To recognize the packet
    msg = of.ofp_flow_mod()
    msg.command = of.OFPFC_MODIFY
    msg.match.dl_type = 0x0880
    msg.action = of.ofp_action_output(port=7)
    core.openflow.sendToDPID(1, msg)

    log.info('  -> queues inicializated')

def startWorkspaceAggregation():
    log.info('Starting workspace aggregation reservations...')
    title = 'A'
    for i in ingress:
        for e in egress:
            #path = nx.shortest_path(graph, i, e)
            w = Workspace(title, wifi=False, addrResp=None, qos=False, bw=None, cos=None)
            w.create(i, e)
            workspaces_aggr[title] = w
            title = chr(ord(title)+1)
    log.info('  -> workspaces inicializated')

def drawGraph():
    time.sleep(15)
    log.info("Network's topology graph:")
    log.info('  -> ingress switches: {}'.format(list(ingress)))
    log.info('  -> egress switches:  {}'.format(list(egress)))
    nx.draw_spectral(graph)
    nx.draw_networkx_edge_labels(graph, pos=nx.spectral_layout(graph))
    #nx.draw_circular(graph)
    #nx.draw_networkx_edge_labels(graph, pos=nx.circular_layout(graph))
    #nx.draw_shell(graph)
    #nx.draw_networkx_edge_labels(graph, pos=nx.shell_layout(graph))
    plt.show()

def launch(help=False, queues=False, aggr=False, graph=False, wifi=False):
    if help:
        print """usage: pox.py openflow.discovery samples.pretty_log dtsa_smart [--help] [--graph] [--wifi]
               \nPOX DTSA-SMART controller with QoS support
               \noptional arguments:\
               \n  --help            show this help message and exit\
               \n  --queues          automatically manage queues configuration\
               \n  --aggr            workspace's aggregation'\
               \n  --graph           show network's topology graph\
               \n  --wifi            run controller in Wifi compatible mode"""
        sys.exit(1)
    core.registerNew(DTSA, wifi, aggr)
    if queues:
        core.callDelayed(20, addQueues)
    if aggr:
        core.callDelayed(14, startWorkspaceAggregation)
    if graph:
        hilo = threading.Thread(target=drawGraph)
        hilo.start()
