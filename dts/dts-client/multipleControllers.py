#!/usr/bin/python

"""

This example creates a multi-controller network from semi-scratch by
using the net.add*() API and manually starting the switches and controllers.

This is the "mid-level" API, which is an alternative to the "high-level"
Topo() API which supports parametrized topology classes.

Note that one could also create a custom switch class and pass it into
the Mininet() constructor.
"""

from mininet.net import Mininet
from mininet.node import Controller, OVSSwitch
from mininet.cli import CLI
from mininet.log import setLogLevel
from mininet.node import RemoteController

def multiControllerNet():
    "Create a network from semi-scratch with multiple controllers."

    net = Mininet(controller=RemoteController, switch=OVSSwitch, build=False)

    print "*** Creating (reference) controllers"
    c1 = net.addController('c1', ip='192.168.1.106', port=6633)
    c2 = net.addController('c2', ip='127.0.0.1', port=6633)

    print "*** Creating switches"
    s1 = net.addSwitch('s1')
    s2 = net.addSwitch('s2')

    print "*** Creating hosts"
    hosts1 = [net.addHost('h%d' % n) for n in 3, 4]
    hosts2 = [net.addHost('h%d' % n) for n in 5, 6]

    print "*** Creating links"
    for h in hosts1:
        net.addLink(s1, h)
    for h in hosts2:
        net.addLink(s2, h)
    net.addLink(s1, s2, 5, 5)

    print "*** Starting network"
    net.build()
    c1.start()
    c2.start()
    s1.start([c1])
    s2.start([c2])

    #print "*** Testing network"
    #net.pingAll()

    print "*** Running CLI"
    CLI(net)

    print "*** Stopping network"
    net.stop()

if __name__ == '__main__':
    setLogLevel( 'info' )  # for CLI output
    multiControllerNet()
