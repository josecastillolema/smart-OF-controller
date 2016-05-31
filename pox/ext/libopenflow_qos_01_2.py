import struct
import pox.openflow.libopenflow_01 as of
from pox.lib.util import assert_type

OFP_VERSION = 0x01

ofp_qos_object_type_rev_map = {
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

# ----------------------------------------------------------------------
# XID Management
# ----------------------------------------------------------------------

MAX_XID = 0x7fFFffFF

def XIDGenerator (start = 1, stop = MAX_XID):
  i = start
  while True:
    yield i
    i += 1
    if i > stop:
      i = start

def xid_generator (start = 1, stop = MAX_XID):
  return XIDGenerator(start, stop).next

def user_xid_generator ():
  return xid_generator(0x80000000, 0xffFFffFF)

generate_xid = xid_generator()

# -----------------------------------------------------------------------

class _ofp_meta (type):
  """
  This takes care of making len() work as desired.
  """
  def __len__ (cls):
    try:
      return cls.__len__()
    except:
      return cls._MIN_LENGTH

class ofp_base (object):
  """
  You should implement a __len__ method.  If your length is fixed, it
  should be a static method.  If your length is not fixed, you should
  implement a __len__ instance method and set a class level _MIN_LENGTH
  attribute to your minimum length.
  """
  __metaclass__ = _ofp_meta

  def _assert (self):
    r = self._validate()
    if r is not None:
      raise RuntimeError(r)
      return False # Never reached
    return True

  def _validate (self):
    return None

  def __ne__ (self, other):
    return not self.__eq__(other)

  @classmethod
  def unpack_new (cls, raw, offset=0):
    o = cls()
    r,length = o.unpack(raw, offset)
    assert (r-offset) == length, o
    return (r, o)

class ofp_header (ofp_base):
  _MIN_LENGTH = 8
  def __init__ (self, version=OFP_VERSION, header_type=22):
    #self.version = OFP_VERSION
    #self.header_type = 22             # OFPT_QUEUEING_DISCIPLINE
    self._xid = None

  def _validate (self):
    return None

  def pack (self):
    assert self._assert()
    packed = b""
    packed += struct.pack("!BBHL", OFP_VERSION, 22, 32, generate_xid())
    return packed

  def unpack (self, raw, offset=0):
    offset,length = self._unpack_header(raw, offset)
    return offset,length

  def _unpack_header (self, raw, offset):
    offset,(self.version, self.header_type, length, self.xid) = \
        _unpack("!BBHL", raw, offset)
    return offset,length

  def __eq__ (self, other):
    if type(self) != type(other): return False
    if self.version != other.version: return False
    if self.header_type != other.header_type: return False
    if len(self) != len(other): return False
    if self.xid != other.xid: return False
    return True

  def show (self, prefix=''):
    outstr = ''
    outstr += prefix + 'version: ' + str(self.version) + '\n'
    outstr += prefix + 'type:    ' + str(self.header_type)# + '\n'
    outstr += " (" + ofp_type_map.get(self.header_type, "Unknown") + ")\n"
    try:
      outstr += prefix + 'length:  ' + str(len(self)) + '\n'
    except:
      pass
    outstr += prefix + 'xid:     ' + str(self.xid) + '\n'
    return outstr
  
  def __str__ (self):
    return self.__class__.__name__ + "\n  " + self.show('  ').strip()

class ofp_qos_header():
  def __init__ (self, object_type=None, action_type=None): 
    self.object_type = object_type       # uint8_t    QoS object type that message will carry
    self.action_type = action_type       # uint8_t    QoS action type of message

class ofp_qos_sqf ():
  def __init__ (self, ofp_qos_header=None, port_no=None, class_id=None, perturb=None, quantum=None):
    self.ofp_qos_header = ofp_qos_header
    self.port_no = port_no        # uint16_t: Port number
    self.class_id = class_id      # uint32_t: Leaf class where a sched will be associated. Must be 0 if is root qdisc
    self.perturb = perturb        # uint32_t: Interval in seconds for queue algorithm perturbatio     
    self.quantum = quantum        # uint32_t: Amount of bytes a flow is allowed to dequeue during a round of the round robin process
    
class ofp_qos_htb (ofp_qos_header):
  def __init__ (self, port_no, class_id, rate, ceil, prio):
    ofp_qos_header.__init__(self, object_type, action_type)
    self.port_no = port_no        # uint16_t: Port number
    self.class_id = class_id      # uint32_t: Child class where a sched will be associated
    self.rate = rate              # uint32_t: maximum rate this class and all its children are guaranteed
    self.ceil = ceil              # uint32_t: maximum rate at wich a class can send, if its parent has to spare
    self.prio = prio              # uint32_t: In the round-robin process, classes with the lowest priority field are tried for packets first

# The * MAIN * data structure to "agregate" queuing discipline
# into the 'msg' field. The sender must fill this message
# with one queueing discipline type before to send.
class ofp_qos_msg (ofp_header):
  _MIN_LENGTH = 72
  def __init__ (self, ofp_header=None, sched_type=None, body=None):
    self.ofp_header = ofp_header
    self.sched_type = sched_type        # uint8_t: If is HTB, SFQ, PFIFO, BFIFO, RED
    self.body = body                    # uint8_t: The 'body' can be one of the scheduler message (HTB, SFQ, ...)

  def __len__ (self):
    l = 32 + len(self.sched_type)
    l += len(self.body)
    return l
