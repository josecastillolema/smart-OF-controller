# Prueba para iperf entre h4(s3) y h3(s2)

echo 'Inserting flows for test between h4(s3) and h3(s2)...'
ovs-ofctl add-flow s3 in_port=1,actions=output:3
ovs-ofctl add-flow s3 in_port=3,actions=output:1
ovs-ofctl add-flow s2 in_port=1,actions=output:2
ovs-ofctl add-flow s2 in_port=2,actions=output:1

echo
echo 'Printing flows for s3'
ovs-ofctl dump-flows s3
echo
echo 'Printing flows for s2:'
ovs-ofctl dump-flows s2

# Results
#   myTopology       -> 2.43 Gbits/sec, 2.43 Gbits/sec
#   myTopology_smart ->  953 Kbits/sec, 1.04 Mbits/sec   deberia ser peor, o diferente, no lo entiendo moder fucker
#   myTopology_smart2 (con tcLink limitado a: bw=10, delay='200ms', jitter='2ms', loss=10)
#      -> 47.6 Mbits/sec
#   myTopology       -> 64 bytes from 10.0.0.4: ttl=64 time=0.066 ms
#   myTopology_smart -> 64 bytes from 10.0.0.4: ttl=64 time=0.066 ms

