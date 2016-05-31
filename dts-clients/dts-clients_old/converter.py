#!/usr/bin/python

# Author: Carlos Guimaraes <cguimaraes@av.it.pt>

import sys
import itertools
import gevent
import gevent.select
from gevent import monkey
monkey.patch_socket()

import dts
import asyncore, socket
from threading import Thread


class asynIPServer(asyncore.dispatcher):
    def __init__(self, ip, port, w):
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.bind((ip, port))
	self.w = w;

    def handle_read(self):
        data = self.recv(1500)
        self.w.send(data + '\n')


def main():
    if len(sys.argv) < 6:
        print "Usage:\n{} <interface> <entity_title> <workspace_title> <input port> <output port>".format(
            sys.argv[0])
        sys.exit(1)
    iface = sys.argv[1]
    e = dts.Entity(iface, sys.argv[2], True)
    print 'Entity "{}" registered.'.format(e.title)

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

    def DTSreader_loop():
        try:
            while True:
                msg = w.recv()

                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.sendto(msg, ('127.0.0.1', int(sys.argv[5])))
                #sock.sendto(msg, ('10.0.0.5', int(sys.argv[5])))

        except gevent.GreenletExit, KeyboardInterrupt:
            pass

    DTSreader = gevent.spawn(DTSreader_loop)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', int(sys.argv[4])))

    def IPreader_loop():
        try:
            while True:
                data = sock.recv(1500)
                w.send(data + '\n')
        except gevent.GreenletExit, KeyboardInterrupt:
            pass
        print "\nreader_loop END\n\n"

    IPreader = gevent.spawn(IPreader_loop)

    # Waiting for events
    gevent.select.select([], [], [])
    while True:
        input("Press Enter to continue...")

    # User endeded session with EOF, stop receiving...
    reader.kill(block=True)

    # Done. The destructors will do the cleanup automatically for us.

if __name__ == "__main__":
    main()

