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
	s1 = self.addSwitch('s1', dpid=toID('00:00:00:00:00:01'))

	# links Add
	self.addLink(s1, h1, 1, 1, intfName1 = 'eth0-h1')
	self.addLink(s1, h2, 2, 1, intfName1 = 'eth0-h2')

def inicializaRed():
    "Create network and run simple performance test"
    topo = MyTopo()
    net = Mininet(topo=topo, link=TCLink, controller=RemoteController)
    net.start()
    
    print "*** Creating queues ..."
    s1 = net.getNodeByName('s1')
    for j in s1.intfNames()[1:]:
        cmd3 = 'ovs-vsctl -- set Port ' + j + ' qos=@newqos\
                -- --id=@newqos create QoS type=linux-htb  other-config:max-rate=3000000 queues=0=@q0,1=@q1\
                -- --id=@q0 create Queue other-config:min-rate=2300000 other-config:max-rate=2800000\
                -- --id=@q1 create Queue other-config:min-rate=80000   other-config:max-rate=1500000'
        s1.cmd(cmd3)
    CLI(net)
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    inicializaRed()
