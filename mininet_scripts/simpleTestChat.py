#!/usr/bin/python                                                                                      
                                                                                                       
from mininet.net import Mininet                                                                        
from mininet.node import Controller                                                                    
from mininet.topo import SingleSwitchTopo                                                              
from mininet.log import setLogLevel                                                                    
                                                                                                       
import os                                                                                              
                                                                                                       
class POXBridge( Controller ):                                                                         
    "Custom Controller class to invoke POX forwarding.l2_learning"                                     
    def start( self ):                                                                                 
        "Start POX learning switch"
	print 'yeaaaaaaaaaaa'                                                        
        self.pox = '%s/jose/pox/poadsfx.py' % os.environ[ 'HOME' ]
        self.cmd( self.pox, 'log.level --DEBUG misc.dtsa &' )  
        print self.pox, 'log.level --DEBUG misc.dtsa &'                                              
    def stop( self ):                                                                                  
        "Stop POX"           
	print 'killlllinnnnnnnnnn'                                                                          
        self.cmd( 'kill %' + self.pox )                                                                
                                                                                                       
controllers = { 'poxbridge': POXBridge }                                                               
                                                                                                       
if __name__ == '__main__':                                                                             
    setLogLevel( 'info' )                                                                              
    net = Mininet(topo=SingleSwitchTopo( 2 ), controller=POXBridge, cleanup=True, xterms=True)     
    net.start()                                                                                        
    net.pingAll()                                                                                      
    net.stop() 
