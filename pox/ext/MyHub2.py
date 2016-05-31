from pox.core import core
import pox.openflow.libopenflow_01 as of
import re

log = core.getLogger()

class MyCtrl (object):
	def __init__ (self, connection):
		self.connection = connection
		connection.addListeners(self)
		self.macStore = {}

	def _handle_PacketIn (self, event):
		packet = event.parsed
		if not packet.parsed:
			log.warning("Ignoring incomplet packet")
			return
		packet_in = event.ofp
		self.act_like_lswitch(packet, packet_in)

	def act_like_hub (self, packet, packet_in):
		self.send_packet(packet_in.buffer_id, packet_in.data,
		of.OFPP_FLOOD, packet_in.in_port)

 	def send_packet (self, buffer_id, raw_data, out_port, in_port):
		msg = of.ofp_packet_out()
		msg.in_port = in_port
		msg.data = raw_data
		action = of.ofp_action_output(port = out_port)
		msg.actions.append(action)
		self.connection.send(msg)

	def act_like_lswitch(self, packet, packet_in):
		srcaddr = packet.src
		if not self.macStore.has_key(srcaddr):
			self.macStore[srcaddr] = packet_in.in_port
			log.debug("New Host detected with MAC %s on Port %s" % (srcaddr, packet_in.in_port))
		dstaddr = packet.dst
		if self.macStore.has_key(dstaddr):
			fm = of.ofp_flow_mod()
			fm.match.in_port = packet_in.in_port
			fm.match.dl_dst = dstaddr
			fm.actions.append(of.ofp_action_output(port = self.macStore[dstaddr]))
			log.debug("Installing FlowTable entry")
			self.connection.send(fm)
			self.send_packet(packet_in.buffer_id, packet_in.data, self.macStore[dstaddr], packet_in.in_port)
			log.debug("Packet <" + str(packet_in.buffer_id) + "> forwarded by controller over Port " + str(self.macStore[dstaddr]))
		else :
			self.act_like_hub(packet, packet_in)
			log.debug("Packet <" + str(packet_in.buffer_id) + "> flooded by controller")

def launch ():
	def start_switch (event):
		log.debug("Conctrolling %s" % (event.connection,))
		MyCtrl(event.connection)
	core.openflow.addListenerByName("ConnectionUp", start_switch)
