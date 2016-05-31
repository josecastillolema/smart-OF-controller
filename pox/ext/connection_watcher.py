from pox.core import core
import pox.openflow.libopenflow_01 as of
from pox.lib.util import dpid_to_str
from pox.lib.recoco import Timer
 
log = core.getLogger()
 
#def _timer_func ():
#  for con in core.openflow.connections:
#    con.send(of.ofp_stats_request(body=of.ofp_port_stats_request()))
#  log.debug("Sent %i flow/port stats request(s)", len(core.openflow.connections))

class MyComponent (object):
  def __init__ (self):
    core.openflow.addListeners(self)
 
  def _handle_ConnectionUp (self, event):
    log.debug("Switch %s has come up.", dpid_to_str(event.dpid))
    #print event.ofp
    event.connection.send(of.ofp_stats_request(body=of.ofp_port_stats_request()))
    event.connection.send(of.ofp_stats_request(body=of.ofp_flow_stats_request()))
    event.connection.send(of.ofp_stats_request(body=of.ofp_desc_stats_request()))
    #event.connection.send(of.ofp_stats_request(body=of.ofp_table_stats_request()))

  #def _handle_PortStatus (self, event):
  #  if event.added:
  #    action = 'added'
  #  elif event.deleted:
  #    action = 'deleted'
  #  else:
  #    action = 'modified'
  #  print 'O porto %s no switch %s foi %s.' % (event.port, event.dpid, action)

  def _handle_SwitchDescReceived (self, event):
    log.debug("SwitchDescStatsReceived from %s", dpid_to_str(event.connection.dpid))

  def _handle_FlowStatsReceived (self, event):
    log.debug("FlowStatsReceived from %s", dpid_to_str(event.connection.dpid))

  def _handle_PortStatsReceived (self, event):
    log.debug("PortStatsReceived from %s", dpid_to_str(event.connection.dpid))
 
def launch ():
  core.registerNew(MyComponent)

  # timer set to execute every five seconds
  #Timer(5, _timer_func, recurring=True)
