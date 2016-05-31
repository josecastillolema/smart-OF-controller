#!/usr/bin/python

# Author: Jose Castillo Lema <josecastillolema@gmail.com>

import sys
import time
import random
import os
import subprocess
import signal
import threading
import shlex

sessions = int(sys.argv[1])
l = [128,256,512,1024]

def startChat():
    s = "/home/mininet/dts/dts-client_old/chat.py -t 'MMstreaming' -bw {} -w {} -e {}".format(random.choice(l), w, e)
    args = shlex.split(s)
    p = subprocess.Popen(args)
    time.sleep(4)
    print 'KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK'
    os.kill(p.pid, signal.SIGINT)

for i in range(0,sessions):
    #os.system("ffplay etcp:{}:{}".format(''.join(["%02x" % ord(x) for x in hash_title]).strip(), iface))
    #time.sleep(5)
    w = str(random.random()*100)
    e = str(random.random()*100)
    hilo = threading.Thread(target=startChat)
    hilo.start()
    time.sleep(random.randint(1,5))
    
    
    
