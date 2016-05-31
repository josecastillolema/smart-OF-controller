#!/usr/bin/python

import re
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.link import TCLink
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel
from mininet.node import RemoteController
from mininet.cli import CLI

class MyTopo (Topo):
    def __init__(self, n=2, **opts):
        Topo.__init__(self, **opts)
	# Add hosts and switches
	h1 = self.addHost('h1')
	h2 = self.addHost('h2')
	s1 = self.addSwitch('s1')
	s2 = self.addSwitch('s2')

	# links Add
	self.addLink(s1, h1)
	self.addLink(s2, h2)
	self.addLink(s1, s2, bw=10, delay='700ms')
	#self.addLink(s1, s3, 3, 2)
	#self.addLink(s2, s3, 2, 3, bw=10, delay='200ms', jitter='2ms', loss=10, max_queue_size=1000, use_htb=True)

def inicializaRed():
    "Create network and run simple performance test"
    topo = MyTopo()
    net = Mininet(topo=topo, link=TCLink)
    net.start()
    CLI(net)
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    inicializaRed()
