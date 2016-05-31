from protocol import dts_pb2, etcp_pb2
from dts_wire import buffer_splitter

import itertools
import hashlib

import socket
import struct
import fcntl

class DTSSocket(socket.socket):
	def __init__(self, iface):
		# Ethertype of MEHAR
		ETH_MEHAR = 0x0880

		# Creates a MEHAR socket
		socket.socket.__init__(self, socket.AF_PACKET, socket.SOCK_RAW, ETH_MEHAR);

		# Binds to one specific interface, while nobody cares to define
		# a substitute to local IP route table. Of course, an efficient
		# global routing algorithm is of minor importance when thinking
		# something to replace TCP/IP.
		self.bind((iface, ETH_MEHAR))

		# Set promiscuous mode...

		# Kernel constants
		SIOGIFINDEX = 0x8933

		SOL_PACKET = 263
		PACKET_ADD_MEMBERSHIP = 1
		PACKET_MR_PROMISC = 1

		# Find out device index, would be easier in Python 3 that has
		# socket.if_nametoindex() function...
		ifr = iface + "\0"*(20 - len(iface))
		r = fcntl.ioctl(self.fileno(), SIOGIFINDEX, ifr)
		ifidx = struct.unpack("16sI", r)[1]

		# Request promiscuous mode
		packet_mreq = struct.pack("IHH8s", ifidx, PACKET_MR_PROMISC, 6, "\0"*6)
		self.setsockopt(SOL_PACKET, PACKET_ADD_MEMBERSHIP, packet_mreq)

		# For generating ids:
		self.id_counter = itertools.count()

	def send(self, addr, *msg_bufs):
		# TODO: validate address size...

		# Ethertype of MEHAR
		ethertype = "\x08\x80"

		byte_seq = itertools.chain((addr, ethertype), msg_bufs)

		return socket.socket.send(self, ''.join(byte_seq))

	def send_all(self, addr, msg_buffer):
		sent = 0
		while sent < len(msg_buffer):
			sent += self.send(addr, msg_buffer[sent:])

	def recv_filter(self, addr):
		while True:
			# TODO: get interface's MTU instead of hardcoding
			resp = self.recv(1518)
			if resp[:12] == addr:
				# There is no need to check ethertype because
				# the socket interface will do it for us.
				return resp[14:] # Strip ETH header/footer

# Send message to DTS and wait for response.
# This function will discard incoming messages it doesn't find relevant,
# making it *NOT* reentrant (it may discard the response of a concurrent call).
def call_dts(sock, msg_obj, msg_type):
	header = dts_pb2.ControlRequest()
	header.type = msg_type
	header.id = sock.id_counter.next()

	# The chosen generic address to DTS
	addr = "DTS\x00\x00\x00\x00\x00\x00\x00\x00\x00"

	header_serial = header.SerializeToString()
	msg_serial = msg_obj.SerializeToString()
	sent = sock.send(addr,
			 struct.pack("<H", len(header_serial)), header_serial,
			 struct.pack("<H", len(msg_serial)), msg_serial)
	if sent != 0:
		# TODO: deal with the case the message to DTS doesn't
		# fit the ethernet frame
		pass

	# Wait for response
	resp_obj = dts_pb2.ControlResponse()
	found_resp = False
	while not found_resp:
		data = sock.recv_filter(addr)
		for msg_buffer in buffer_splitter(data):
			resp_obj.ParseFromString(msg_buffer)
			#print resp_obj.request_id, header.id
			if resp_obj.request_id == header.id:
				found_resp = True
				break;
	return resp_obj

class DTSException(Exception):
	pass

