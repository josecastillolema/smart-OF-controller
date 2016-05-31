#!/usr/bin/python
# packetget.py xx yy iface numberFlows
# packetgen.py 20 25 h403-eth0 3

import socket
import time
import random
import threading
import code
import array
import timeit
import cProfile
import readline
import rlcompleter
import _lsprof
import sys
import signal
from gevent import monkey
monkey.patch_socket()
import dts

from impacket import ImpactDecoder, ImpactPacket

AS_ROOT = True

def _val_from_cdf(cdf):
    '''
    input is a cdf dict mapping prob->value
    output is value
    '''
    
    k = cdf.keys()
    k.sort()
    
    assert(k[-1] == 1.0)
    
    r = random.random()
    for p in k:
        if r < p:
            if callable(cdf[p]):
                return cdf[p]()
            else:
                return cdf[p]

    raise Exception("This should never be reached")
        
class Flow(object):
    '''
    Represents a single active flow
    '''
    
    def __init__(self, pkt_size, max_pkts, eth, ip, udp):
        
        self.eth = eth.get_packet()     
        self.max_pkts = max_pkts
        self.num_sent = 0
        
class FlowGenerator(threading.Thread):
    def __init__(self, 
                 max_rate = 2500,
                 num_flows = 10000,
                 pkt_size = 100,
                 flow_size_cdf = {0.999: 10,
                                  0.9999: 100,
                                  0.99999: 1000,
                                  1.0: 100000,
                                  },
                 num_src_machines = 100,
                 num_dst_machines = 100,
                 iface = "eth0",
                 batch_size = 1,
                 entity = 'e',
                 workspace = 'w'
                 ):
        '''
        Initialize a packet generator instance with the specified
        parameters:
        
        - max_rate: maximum bitrate to gen packets in kbps
        - num_flows: total num of active flows at any given time
        - pkt_size: packet size
        - dst_ports_pdf: a dict mapping cdf -> port num
                         "random" can be used instead of port num.
        - flow_size_pdf: a dict mapping cdf -> flow sizes in packets
        - num_src/dst_machines: total number of machines this flow generator 
                        will simulate
        - src/dst_ip_prefix: subnet the machines will use as source and dst addresses
                     format: "x.x.x.x/int"
        - src/dst_mac_prefix: specify the macs for the machines. format:"xx:xx:xx:xx:xx:xx/int"
        - src/dst_mac_list: If specified, this will override the prefix
        - iface: name of interface to send packets on
        '''
        
        self.batch_size = batch_size
        
        self.max_flows = num_flows
        self.flow_size_cdf = flow_size_cdf
        self.pkt_size = pkt_size
        self.cdf_counter = 0
        
        self.workspace = workspace
        self.entity = entity
        
        # Calculate the delay between packets to maintain bitrate
        self.delay = self.batch_size * pkt_size * 8.0 / max_rate / 1000
        
        #print "Delay: %s ms" % (self.delay*1000)
        
        # maintains the active flow set
        self.flows = []
        
        # what flow should a packet come from?
        self.next_flow = 0
        
        # socket to use
        if AS_ROOT:
            e = dts.Entity(iface, entity, True)
            print 'Entity "{}" registered.'.format(e.title)
        
            w = dts.Workspace(iface, workspace)
        
            try:
                w.attach(e)
                print 'Attached to workspace "{}".'.format(w.title)
            except dts.DTSException:
                # Failed to attach, probably does not exists,
                # then try to create
                w.create_on_dts(e)
                print 'Created workspace "{}" and attached to it.'.format(w.title)
            
        super(FlowGenerator, self).__init__()
        
        self.total_sent = 0
        
        # cache the creation of a packet
        self.eth = ImpactPacket.Ethernet()
        self.ip = ImpactPacket.IP()
        self.udp = ImpactPacket.UDP()
        
        data_len = (pkt_size
                    - self.eth.get_header_size()
                    - self.ip.get_header_size()
                    - self.udp.get_header_size())
        data = ImpactPacket.Data()
        data.set_bytes(array.array('B', [ i % 0xff for i in range(data_len) ]))

        self.udp.contains(data)
        self.ip.contains(self.udp)
        self.eth.contains(self.ip)

        self.calibrated = False
        
        
    def __send_pkt(self, idx):
        '''
        generate and send a packet from a flow at
        a particular index
        '''
        
        flow = self.flows[idx]
        if AS_ROOT: w.send(flow.eth)
        flow.num_sent += 1
        if flow.max_pkts == flow.num_sent:
            self.flows.pop(idx)
        
        self.total_sent += 1
        
    def hack_val_from_cdf(self, cdf):
        k = cdf.keys()
        index = self.cdf_counter
        self.cdf_counter = (self.cdf_counter+1)%len(k)
        return cdf[k[index]]
    
    def send_pkt(self):
        '''
        Send a packet from a flow. If the maximum number of active flows has
        been reached, this function selects one of the flows and sends a packet.
        Otherwise, a new flow is started. If the flow has reached its end, the flow
        is removed. This function cycles through the active flows in round-robin.
        '''
        
        i = 0
        while i < self.batch_size:
            i += 1
            # if we have not reached the max num of flows
            if len(self.flows) < self.max_flows:
                # create a new flow.
                max_pkts = _val_from_cdf(self.flow_size_cdf)
                
                self.flows.append(Flow(self.pkt_size, max_pkts, self.eth, self.ip, self.udp))
                try:
                    w.send(-1)
                except:
                    sys.exit()
                    
            # otherwise send the next pkt of a flow
            else:
                try:
                    w.send(self.next_flow)
                except:
                    sys.exit()
                self.next_flow += 1
                if self.next_flow == self.max_flows: self.next_flow = 0
                
    def hist_by_flow_size(self):
        '''
        Return the number of flows for each flow size
        '''
        
        hist = {}
        for flow in self.flows:
            if flow.max_pkts in hist:
                hist[flow.max_pkts] += 1
            else:
                hist[flow.max_pkts] = 1
        
        return hist
    
    def hist_by_dst_port(self):
        '''
        Return the number of flows for each flow size
        '''
        
        hist = {}
        for flow in self.flows:
            if flow.tp_dst in hist:
                hist[flow.tp_dst] += 1
            else:
                hist[flow.tp_dst] = 1
        
        return hist
    
    def run(self):
        self.run_more = True
        offset = 0
        while(self.run_more):
            p = cProfile.Profile()
            p.runcall(FlowGenerator.send_pkt, self)
            s = p.getstats()
            # get the entry corresponding to the call
            l = [e for e in s if hasattr(e, 'code') 
                 and hasattr(e.code, 'co_name')
                 and e.code.co_name == 'send_pkt']
            #code.interact(local=locals())
            offset = l[0].totaltime
            #print offset
            stime = self.delay - offset
            if self.delay - offset < 0: stime = 0
            time.sleep(stime)
            
    def stop(self):
        self.run_more = False
    
