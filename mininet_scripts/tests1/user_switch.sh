# Prueba para iperf entre h1(s1) y h4(s3)

echo 'Deleting previous flows'
ovs-ofctl dump-flows I1
ovs-ofctl dump-flows E3

echo 'Inserting flows for test between h1(I1) and h4(E3)...'
ovs-ofctl add-flow I1 dl_src=5a:b6:6a:83:e9:4d,dl_dst=84:ff:2a:a8:45:7a,dl_type=0x0880,actions=enqueue:1q1,enqueue:3q1
ovs-ofctl add-flow I1 dl_src=3d:a1:8d:2a:cd:03,dl_dst=47:97:6e:71:3c:11,dl_type=0x0880,actions=enqueue:1q0,enqueue:3q0

ovs-ofctl add-flow E3 dl_src=5a:b6:6a:83:e9:4d,dl_dst=84:ff:2a:a8:45:7a,dl_type=0x0880,actions=enqueue:1q1,enqueue:2q1
ovs-ofctl add-flow E3 dl_src=3d:a1:8d:2a:cd:03,dl_dst=47:97:6e:71:3c:11,dl_type=0x0880,actions=enqueue:1q0,enqueue:2q0

echo
echo 'Printing flows for s1:'
ovs-ofctl dump-flows I1
echo
echo 'Printing flows for s3:'
ovs-ofctl dump-flows E3

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
