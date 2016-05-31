#!/usr/bin/python

"""
         3              2     1
     I1 ------------------ s3----h4
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
	h7 = self.addHost('h7')
	h8 = self.addHost('h8')
	s1 = self.addSwitch('I1', dpid=toID('00:00:00:00:00:01'))
	s2 = self.addSwitch('s2', dpid=toID('00:00:00:00:00:02'))
	s3 = self.addSwitch('s3', dpid=toID('00:00:00:00:00:03'))
	s4 = self.addSwitch('E4', dpid=toID('00:00:00:00:00:04'))

	# links Add
	self.addLink(s1, h1, 1, 1, intfName1 = 'eth0-h1')
	self.addLink(s1, h2, 2, 1, intfName1 = 'eth0-h2')
	self.addLink(s1, h7, 4, 1, intfName1 = 'eth0-h7')
	self.addLink(s2, h3, 1, 1, intfName1 = 'eth0-h3')
	self.addLink(s3, h4, 1, 1, intfName1 = 'eth0-h4')
	self.addLink(s4, h5, 1, 1, intfName1 = 'eth0-h5')
	self.addLink(s4, h8, 2, 1, intfName1 = 'eth0-h8')
	self.addLink(s4, h6, 4, 1, intfName1 = 'eth0-h6')
	self.addLink(s1, s3, 3, 2, bw=1,use_htb=True)
	self.addLink(s2, s3, 2, 3, bw=0.3,use_htb=True, delay='700ms', jitter='10ms', loss=10)
	self.addLink(s3, s4, 4, 5, bw=1,use_htb=True)
	self.addLink(s2, s4, 3, 3, bw=1,use_htb=True)

def inicializaRed():
    "Create network and run simple performance test"
    topo = MyTopo()
    net = Mininet(topo=topo, link=TCLink, controller=RemoteController)
    net.start()
    print "*** Creating queues ..."
    switches = net.getNodeByName('I1', 's2', 's3', 'E4')
    for i in switches:
        for j in i.intfNames()[1:]:
            if j in ['s2-eth2', 's3-eth3']:
                continue
            #if j in ['s1-eth3', 's3-eth2', 's3-eth4', 's4-eth5', 's2-eth3', 's4-eth3']:
            cmd  = 'ovs-vsctl -- set port ' + j + ' qos=@newqos -- --id=@newqos create qos type=linux-htb other-config:max-rate=1500000\
                    queues:0=@newqueue -- --id=@newqueue create queue other-config:min-rate=1000 other-config:max-rate=1500000'
            cmd2 = 'ovs-vsctl -- set port ' + j + ' qos=@newqos -- --id=@newqos create qos type=linux-htb other-config:max-rate=1500000\
                    queues:1=@newqueue -- --id=@newqueue create queue other-config:min-rate=1500000 other-config:max-rate=1500000'
            cmd3 = 'ovs-vsctl -- set Port ' + j + ' qos=@newqos\
                    -- --id=@newqos create QoS type=linux-htb  other-config:max-rate=3000000 queues=0=@q0,1=@q1\
                    -- --id=@q0 create Queue other-config:min-rate=2300000 other-config:max-rate=2800000\
                    -- --id=@q1 create Queue other-config:min-rate=80000   other-config:max-rate=1500000'
            #cmd4 = 'tc class change dev ' + j + ' parent 1:fffe classid 1:1 htb rate 1kbit ceil 1500kbit burst 1563b cburst 1563b'
            #i.cmd(cmd)                   # Usar i.cmdPrint(cmd) para imprimir el resultado
            #i.cmd(cmd2)
            i.cmd(cmd3)
            #i.cmdPrint(cmd4)
    #for i in switches:
        #for j in i.intfNames()[1:]:
            #cmd4 = 'tc class change dev ' + j + ' parent 1:fffe classid 1:1 htb rate 1kbit ceil 770kbit burst 1563b cburst 1563b'
            #i.cmd(cmd4)
            #for k in [1, 2]:
            #   cmd5 = 'tc qdisc add dev ' + j +' parent 1:' + str(k) + ' handle ' + str(k) + '0: sfq perturb 10'
            #   i.cmd(cmd5)
    CLI(net)
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    inicializaRed()
