#!/usr/bin/python2.7

import sys
import itertools
import netifaces
import hashlib
import os
import gevent
import gevent.select
from argparse import ArgumentParser
from gevent import monkey
monkey.patch_socket()

import dts

def main():
    argp = ArgumentParser(
        version='1.0',
        description="ETArch's SMART chat with QoS support",
    )
    helpType = 'NC: Network Control | telephony: Telephony | \
                MMconferencing: Multimedia Conferencing | RT: Real-Time Interactive | \
                MMstreaming: Multimedia Streaming |\
                bVideo: Broadcast Video'
    argp.add_argument('-i' , '--iface'    , metavar='INTERFACE',  type=str,         help='interface')
    argp.add_argument('-e' , '--entity'   ,                       type=str,         help='entity title')
    argp.add_argument('-w' , '--workspace', required=True,        type=str,         help='workspace title')
    argp.add_argument(       '--interval' ,                       type=int,         help='interval in ms')
    argp.add_argument('-q' , '--QoS'      , action='store_const', const=True,       help='require Quality of Service aware routing')
    argp.add_argument('-bw', '--bandwidth',                       type=int,         help='bandwidth allocation required')
    argp.add_argument('-t' , '--type'     , choices=['NC', 'telephony', 'MMconferencing', 'RT', 'MMstreaming', 'bVideo'], help=helpType)
    argp.add_argument('-vs', '--server'   , metavar='FILE',       type=str,         help='start a ffmpeg video server on this host')
    argp.add_argument('-vc', '--client'   , action='store_const', const=True,       help='start a ffmpeg video client on this host')
    argp.add_argument('-b' , '--bitrate'  ,                       type=int,         help='bitrate for the ffmpeg video server (Kbits/s)')
    args = argp.parse_args()
    #print vars(args)

    if args.type and not args.bandwidth:
        argp.print_usage()
        print 'chat.py: error: argument -bw/--bandwidth is required'
        sys.exit(1)
    if args.bitrate and not args.server:
        argp.print_usage()
        print 'chat.py: error: argument -vs/--server is required'
        sys.exit(1)

    if args.iface:
         iface = args.iface
    else:
         iface = netifaces.interfaces()[1] if netifaces.interfaces()[0] == 'lo' else netifaces.interfaces()[0]

    if args.entity:
         entity = args.entity
    else:
         entity = iface.partition('-')[0] if iface.partition('-')[0][0] == 'h' else iface.partition('-')[2]

    e = dts.Entity(iface, entity, True)
    print 'Entity "{}" registered.'.format(e.title)

    w = dts.Workspace(iface, args.workspace, args.QoS, args.bandwidth, args.type)

    try:
        w.attach(e)
        print 'Attached to workspace "{}".'.format(w.title)	
    except dts.DTSException:
        # Failed to attach, probably does not exists,
        # then try to create
        print 'Failed to attach, trying to create'
        w.create_on_dts(e)
        print 'Created workspace "{}" and attached to it.'.format(w.title)

    if not args.client:
        def reader_loop():
            try:
                while True:
                    msg = w.recv()
                    sys.stdout.write(msg)
            except gevent.GreenletExit, KeyboardInterrupt:
                pass
        reader = gevent.spawn(reader_loop)
    else:
        hash_title = hashlib.sha256(args.workspace).digest()[:12]
        #os.system("ffplay etcp:{}:{}".format(''.join(["%02x" % ord(x) for x in hash_title]).strip(), iface))
        os.system('ffmpeg -re -i etcp:{}:{} -f mpegts -vcodec mpeg4 -strict -2 -acodec ac3 -ac 2 -ab 128k -r 30 -b:v 500k -threads 2 salida{}.avi'
                  .format(''.join(["%02x" % ord(x) for x in hash_title]).strip(), iface, iface))

    # Receiver loop
    if not args.server:
        try:
           cont = 1
           while True:
               gevent.select.select([sys.stdin.fileno()], [], [])
               if args.interval:
                   from time import sleep
                   sleep(float(args.interval)/1000.)
                   w.send(str(cont) + '\n')
                   cont+=1
               else:
                   msg = raw_input()
                   w.send(msg + '\n')
        except EOFError, KeyboardInterrupt:
            pass
        # User endeded session with EOF, stop receiving...
        reader.kill(block=True)
    else:
         hash_title = hashlib.sha256(args.workspace).digest()[:12]
         os.system("ffmpeg -re -i {} -f mpegts -vcodec mpeg4 -strict -2 -acodec ac3 -ac 2 -ab 128k -r 30 -b:v 500k -threads 2 etcp:{}:{}"
         #os.system("ffmpeg -re -i {} -f mpegts -vcodec mpeg4 -strict -2 -acodec ac3 -ac 2 -ab 128k -r 30 -b:v 2000k -threads 2 etcp:{}:{}"
         #os.system("ffmpeg -re -i blockhead.mp4 -f mpegts -vcodec mpeg4 -strict -2 -an -r 30 -b:v 1000k -threads 2 etcp:{}:{}"     # funciona mal, no se pq
                   .format(args.server, ''.join(["%02x" % ord(x) for x in hash_title]).strip(), iface))

    # Done. The destructors will do the cleanup automatically for us.

if __name__ == "__main__":
    main()
