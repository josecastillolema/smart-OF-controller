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
		self.act_like_hub(packet, packet_in)

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


def launch ():
	def start_switch (event):
		log.debug("Conctrolling %s" % (event.connection,))
		MyCtrl(event.connection)
	core.openflow.addListenerByName("ConnectionUp", start_switch)
