#!/usr/bin/python

# Author: Jose Castillo Lema <josecastillolema@gmail.com>

import sys
import time
import random
import os

sessions = int(sys.argv[1])
l = [128,256,512,1024]

for i in range(0,sessions):
    #os.system("ffplay etcp:{}:{}".format(''.join(["%02x" % ord(x) for x in hash_title]).strip(), iface))
    #time.sleep(5)
    w = str(random.random()*100)
    e = str(random.random()*100)
    os.system("~/dts/dts-client_old/chat.py -t 'MMstreaming' -bw {} -w {} -e {} &".format(random.choice(l), w, e))
    os.kill(e, signal.SIGINT)
    time.sleep(random.randint(1,5))
