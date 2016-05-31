#!/usr/bin/python

"""
     s1
     |1\ 2
     |  \
     h1  h2
""" 

import re
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.link import TCLink
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel
from mininet.node import RemoteController
from mininet.cli import CLI

def toID(mac):
    return '0000' + re.sub('[:]', '', mac)

class MyTopo (Topo):
    def __init__(self, n=2, **opts):
        Topo.__init__(self, **opts)
	# Add hosts and switches
	h1 = self.addHost('h1')
	h2 = self.addHost('h2')
	I1 = self.addSwitch('I1', dpid=toID('00:00:00:00:00:01'))
	s2 = self.addSwitch('s2', dpid=toID('00:00:00:00:00:02'))
	s3 = self.addSwitch('s3', dpid=toID('00:00:00:00:00:03'))
	s4 = self.addSwitch('s4', dpid=toID('00:00:00:00:00:04'))
	s5 = self.addSwitch('s5', dpid=toID('00:00:00:00:00:05'))
	s6 = self.addSwitch('s6', dpid=toID('00:00:00:00:00:06'))
	s7 = self.addSwitch('s7', dpid=toID('00:00:00:00:00:07'))
	s8 = self.addSwitch('s8', dpid=toID('00:00:00:00:00:08'))
	E2 = self.addSwitch('E2', dpid=toID('00:00:00:00:00:09'))

	# links Add
	self.addLink(I1, h1)#, 2, 1, intfName1 = 'eth0-h1')
	self.addLink(E2, h2)#, 2, 1, intfName1 = 'eth0-h2')
	self.addLink(I1, s2)
	self.addLink(s2, s3)
	self.addLink(s3, s4)
	self.addLink(s4, s5)
	self.addLink(s5, s6)
	self.addLink(s6, s7)
	self.addLink(s7, s8)
	self.addLink(s8, E2)

def inicializaRed():
    "Create network and run simple performance test"
    topo = MyTopo()
    net = Mininet(topo=topo, link=TCLink, controller=RemoteController)
    net.start()
    
    print "*** Creating queues ..."
    switches = net.getNodeByName('I1', 's2', 's3', 's4', 's5', 's6', 's7', 's8', 'E2')
    for i in switches:
        for j in i.intfNames()[1:]:
            cmd3 = 'ovs-vsctl -- set Port ' + j + ' qos=@newqos\
                    -- --id=@newqos create QoS type=linux-htb  other-config:max-rate=3000000 queues=0=@q0,1=@q1\
                    -- --id=@q0 create Queue other-config:min-rate=2300000 other-config:max-rate=2800000\
                    -- --id=@q1 create Queue other-config:min-rate=80000   other-config:max-rate=1500000'
            i.cmd(cmd3)

    CLI(net)
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    inicializaRed()
