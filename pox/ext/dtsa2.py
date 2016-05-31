# normal:  ./pox.py dtsa_smart openflow.discovery samples.pretty_log
# debug:   ./pox.py log.level --DEBUG dtsa_smart samples.pretty_log openflow.discovery --no_flow=true --explicit_drop=disabled
# poxdesk: ./pox.py samples.pretty_log web messenger messenger.log_service messenger.ajax_transport openflow.of_service poxdesk openflow.discovery dtsa_smart
#
# Parameters for dtsa-final:
#  Name                      Default                   Active
#  ---------------           ---------------           ---------------
#  graph                     False                     False
#  wifi                      False                     False
#  help                      False                     False

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
from pox.openflow.discovery import Discovery
from pox.lib.addresses import EthAddr
from pox.lib.util import dpid_to_str, str_to_dpid
from protocol import dts_pb2, etcp_pb2, dtscp_pb2
from dts_wire import buffer_splitter

log = core.getLogger()
g = nx.Graph()
ports = {}

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
        self.title      = title
        self.wifi       = wifi
        self.addrResp   = addrResp
        self.qos        = qos
        self.bw         = bw
        self.cos        = cos
        if not self.wifi:
             self.title_hash = hashlib.sha256(title).digest()[:12]
        else:
             self.title_hash  = hashlib.sha256(self.title).digest()[:6] + addrResp[:6]
             #print 'DEBUG2', self.title_hash.encode('string-escape')
        self.entities   = {}
        self.switches   = {}
        self.switches2  = []

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
    
        #print 'self.switches.keys', self.switches.keys()
        #print g.nodes()

        if self.qos:
            path = nx.shortest_path (g, self.switches2[-1], self.switches2[0], weight='cost')
            strInfo = '  -> path with minimum QoS cost between %d and %d is: ' % (self.switches2[-1], self.switches2[0])
        else:
            path = nx.shortest_path (g, self.switches2[-1], self.switches2[0])
            strInfo = '  -> shortest path between %d and %d is: ' % (self.switches2[-1], self.switches2[0])
        strInfo += str(path)
        log.info(strInfo)

        if not self.wifi:
            msg = of.ofp_flow_mod()
            #msg.match.dl_type = 0x0880
            msg.command = of.OFPFC_MODIFY_STRICT      # of.OFPFC_MODIFY || of.OFPFC_ADD
            msg.match.dl_dst = self.title_hash[:6]
            msg.match.dl_src = self.title_hash[6:]
            if path.__len__() > 1:
                if self.bw and self.cos:
                    log.info("  -> enqueuing in queue , corresponding to CoS '%s'" % self.cos)
                for i in path:
                    if i == path[-1]:
                        self.switches[i].used_ports.add(g[i][path[path.index(i)-1]]['ports'][i])
                    elif i == path[0]:
                        self.switches[i].used_ports.add(g[i][path[path.index(i)+1]]['ports'][i])
                    else:
                        try:
                            s = self.switches[i]
                        except KeyError:
                            s = self.WPSwitch(i)
                            self.switches[i] = s
                        
                        self.switches[i].used_ports.add(g[i][path[path.index(i)+1]]['ports'][i])
                        self.switches[i].used_ports.add(g[i][path[path.index(i)-1]]['ports'][i])
                    if self.bw and self.cos:
                        msg.actions = [of.ofp_action_enqueue(port=p) for p in self.switches[i].used_ports] # ofp_action_enqueue(port=0, queue_id=0)
                    else:
                        print 'action_ouput'
                        msg.actions = [of.ofp_action_output(port=p)  for p in self.switches[i].used_ports]
                
                    core.openflow.sendToDPID(i, msg)
            else:
                msg.actions = [of.ofp_action_output(port=p) for p in s.used_ports]
                core.openflow.sendToDPID(s.dpid, msg)
        else: # ============================================================================================================================== wifi
            rule1 = of.ofp_flow_mod()
            rule1.match.dl_dst = self.title_hash[:6]
            #rule1.match.dl_type = 0x0880
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
                        self.switches[i].used_ports.add(g[i][path[path.index(i)-1]]['ports'][i])
                    elif i == path[0]:
                        self.switches[i].used_ports.add(g[i][path[path.index(i)+1]]['ports'][i])
                        for p in self.switches[i].used_ports:
                            rule1.actions.append(of.ofp_action_output(port=p))
                        core.openflow.sendToDPID(i, rule1)
                    else:
                        try:
                            s = self.switches[i]
                        except KeyError:
                            s = self.WPSwitch(i)
                            self.switches[i] = s
                        self.switches[i].used_ports.add(g[i][path[path.index(i)+1]]['ports'][i])
                        self.switches[i].used_ports.add(g[i][path[path.index(i)-1]]['ports'][i])
                    if self.bw and self.cos:
                        # print 'enqueue'
                        rule2.actions = [of.ofp_action_enqueue(port=p) for p in self.switches[i].used_ports] # ofp_action_enqueue(port=0, queue_id=0)
                    else:
                        rule2.actions = [of.ofp.action_output(port=p)  for p in self.switches[i].used_ports]
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
        #msg.match.dl_type = 0x0880

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

    def __init__(self, wifi=False):
        self.wifi = wifi
        self.entities = {}
        self.workspaces = {}
        def startup ():
            core.openflow.addListeners(self, priority=0)
            core.openflow_discovery.addListeners(self)
        core.call_when_ready(startup, ('openflow','openflow_discovery'))

        # TODO: insert flow that will redirect to here DTS addressed packages

    @staticmethod
    def reply_request(success, request_id, event, wifi=None, addrResp=None):
        # Build the response
        resp_msg = dts_pb2.ControlResponse()
        resp_msg.status = resp_msg.SUCCESS if success else resp_msg.FAILURE
        if request_id != None:
            resp_msg.request_id = request_id
        resp_msg = resp_msg.SerializeToString()

        # Send response back to the entity
        resp = of.ofp_packet_out()
        if not wifi:
            resp.data = ''.join((DTSA.HEADER, struct.pack("<H", len(resp_msg)), resp_msg))
        else:
            resp.data = ''.join((addrResp, struct.pack("<H", len(resp_msg)), resp_msg))
        resp.actions.append(of.ofp_action_output(port = event.port))
        event.connection.send(resp)

    def on_entity_register(self, msg, request_id, event, addrResp):
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

        self.reply_request(success, request_id, event, self.wifi, addrResp)

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

    def on_workspace_create(self, msg, request_id, event, addrResp=None):
        if ((msg.workspace_title in self.workspaces)
            or (msg.attach_too
                and (not msg.HasField('entity_title')
                     or msg.entity_title not in self.entities))):
            success = False
        else:
            qos = None; bw = None; cos=None
            for i in range (0, msg.capabilities.__len__()):
                type = msg.capabilities[i].type
                value = msg.capabilities[i].value
                if type == 1:       # qos_required_bool
                    qos = True
                if type == 2:       # bw_required_int32
                    bw  = value
                if type == 3:       # cos_required_string
                    cos = value

            w = Workspace(msg.workspace_title, self.wifi, addrResp, qos, bw, cos)
            self.workspaces[msg.workspace_title] = w

            if not qos:
                log.info('Registered workspace "{}".'.format(msg.workspace_title))
            else:
                log.info('Registered SMART QoS workspace "{}".'.format(msg.workspace_title))

            if msg.attach_too:
                w.attach(self.entities[msg.entity_title], event.parsed)
                log.info('  -> attached entity "{}".'.format(msg.entity_title))

            success = True

        self.reply_request(success, request_id, event, self.wifi, addrResp)

    def on_workspace_attach(self, msg, request_id, event, *rest):
        success = False
        if (msg.workspace_title in self.workspaces
            and msg.entity_title in self.entities):

            w = self.workspaces[msg.workspace_title]
            if msg.entity_title not in w.entities:
                w.attach(self.entities[msg.entity_title], event.parsed)
                log.info('Attached entity "{}" to workspace "{}".'.format(msg.entity_title, msg.workspace_title))
                success = True

        self.reply_request(success, request_id, event, self.wifi, *rest)

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
            addrResp = event.data[:12][6:] + event.data[:12][:6] + '\x08\x80'
        else:
            addrResp = None
        buff = cleanBuffer(event.data[14:])
        request = dts_pb2.ControlRequest()
        buff_iter = buffer_splitter(buff)
        while True:
            try:
                request.ParseFromString(buff_iter.next())
            except StopIteration:
                break

            try:
                msg_buff = buff_iter.next()
            except StopIteration:
                log.info('Problem detected ... activating detonation in 3 .. 2 ..')
                pass

            try:
                (handler, msg_class) = self.MSG_HANDLER[request.type]
                msg = msg_class()
                msg.MergeFromString(msg_buff)
                handler(self, msg, request.id, event, addrResp)
            except KeyError:
                # TODO: deal with unknown message type
                pass

    def _handle_LinkEvent (self, event):
        for l in core.openflow_discovery.adjacency:
            prts = {l.dpid1: l.port1, l.dpid2: l.port2}
            cost = 0; link = ''
            if ports[l.dpid1][l.port1] & of.OFPPF_10MB_HD:
                cost += 10; link += '10MB_HD'
            if ports[l.dpid1][l.port1] & of.OFPPF_10MB_FD:
                cost += 10; link += '10MB_FD' 
            if ports[l.dpid1][l.port1] & of.OFPPF_100MB_HD:
                cost += 8; link += '100MB_HD'
            if ports[l.dpid1][l.port1] & of.OFPPF_100MB_FD:
                cost += 8; link += '100MB_FD'
            if ports[l.dpid1][l.port1] & of.OFPPF_1GB_HD:
                cost += 5; link += '1GB_HD'
            if ports[l.dpid1][l.port1] & of.OFPPF_1GB_FD:
                cost += 5; link += '1GB_FD'
            if ports[l.dpid1][l.port1] & of.OFPPF_10GB_FD:
                cost += 2; link += '10GB_FD'
            if ports[l.dpid1][l.port1] & of.OFPPF_COPPER:
                cost += 5; link += ' COPPER'
            if ports[l.dpid1][l.port1] & of.OFPPF_FIBER:
                cost += 0; link += ' FIBER'
            if ports[l.dpid1][l.port1] & of.OFPPF_AUTONEG:
                cost += 999; link += ' AUTONEG'
            if ports[l.dpid1][l.port1] & of.OFPPF_PAUSE:
                cost += 999; link += ' PAUSE'
            if ports[l.dpid1][l.port1] & of.OFPPF_PAUSE_ASYM:
                cost += 999; link += ' PAUSE_ASYM'
            g.add_edge(l.dpid1, l.dpid2, cost=cost, ports=prts, link=link)
            
    def _handle_ConnectionUp (self, event):
        log.debug("Switch %s has come up.", dpid_to_str(event.dpid))
        ports[event.dpid] = {}
        for i in event.ofp.ports:
            ports[event.dpid][i.port_no] = i.curr
        # AJUSTE MANUAL ------> BORRAR
        if dpid_to_str(event.dpid) == '00-00-00-00-00-02':
        	ports[event.dpid][2] = 130
        if dpid_to_str(event.dpid) == '00-00-00-00-00-03':
        	ports[event.dpid][3] = 130

