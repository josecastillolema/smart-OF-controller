# Prueba para iperf entre h1(s1) y h4(s3)

echo 'Inserting flows for test between h1(s1) and h4(s3)...'
ovs-ofctl add-flow s1 in_port=1,actions=enqueue:3:0
ovs-ofctl add-flow s1 in_port=3,actions=enqueue:1:0
ovs-ofctl add-flow s3 in_port=1,actions=enqueue:2:0
ovs-ofctl add-flow s3 in_port=2,actions=enqueue:1:0

echo
echo 'Printing flows for s1:'
ovs-ofctl dump-flows s1
echo
echo 'Printing flows for s3:'
ovs-ofctl dump-flows s3

# Results
#   myTopology                    -> 1.30 Gbits/sec, 2.78 Gbits/sec, 896 Mbits/sec
#   myTopology_smart  (con tcLink) ->  955 Kbits/sec, 1.07 Mbits/sec
#   myTopology_smart2 (con tcLink limitado a: bw=10, delay='200ms', jitter='2ms', loss=10, max_queue_size=1000, use_htb=True)
#      -> 19.1 Mbits/sec, 34.1 Mbits/sec, 3.03 Mbits/sec
#   myTopology_smart2 (con tcLink limitado a: bw=10, delay='200ms', jitter='2ms', loss=10)
#      -> 47.4 Mbits/sec
#   myTopology        -> 64 bytes from 10.0.0.4: ttl=64 time=0.066 ms
#   myTopology_smart  -> 64 bytes from 10.0.0.4: ttl=64 time=0.066 ms
#   myTopology_smart2 -> 64 bytes from 10.0.0.4: ttl=64 time=0.053 ms