if __name__ == "__main__":
    readline.set_completer(rlcompleter.Completer(globals()).complete)
    readline.parse_and_bind("tab: complete")

    # useful things:
   
    mixed_size = {0.999: 10,
                  0.9999: 100,
                  0.99999: 1000,
                  1.0: 100000,
                  }
    
    single_small_size = {1.0: 1}
    single_med_size = {1.0: 10}
    single_long_size = {1.0: 100000000}
    
    # define defaults
    params = {'max_rate' : 20,
              'num_flows' : 3,
              'pkt_size' : 100,
              'flow_size_cdf' : single_long_size,

              'iface' : "eth2",
              'batch_size': 1,
              }

    flowgen = None
    
    def make_flowgen():
        global flowgen
        if flowgen: flowgen.stop()
        flowgen = FlowGenerator(**params)
        
    def bw():
        global flowgen
        num_start = flowgen.total_sent
        time.sleep(5)
        print "Bandwidth: %s kbps" % ((flowgen.total_sent - num_start)*100*8/5.0/1000)
    
    def profile():
        global flowgen
        cProfile.run('flowgen.send_pkt()')    
        
    def run(entity, workspace, iface, nflows=3):
        global flowgen
        global params
        #if machine == 0x10 or machine == 0x11:
        params['iface'] = iface 
        params['num_flows'] = nflows
        params['entity'] = entity
        params['workspace'] = workspace
        print params
        make_flowgen()
        flowgen.start()
        
    #time.sleep(3)
    #bw()
    #flowgen.stop()

    if len(sys.argv) == 5:
        print "Running with entity", sys.argv[1], "workspace", sys.argv[2], "iface", sys.argv[3], "nflows", sys.argv[4]
        run(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]))     # entidade, workspace, iface, nflows
        flowgen.join()
    elif len(sys.argv) == 7:
        print "Running with idx %i" % int(sys.argv[1])
        user_mix  = { float(sys.argv[4]): 80,
                      float(sys.argv[5]): 5060,
                      float(sys.argv[6]): 1234
                      }
        
        run(int(sys.argv[1], 16), int(sys.argv[2], 16), sys.argv[3], user_mix )
        flowgen.join()
    else:
        help = '''
            Welcome to the flow generator.
            "params" contains the parameters that will be passed to
            the flow generator.
            When you are done modifying params, use "make_flowgen()" to create
            the flow generator object. For more info on the parameters,
            type "print FlowGenerator.__init__.__doc__"
            To start packet generation, use "flowgen.start()". To stop,
            use "flowgen.stop()"
            use "bw()" to measure the current throughput.
            use "profile()" to profile packet sending (Make sure you have stopped
            flowgen first).
            use "flowgen.hist_by_flow_size()" and flowgen.hist_by_dst_port()" to
            get a historgram of flows.
            '''
        code.interact(help, local=locals())
    
    if flowgen:
        flowgen.stop()
