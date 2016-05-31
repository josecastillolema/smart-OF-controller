#!/usr/bin/python2.7

import sys
import itertools
import gevent
import gevent.select
from subprocess import call
from gevent import monkey
monkey.patch_socket()
import hashlib
import dts

def main():
    if len(sys.argv) < 4:
        print "Usage:\n{} <interface> <entity_title> <workspace_title> <interval in ms>".format(
            sys.argv[0])

        print sys.argv[0]
        print '\n'
        print sys.argv[1]
        print '\n'
        print sys.argv[2]
        print '\n'
        sys.exit(1)


    iface = sys.argv[1]
    e = dts.Entity(iface, sys.argv[2], True)
    print 'Entity "{}" registered.'.format(e.title)
    resp = hashlib.sha256(sys.argv[3]).digest()[:6]
    w = dts.Workspace(iface, sys.argv[3])

    try:
        w.attach(e)
        print 'Attached to workspace "{}".'.format(w.title)	
    except dts.DTSException:
        # Failed to attach, probably does not exists,
        # then try to create
	print 'Failed to attach, trying to create'
        w.create_on_dts(e)
        print 'Created workspace "{}" and attached to it.'.format(w.title)

    def reader_loop():
        try:
            while True:
                msg = w.recv()
                sys.stdout.write(msg)
        except gevent.GreenletExit, KeyboardInterrupt:
            pass
    reader = gevent.spawn(reader_loop)

    # Receiver loop
    try:
        cont = 1
        while True:
            gevent.select.select([sys.stdin.fileno()], [], [])
            if len(sys.argv) == 5:
                from time import sleep
                sleep(float(sys.argv[4])/1000.)
                w.send(str(cont) + '\n')
                cont+=1
            else:
                msg = raw_input()
                try:
                    if msg[:13] == './videoserver':
                        import os
                        dire = os.getcwd()
                        os.system("ffmpeg -re -i {}/{} -f mpegts -vcodec mpeg4 -strict -2 -acodec ac3 -ac 2 -ab 128k -r 30 -b:v 2000k -threads 2 etcp:{}:{}".format(dire, msg.split()[1], ''.join( [ "%02x" % ord( x ) for x in resp[:12]]).strip(), iface))
                    elif msg[:13] == './videoclient':
                        import os
                        os.system("ffplay etcp:{}:{}".format(''.join( [ "%02x" % ord( x ) for x in resp[:12]] ).strip(), iface))
                    else:
                        w.send(msg + '\n')
                except:
                    print 'Incorrect input'
    except EOFError, KeyboardInterrupt:
        pass
    
    # User endeded session with EOF, stop receiving...
    reader.kill(block=True)

    # Done. The destructors will do the cleanup automatically for us.

if __name__ == "__main__":
    main()

