#!/usr/bin/python

import sys
import itertools
import gevent
import gevent.select
from gevent import monkey
monkey.patch_socket()

import dts

def main():
    if len(sys.argv) < 4 or len(sys.argv) > 6:
        print "Usage:\n{} <interface> <entity_title> <workspace_title> [qosRequired] [bwRequired]".format(sys.argv[0])
        sys.exit(1)

    iface = sys.argv[1]

    e = dts.Entity(iface, sys.argv[2], True)
    print 'Entity "{}" registered.'.format(e.title)

    try:
        if sys.argv[4] in ['true', 'yes', 'on']:
            qosRequired = True
        else:
            qosRequired = False
    except IndexError:
        qosRequired = False

    try:
        bwRequired = int(sys.argv[5])
    except IndexError:
        bwRequired = 0

    w = dts.Workspace(iface, sys.argv[3], qosRequired, bwRequired)

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
        while True:
            gevent.select.select([sys.stdin.fileno()], [], [])
            msg = raw_input()
            w.send(msg + '\n')
    except EOFError, KeyboardInterrupt:
        pass

    # User endeded session with EOF, stop receiving...
    reader.kill(block=True)

    # Done. The destructors will do the cleanup automatically for us.

if __name__ == "__main__":
    main()
