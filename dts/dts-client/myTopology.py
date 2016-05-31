"""Custom topology example

Two directly connected switches plus a host for each switch:

   host --- switch --- switch --- host

Adding the 'topos' dict with a key/value pair to generate our newly defined
topology enables one to pass in '--topo=mytopo' from the command line.

sudo mn --custom myTopology.py --topo mytopo --controller=remote,ip=127.0.0.1


s1 ------------------ s3-h4
| \                 / |
h1 h2             s2  s4-h5
                  |
				  h3
"""                 
import re

def toID(mac):
    return '0000' + re.sub('[:]', '', mac)

from mininet.topo import Topo
class MyTopo( Topo ):
	"Simple topology example."

	def __init__( self ):
		"Create custom topo."

		# Initialize topology
		Topo.__init__( self )

		# Add hosts and switches
		h1 = self.addHost('h1')
		h2 = self.addHost('h2')
		h3 = self.addHost('h3')
		h4 = self.addHost('h4')
		h5 = self.addHost('h5')
		s1 = self.addSwitch('s1', dpid=toID('00:00:00:00:00:01'))
		s2 = self.addSwitch('s2', dpid=toID('00:00:00:00:00:02'))
		s3 = self.addSwitch('s3', dpid=toID('00:00:00:00:00:03'))
		s4 = self.addSwitch('s4', dpid=toID('00:00:00:00:00:04'))

		# Add links
		self.addLink(s1, h1, 1, 1)
		self.addLink(s1, h2, 2, 1)
		self.addLink(s2, h3, 1, 1)
		self.addLink(s3, h4, 1, 1)
		self.addLink(s4, h5, 1, 1)
		self.addLink(s1, s3, 3, 2)
		self.addLink(s2, s3, 2, 3)
		self.addLink(s3, s4, 4, 2)
		self.addLink(s2, s4, 3, 3)
topos = { 'mytopo': ( lambda: MyTopo() ) }
