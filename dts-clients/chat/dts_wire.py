import struct

# With the wire format of 2 bytes little-endian size preceeding each
# protobuff message, this iterator will return the correctly split message
# buffer.
def buffer_splitter(full_buffer):
	bcount = 0
	full_len = len(full_buffer)
	while bcount < full_len:
		(msg_size,) = struct.unpack("<H", full_buffer[bcount:bcount+2])
		bcount += 2

		yield full_buffer[bcount:bcount+msg_size]
		bcount += msg_size
