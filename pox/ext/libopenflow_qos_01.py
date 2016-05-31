import struct
import pox.openflow.libopenflow_01 as of
from pox.lib.util import assert_type

ofp_qos_object_type_map = {
  'OFP_QOS_QDISC' : 0,             # Queueing discipline
  'OFP_QOS_CLASS' : 1,             # Class of queueing discipline
}

ofp_qos_action_type_rev_map = {
  'OFP_QOS_ACTION_ADD'    : 0,     # Add qdisc, class or filter
  'OFP_QOS_ACTION_DEL'    : 1,     # Delete qdisc or class
  'OFP_QOS_ACTION_CHANGE' : 2,     # Change an existing configuration
}

ofp_qos_error_type_rev_map = {
  'OFP_QOS_ERROR_NONE'           : 0,    # Success
  'OFP_QOS_ERROR_RTNETLINK'      : 1,    # Some error with rtnetlink
  'OFP_QOS_ERROR_INVALID_DEV'    : 2,    # There isn't device name
  'OFP_QOS_ERROR_UNKNOWN_ACTION' : 3,    # Just add, del and change
  'OFP_QOS_ERROR_UNKNOWN_OBJECT' : 4,    # Qdisc or class
  'OFP_QOS_ERROR_UNKNOWN_SCHED'  : 5,    # There isn't a sched name
}

ofp_qos_type_rev_map = {
  'OFP_QOS_SCHED_NONE'  : 0,       # Clear any rules associated to one or more ports
  'OFP_QOS_SCHED_HTB'   : 1,       # HTB scheduler (queueing discipline - classful)
  'OFP_QOS_SCHED_PFIFO' : 2,       # PFIFO scheduler (classless)
  'OFP_QOS_SCHED_BFIFO' : 3,       # BFIFO scheduler (classless)
  'OFP_QOS_SCHED_SFQ'   : 4,       # SFQ scheduler (classless)
  'OFP_QOS_SCHED_RED'   : 5,       # RED scheduler (classless)
}

_message_type_to_class = {}
_message_class_to_types = {}
ofp_type_rev_map = {}
ofp_type_map = {}

def openflow_message (ofp_type, type_val, reply_to=None, request_for=None, switch=False, controller=False):
  ofp_type_rev_map[ofp_type] = type_val
  ofp_type_map[type_val] = ofp_type
  def f (c):
    c.header_type = type_val
    c._from_switch = switch
    c._from_controller = controller
    _message_type_to_class[type_val] = c
    _message_class_to_types.setdefault(c, set()).add(type_val)
    return c
  return f

def openflow_s_message (*args, **kw):
  return openflow_message(switch=True, *args, **kw)

# Header to encapsulate all qos messages (controller --> datapath)
#  OFP_ASSERT(sizeof(struct ofp_qos_header) == 2);
class ofp_qos_header (of.ofp_base):
  _MIN_LENGTH = 2
  def __init__ (self, object_type, action_type):
    self.object_type = object_type       # uint8_t    QoS object type that message will carry
    self.action_type = action_type       # uint8_t    QoS action type of message

  def _validate (self):
    return None

  def pack (self):
    assert self._assert()

    packed = b""
    packed += struct.pack("!BB", self.object_type, self.action_type)
    return packed

  def unpack (self, raw, offset=0):
    offset,length = self._unpack_header(raw, offset)
    return offset,length

  def _unpack_header (self, raw, offset):
    offset,(self.object_type, self.action_type) = \
        _unpack("!BB", raw, offset)
    return offset,length

  def __eq__ (self, other):
    if type(self) != type(other): return False
    if self.object_type != other.object_type: return False
    if self.action_type != other.action_type: return False
    return True

  def show (self, prefix=''):
    outstr = ''
    outstr += prefix + 'object_type: ' + str(self.version) + '\n'
    outstr += prefix + 'action_type: ' + str(self.header_type) + '\n'
    return outstr
  
  def __str__ (self):
    return self.__class__.__name__ + "\n  " + self.show('  ').strip()

# Stochastic Fairness Queueing reorders  queued  traffic  so  each
# 'session' gets to send a packet in turn.
class ofp_qos_sqf (ofp_qos_header):
  def __init__ (self, port_no, class_id, rate, ceil, prio):
    ofp_qos_header.__init__(self, object_type, action_type)
    self.port_no = port_no        # uint16_t: Port number
    self.class_id = class_id      # uint32_t: Leaf class where a sched will be associated. Must be 0 if is root qdisc
    self.perturb = perturb        # uint32_t: Interval in seconds for queue algorithm perturbation
    self.quantum = quantum        # uint32_t: Amount of bytes a flow is allowed to dequeue during a round of the round robin process

  def pack (self):
    assert self._assert()

    packed = b""
    packed += ofp_qos_header.pack(self)
    packed += struct.pack('!HIII', self.port_no, self.class_id, self.perturb, self.quantum)
    return packed

  @staticmethod
  def __len__ ():
    return 16