def addQueues():
    #print ports
    log.info('Starting queue configuration...')
    msg = ofq2.ofp_qos_msg()
    msg.ofp_header = ofq2.ofp_header()
    msg.sched_type = ofq2.ofp_qos_type_rev_map['OFP_QOS_SCHED_HTB']
    msg.body = ofq2.ofp_qos_sqf()
    msg.body.ofp_qos_header = ofq2.ofp_qos_header()
    msg.body.ofp_qos_header.object_type = ofq2.ofp_qos_object_type_rev_map['OFP_QOS_CLASS']
    msg.body.ofp_qos_header.action_type = ofq2.ofp_qos_action_type_rev_map['OFP_QOS_ACTION_ADD']
    msg.body.port_no = 1
    msg.body.class_id = 0
    msg.body.perturb = 5
    msg.body.quantum = 100
    packed = b""
    packed += msg.ofp_header.pack()
    packed += struct.pack('!BpppBBHIII', msg.sched_type, '0', '0', '0',
                          msg.body.ofp_qos_header.object_type, msg.body.ofp_qos_header.action_type,
                          msg.body.port_no, msg.body.class_id, msg.body.perturb, msg.body.quantum)

    #core.openflow.sendToDPID(str_to_dpid('1'), msg)
    log.info('Queues inicializated')

    #cabezallo = of.ofp_header()
    #cabezallo.show()
    #cabezallo2 = cabezallo.pack()
    #print 'cabezallo:', bin(cabezallo2), str(cabezallo2)
    
def drawGraph():
    time.sleep(15)
    log.info("Network's topology graph:")
    nx.draw_spectral(g)
    nx.draw_networkx_edge_labels(g,pos=nx.spectral_layout(g))
    #nx.draw_circular(g)
    #nx.draw_networkx_edge_labels(g,pos=nx.circular_layout(g))
    #nx.draw_shell(g)
    #nx.draw_networkx_edge_labels(g,pos=nx.shell_layout(g))
    plt.show()

def launch(help=False, graph=False, wifi=False):
    if help:
        print """usage: pox.py openflow.discovery samples.pretty_log dtsa_smart [--help] [--graph] [--wifi]
               \nPOX DTSA-SMART controller with QoS support
               \noptional arguments:\
               \n  --help            show this help message and exit\
               \n  --graph           show network's topology graph\
               \n  --wifi            run controller in Wifi compatible mode"""
        sys.exit(1)
    core.registerNew(DTSA, wifi)
    core.callDelayed(13, addQueues)
    if graph:
        hilo = threading.Thread(target=drawGraph)
        hilo.start()
