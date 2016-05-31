#!/usr/bin/python

"""
Federal University of Para (UFPA) - Brazil - 2012
Research Group on Computer Networks and Multimedia Communication (GERCOM)
Author: Airton Ishimori

This script creates a simple QoSFlow network.

Dependency: The user-space datapath must be qosflow-datapath. So, it must be installed.
"""

from mininet.net import Mininet
from mininet.node import Node, Switch, UserSwitch
from mininet.link import Link
from mininet.link import Intf
from mininet.cli import CLI
from mininet.log import setLogLevel, info
from mininet.node import RemoteController, Controller, OVSKernelSwitch
from mininet.moduledeps import moduleDeps, pathCheck, OVS_KMOD, OF_KMOD, TUN
from mininet.log import info, error, warn, debug

import os

class QoSFlowController( Controller ):
    "QoSFlow Controller running outside of Mininet's control."

    def __init__( self, name, ip='192.168.1.1',
                  port=6633, **kwargs):
        """Init.
           name: name to give controller
           ip: the IP address where the remote controller is
           listening
           port: the port where the remote controller is listening"""
        Controller.__init__( self, name, ip=ip, port=port, **kwargs )

    def start( self ):
        "Overridden to do nothing."
        return

    def stop( self ):
        "Overridden to do nothing."
        return

    def checkListening( self ):
        "Warn if remote controller is not accessible"
        listening = self.cmd( "echo A | telnet -e A %s %d" %
                              ( self.ip, self.port ) )
        if 'Unable' in listening:
            warn( "Unable to contact the remote controller"
                  " at %s:%d\n" % ( self.ip, self.port ) )

   
class QoSFlowUserSwitch(Switch):
    "QoSFlow User-space switch."

    dpidLen = 12
    
    def __init__( self, name, bw=100000000, **kwargs ): #100mbps
        """Init.
           name: name for the switch
           bw: interface maximum bandwidth"""
        
        Switch.__init__( self, name, **kwargs )
        self.bandwidth = bw
        pathCheck( 'ofdatapath', 'ofprotocol', moduleName='QoSFlow 0.1 datapath')
        if self.listenPort: # dpctl
            self.opts += ' --listen=ptcp:%i ' % self.listenPort

    @classmethod
    def setup( cls ):
        "Ensure any dependencies are loaded; if not, try to load them."
        if not os.path.exists( '/dev/net/tun' ):
            moduleDeps( add=TUN )

    def dpctl( self, *args ):
        "Run dpctl command"
        if not self.listenPort:
            return "can't run dpctl without passive listening port"
        return self.cmd( 'dpctl ' + ' '.join( args ) +
                         ' tcp:127.0.0.1:%i' % self.listenPort )

    def start( self, controllers ):
        """Start OpenFlow reference user datapath.
           Log to /tmp/sN-{ofd,ofp}.log.
           controllers: list of controller objects"""
           
        if not self.bandwidth:
            info('Unable to start. You must set interface maximum bandwidth (in bits/s format)!\n')
            return
        
        # Add controllers
        clist = ','.join( [ 'tcp:%s:%d' % ( c.IP(), c.port )
                            for c in controllers ] )
        ofdlog = '/tmp/' + self.name + '-ofd.log'
        ofplog = '/tmp/' + self.name + '-ofp.log'
        self.cmd( 'ifconfig lo up' )
        intfs = [ str( i ) for i in self.intfList() if not i.IP() ]
        self.cmd( 'ofdatapath -i ' + ','.join( intfs ) +
                  ' punix:/var/run/' + self.name + ' -d ' + self.dpid + ' -b ' + str(self.bandwidth) + 
                  ' 1> ' + ofdlog + ' 2> ' + ofdlog + ' &' )
        self.cmd( 'ofprotocol unix:/var/run/' + self.name +
                  ' ' + clist +
                  ' --fail=closed ' + self.opts +
                  ' 1> ' + ofplog + ' 2>' + ofplog + ' &' )

    def stop( self ):
        "Stop OpenFlow reference user datapath."
        self.cmd( 'kill %ofdatapath' )
        self.cmd( 'kill %ofprotocol' )
        self.deleteIntfs()

#### NETWORK ####

def QoSFlowNet():
    "Create network by using QoSFlow user switch."
    
    net = Mininet(controller=QoSFlowController, switch=QoSFlowUserSwitch) 
    
    info('*** Adding controller\n')
    net.addController('c0')
    
    info('*** Adding hosts\n')
    h1 = net.addHost('h1', ip='10.0.0.1')
    h2 = net.addHost('h2', ip='10.0.0.2')
    h3 = net.addHost('h3', ip='10.0.0.3')
    #h4 = net.addHost('h4', ip='10.0.0.4')
    #h5 = net.addHost('h5', ip='10.0.0.5')
    
    info('*** Adding switch\n')
    s1 = net.addSwitch('s1')
    #s2 = net.addSwitch('s2')
    #s3 = net.addSwitch('s3')
    #s4 = net.addSwitch('s4')
    
    info('*** Creating host links\n')
    h1.linkTo(s1)
    h2.linkTo(s1) 
    h3.linkTo(s1) 
    #h2.linkTo(s2)
    #h3.linkTo(s3)
    #h4.linkTo(s1)
    #h5.linkTo(s4)
    
    info('*** Creating swicth links\n')
    #s1.linkTo(s2)
    #s2.linkTo(s4)
    #s2.linkTo(s3)
    
    print s1.intfNames()
    #print s2.intfNames()
    #print s3.intfNames()
    #print s4.intfNames()
    
    
    info('*** Starting network\n')
    net.start()
    
    info('*** Running CLI\n')
    CLI(net)

    info('*** Stopping network')
    net.stop()

if __name__ == '__main__':
    setLogLevel( 'info' )
    info( '*** QoSFlow network have been started\n' )
    QoSFlowNet()