#  Classfull queueing discipline
#
#  The Hierarchy Token Bucket implements a rich linksharing hierarchy
#  of classes with an emphasis on conforming to existing practices.
#  HTB facilitates guaranteeing bandwidth to classes, while also
#  allowing specification of upper limits to inter-class sharing.
#  It contains shaping elements, based on TBF and can prioritize classes.
# 
#  To see how HTB works, see: http://luxik.cdi.cz/~devik/qos/htb/manual/userg.htm"""
class ofp_qos_htb (ofp_qos_header):
  def __init__ (self, port_no, class_id, rate, ceil, prio):
    ofp_qos_header.__init__(self, object_type, action_type)
    self.port_no = port_no        # uint16_t: Port number
    self.class_id = class_id      # uint32_t: Child class where a sched will be associated
    self.rate = rate              # uint32_t: maximum rate this class and all its children are guaranteed
    self.ceil = ceil              # uint32_t: maximum rate at wich a class can send, if its parent has to spare
    self.prio = prio              # uint32_t: In the round-robin process, classes with the lowest priority field are tried for packets first

  def _validate (self):
    return None

  def pack (self):
    assert self._assert()

    packed = b""
    packed += ofp_qos_header.pack(self)
    packed += struct.pack('!HIIII', self.port_no, self.class_id, self.rate, self.ceil, self.prio)
    return packed

  def unpack (self, raw, offset=0):
    offset,length = self._unpack_header(raw, offset)
    offset,(self.port_no, self.class_id, self.rate, self.ceil, self.prio) = \
        _unpack("!HIIII", raw, offset)
    offset = _skip(raw, offset, 4)
    assert length == len(self)
    return offset,length

  @staticmethod
  def __len__ ():
    return 20

  def __eq__ (self, other):
    if type(self) != type(other): return False
    if not ofp_header.__eq__(self, other): return False
    if self.port_no != other.port_no: return False
    if self.class_id != other.class_id: return False
    if self.rate != other.rate: return False
    if self.ceil != other.ceil: return False
    if self.prio != other.prio: return False
    return True

  def show (self, prefix=''):
    outstr = ''
    outstr += prefix + 'header: \n'
    outstr += ofp_header.show(self, prefix + '  ')
    outstr += prefix + 'port_no: ' + str(self.port_no) + '\n'
    outstr += prefix + 'class_id: ' + str(self.class_id) + '\n'
    outstr += prefix + 'rate: ' + str(self.rate) + '\n'
    outstr += prefix + 'ceil: ' + str(self.ceil) + '\n'
    outstr += prefix + 'prio: ' + str(self.prio) + '\n'
    return outstr

# The * MAIN * data structure to "agregate" queuing discipline
# into the 'msg' field. The sender must fill this message
# with one queueing discipline type before to send.
@openflow_s_message("OFPT_QUEUEING_DISCIPLINE", 22)
class ofp_qos_msg (of.ofp_header):
  def __init__ (self, sched_type, body):
    of.ofp_header.__init__(self)        # OpenFlow protocol message header
    self.sched_type = sched_type        # uint8_t: If is HTB, SFQ, PFIFO, BFIFO, RED
    self.body = body                    # uint8_t: The 'body' can be one of the scheduler message (HTB, SFQ, ...)

  def _validate (self):
    return None

  def pack (self):
    assert self._assert()

    packed = b""
    packed += ofp_header.pack(self)
    packed += struct.pack('!BpppB', self.sched_type, '0', '0', '0', self.body)
    return packed

  def unpack (self, raw, offset=0):
    offset,length = self._unpack_header(raw, offset)
    offset,(self.sched_type, self.body) = \
        _unpack("!BpppB", raw, offset)
    offset = _skip(raw, offset, 4)
    assert length == len(self)
    return offset,length

  @staticmethod
  def __len__ ():
    return 32

  def __eq__ (self, other):
    if type(self) != type(other): return False
    if not ofp_header.__eq__(self, other): return False
    if self.sched_type != other.sched_type: return False
    if self.body != other.body: return False
    return True

  def show (self, prefix=''):
    outstr = ''
    outstr += prefix + 'header: \n'
    outstr += ofp_header.show(self, prefix + '  ')
    outstr += prefix + 'sched_type: ' + str(self.sched_type) + '\n'
    outstr += prefix + 'body: ' + str(self.body) + '\n'
    return outstr