class Workspace(object):
	def __init__(self, iface, title, qosRequired=False, bwRequired=None, cosRequired=None):
		self.title = title
		self.qosRequired     = qosRequired
		self.bwRequired      = bwRequired
		self.cosRequired     = cosRequired
		self.hash_title      = hashlib.sha256(title).digest()[:12]
		self.socket          = DTSSocket(iface)
		self.attached_entity = None
		self.created         = False

	def __del__(self):
		if self.attached_entity:
			self.detach()
		if self.created:
			self.delete_on_dts()
		self.socket.close()

	def create_on_dts(self, auto_attach_to=None):
		msg = etcp_pb2.WorkspaceCreate()
		msg.workspace_title = self.title

		try:
			if self.qosRequired:
				capabilities = msg.capabilities.add()
				capabilities.type   = 1                      # qos_required_bool
				capabilities.length = 1
				capabilities.value  = 'True'
			if self.bwRequired:
				capabilities = msg.capabilities.add()
				capabilities.type   = 2                      # bw_required_int32
				capabilities.length = 1
				capabilities.value  = str(self.bwRequired)
				if self.cosRequired:
					capabilities = msg.capabilities.add()
					capabilities.type   = 3                      # cos_required_sting
					capabilities.length = 1
					capabilities.value  = self.cosRequired
		except NameError:
			pass

		if auto_attach_to:
			msg.entity_title = auto_attach_to.title
			msg.attach_too = True

		res = call_dts(self.socket, msg, dts_pb2.ControlRequest.ETCP_WORKSPACE_CREATE)
		if res.status != dts_pb2.ControlResponse.SUCCESS:
			raise DTSException("Workspace creation failed.")

		self.created = True
		if auto_attach_to:
			self.attached_entity = auto_attach_to

	def delete_on_dts(self):
		msg = etcp_pb2.WorkspaceDelete()
		msg.title = self.title

		res = call_dts(self.socket, msg, dts_pb2.ControlRequest.ETCP_WORKSPACE_DELETE)
		if res.status != dts_pb2.ControlResponse.SUCCESS:
			raise DTSException("Workspace deletion failed.")

		self.created = False

	def attach(self, entity):
		msg = etcp_pb2.WorkspaceAttach()
		msg.workspace_title = self.title
		msg.entity_title = entity.title

		res = call_dts(self.socket, msg, dts_pb2.ControlRequest.ETCP_WORKSPACE_ATTACH)
		if res.status != dts_pb2.ControlResponse.SUCCESS:
			raise DTSException("Failed to attach to workspace.")

		self.attached_entity = entity

	def detach(self):
		msg = etcp_pb2.WorkspaceDetach()
		msg.workspace_title = self.title
		msg.entity_title = self.attached_entity.title

		res = call_dts(self.socket, msg, dts_pb2.ControlRequest.ETCP_WORKSPACE_DETACH)
		if res.status != dts_pb2.ControlResponse.SUCCESS:
			raise DTSException("Failed to detach from workspace.")

		self.attached_entity = False

	def send(self, msg):
		self.socket.send_all(self.hash_title, msg)

	def recv(self):
		return self.socket.recv_filter(self.hash_title)

class Entity(object):
	def __init__(self, iface, title, register_now=False):
		self.iface = iface
		self.title = title
		self.registered = False
		self.socket = DTSSocket(iface)
		if register_now:
			self.register()

	def __del__(self):
		if self.registered:
			self.unregister()
		self.socket.close()
			
	def register(self):
		msg = etcp_pb2.EntityRegister()
		msg.title = self.title
		res = call_dts(self.socket, msg, dts_pb2.ControlRequest.ETCP_ENTITY_REGISTER)
		if res.status != dts_pb2.ControlResponse.SUCCESS:
			raise DTSException("Failed to register entity.")

		self.registered = True

	def unregister(self):
		msg = etcp_pb2.EntityUnregister()
		msg.title = self.title
		res = call_dts(self.socket, msg, dts_pb2.ControlRequest.ETCP_ENTITY_UNREGISTER)
		if res.status != dts_pb2.ControlResponse.SUCCESS:
			raise DTSException("Failed to unregister entity.")

		self.registered = False
