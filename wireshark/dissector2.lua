-- create myproto protocol and its fields
p_myproto = Proto ("myproto","My Protocol")
local f_command = ProtoField.uint16("myproto.command", "Command", base.HEX)
local f_data = ProtoField.string("myproto.data", "Data", FT_STRING)
 
p_myproto.fields = {f_command}
 
-- myproto dissector function
function p_myproto.dissector (buf, pkt, root)
  -- validate packet length is adequate, otherwise quit
  if buf:len() == 0 then return end
  pkt.cols.protocol = p_myproto.name
 
  -- create subtree for myproto
  subtree = root:add(p_myproto, buf(0))
  -- add protocol fields to subtree
  subtree:add(f_command, buf(0,2)):append_text(" [Command text]")
 
  -- description of payload
  subtree:append_text(", Command details here or in the tree below")
end
 
-- Initialization routine
function p_myproto.init()
end
 
-- register a chained dissector for port 8002
local tcp_dissector_table = DissectorTable.get("tcp.port")
dissector = tcp_dissector_table:get_dissector(8002)
  -- you can call dissector from function p_myproto.dissector above
  -- so that the previous dissector gets called
tcp_dissector_table:add(8002, p_myproto)
