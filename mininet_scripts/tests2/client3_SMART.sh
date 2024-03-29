set -v
#~/dts/dts-client_old/chat.py -e client3_SMART -w wor3_SMART -vc -t 'MMstreaming' -bw 1500

echo "Initiating DTS<->IP converter..."
python2.7 /home/mininet/dts/dts-client_old/converter_smart.py h8-eth1 client3_SMART wor3_SMART 4551 55555 &
echo "Initiating DTS<->IP converter...done"

sleep 4

echo "Receiving video stream..."
#cvlc /home/mininet/dts/dts-client_old/video.mp4 --sout '#rtp{dst=127.0.0.1,port=55555,mux=ts,ttl=5}'
#ffmpeg -re -i /home/mininet/dts/dts-client_old/blockhead.mp4 -f mpegts -vcodec mpeg4 -strict -2 -acodec ac3 -ac 2 -ab 128k -r 30 -b:v 500k -threads 2 rtp://127.0.0.1:55555
ffplay rtp://127.0.0.1:55555
#cvlc ./sintel_trailer-480p.mp4  --sout '#transcode{vcodec=h264,vb=0,scale=0,acodec=mpga,ab=128,channels=2,samplerate=44100}:rtp{dst=127.0.0.1,port=55555,mux=ts,ttl=64}'
echo "Receiving video stream...done"
