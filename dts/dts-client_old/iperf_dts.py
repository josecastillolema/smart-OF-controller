#!/usr/bin/python

# Laurent GUERBY 20120304
# on server 91.2saasal24.148.1
# while true; do date; ./iperf.py -s 5555; sleep 1; done
# on client source route from 192.168.88.221
# T=t1; for i in 5 8; do for j in 400 1200; do N=z-$T-$i-$j.csv; ./iperf.py -a 192.168.88.221 -c 91.224.148.1:5555 -d ${i}.0e6 -l $j -n 2000 >& $N; tail -5 $N; sleep 1; done;done

import time
import os
import sys
import socket
import select
import struct
import random
import gevent
import gevent.select
from gevent import monkey
monkey.patch_socket()

import dts

N=256*256*256*256-1
S=160000
random.seed(0)
random_s="".join([struct.pack("I",random.randint(0,N)) for i in xrange(S/4)])

mode=None
RBUFL=2000
BYE="BYE"
PL=1200 # packet of 1200 bytes content
TR=1.0e6 # Mbit/s
NP=1000 # number of packets

opt_l=sys.argv[1:]
while len(opt_l)>0:
    opt=opt_l.pop(0)
    if opt=="-c":
        entity = 'e2'
        workspace = 'w'
        mode="client"
    elif opt == "-s":
        entity = 'e'
        workspace = 'w'
        mode="server"
    elif opt=="-a":
        addr=opt_l.pop(0)
    elif opt=="-d":
        TR=float(opt_l.pop(0))
    elif opt=="-l":
        PL=int(opt_l.pop(0))
    elif opt=="-n":
        NP=int(opt_l.pop(0))

#s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
iface = 'h2-eth0'
e = dts.Entity(iface, entity, True)
print 'Entity "{}" registered.'.format(e.title)

w = dts.Workspace(iface, workspace)

try:
    w.attach(e)
    print 'Attached to workspace "{}".'.format(w.title)
except dts.DTSException:
    # Failed to attach, probably does not exists,
    # then try to create
    w.create_on_dts(e)
    print 'Created workspace "{}" and attached to it.'.format(w.title)


sleep_delay=PL*8.0/TR
PLOVER=8+20+8 # 8 byte seq and stamp, 20 byte IP header, 8 byte UDP header
PL2=PL-PLOVER

print mode, 'workspace:', workspace, 'entity:', entity, 'sleep_delay:', sleep_delay*1.0e6, 'PL:', PL, 'TR:', TR

info_l=[]

if mode=="server":
    p  = w.recv()
    PL,TR,NP = struct.unpack("IfI",p)
    sleep_delay = PL*8.0/TR
    PL2 = PL-PLOVER
    print 'sleep_delay:', sleep_delay, 'PL:', PL, 'TR:', TR, 'NP:', NP
    seq_n = 0
    rand_i = 0
    t0=time.time()
    for i in xrange(NP):
        tt = time.time()
        t1 = int((tt-t0)*1.0e6)
        seq_n += 1
        buf = struct.pack("II%ds"%PL2, seq_n, t1, random_s[:PL2])
        w.send(buf)
        time.sleep(min(max(0.0,t0+(i+1)*sleep_delay-time.time()),sleep_delay)) # to tt+sleep_delay
    for i in xrange(10):
        w.send(BYE)
        time.sleep(sleep_delay)
    
elif mode=="client":
    w.send(struct.pack("IfI",PL,TR,NP))
    t0 = time.time()
    while True:
        t1=int((time.time()-t0)*1.0e6)
        buf = w.recv()
        if buf == BYE: break
        seq_n, t1p, sd = struct.unpack("II%ds"%PL2, buf)
        info_l.append((t1, seq_n, t1p,t1-t1p))
 
    min_t = min([x[3] for x in info_l])
    for t1, seq_n, t1p, td in info_l:
        print "t1:%d, seq_n:%d, t1p:%d, td:%d, td-min_t:%d"%(t1,seq_n,t1p,td,td-min_t)
    dt = (info_l[-1][0]-info_l[0][0])*1.0e-6
    np = len(info_l)
    print np*PL*8.0/dt,float(np)/dt,TR/PL/8.0,np, 'PL:', PL, 'TR:', TR, 'NP:', NP
