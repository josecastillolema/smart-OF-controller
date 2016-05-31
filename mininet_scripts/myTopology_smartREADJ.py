#!/usr/bin/python

# Author: Jose Castillo Lema <josecastillolema@gmail.com>
"""
         3              2     1
     I1 ------------------ E3----h4
  4/ |1\ 2               3/  \ 4
  /  |  \            10mb/    \
 h7  h1  h2            2/     5\   1
                      s2-3---3-E4--------h5
                        |      | \
                        |1    4|  \ 2
                       h3     h6   h8
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
	h3 = self.addHost('h3')
	h4 = self.addHost('h4')
	h5 = self.addHost('h5')
	h6 = self.addHost('h6')
	I1 = self.addSwitch('I1', dpid=toID('00:00:00:00:00:01'))
	I2 = self.addSwitch('I2', dpid=toID('00:00:00:00:00:02'))
	I3 = self.addSwitch('I3', dpid=toID('00:00:00:00:00:03'))
	C1 = self.addSwitch('C1', dpid=toID('00:00:00:00:00:04'))
	C2 = self.addSwitch('C2', dpid=toID('00:00:00:00:00:05'))
	C3 = self.addSwitch('C3', dpid=toID('00:00:00:00:00:06'))
	C4 = self.addSwitch('C4', dpid=toID('00:00:00:00:00:07'))
	C5 = self.addSwitch('C5', dpid=toID('00:00:00:00:00:08'))
	C6 = self.addSwitch('C6', dpid=toID('00:00:00:00:00:09'))
	C7 = self.addSwitch('C7', dpid=toID('00:00:00:00:00:0A'))
	E1 = self.addSwitch('E1', dpid=toID('00:00:00:00:00:0B'))
	E2 = self.addSwitch('E2', dpid=toID('00:00:00:00:00:0C'))
	E3 = self.addSwitch('E3', dpid=toID('00:00:00:00:00:0D'))

	# links Add
	self.addLink(I1, h1, 1, 1, intfName1 = 'eth0-h1')
	self.addLink(I2, h2, 1, 1, intfName1 = 'eth0-h2')
	self.addLink(I3, h3, 1, 1, intfName1 = 'eth0-h7')
	self.addLink(E1, h4, 1, 1, intfName1 = 'eth0-h3')
	self.addLink(E2, h5, 1, 1, intfName1 = 'eth0-h4')
	self.addLink(E3, h6, 1, 1, intfName1 = 'eth0-h5')

	self.addLink(I1, C1,  2,  2, bw=10,use_htb=True)
	self.addLink(C1, C5,  3,  3, bw=10,use_htb=True)
	self.addLink(C5, E1,  4,  4, bw=10,use_htb=True)
	self.addLink(I2, C2,  5,  5, bw=10,use_htb=True)
	self.addLink(C2, C4,  6,  6, bw=10,use_htb=True)
	self.addLink(C4, C5,  7,  7, bw=10,use_htb=True)
	self.addLink(C4, C6,  8,  8, bw=10,use_htb=True)
	self.addLink(C6, E1,  9,  9, bw=10,use_htb=True)
	self.addLink(C6, E2, 10, 10, bw=10,use_htb=True)
	self.addLink(I3, C3, 11, 11, bw=10,use_htb=True)
	self.addLink(C3, C4, 12, 12, bw=10,use_htb=True)
	self.addLink(C3, C7, 13, 13, bw=10,use_htb=True)
	self.addLink(C7, E3, 14, 14, bw=10,use_htb=True)

def inicializaRed():
    "Create network and run simple performance test"
    topo = MyTopo()
    net = Mininet(topo=topo, link=TCLink, controller=RemoteController)
    net.start()
    CLI(net)
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    inicializaRed()
