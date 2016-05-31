# Flooding de myTopology.py
#
#       3             2     1
#  s1 ------------------ s3-------h4
#  |1\2               3/   \4
#  |  \              2/     \2
#  h1  h2            s2------s4----5
#                     | 3   3   1
#                    1|
#                     h3
#echo -v

echo 'Printing stats for s1 BEFORE flooding:'
ovs-ofctl dump-ports s1
echo
echo 'Flooding myTopology...'
ovs-ofctl add-flow s1 in_port=1,actions=flood
ovs-ofctl add-flow s1 in_port=2,actions=flood
ovs-ofctl add-flow s1 in_port=3,actions=flood
ovs-ofctl add-flow s3 in_port=1,actions=flood
ovs-ofctl add-flow s3 in_port=2,actions=output:3
ovs-ofctl add-flow s3 in_port=3,actions=output:2
ovs-ofctl add-flow s3 in_port=4,actions=output:2
ovs-ofctl add-flow s2 in_port=1,actions=flood
ovs-ofctl add-flow s2 in_port=2,actions=output:3
ovs-ofctl add-flow s2 in_port=3,actions=output:2
ovs-ofctl add-flow s4 in_port=1,actions=flood
ovs-ofctl add-flow s4 in_port=2,actions=output:3
ovs-ofctl add-flow s4 in_port=3,actions=output:2

echo
echo 'Printing flows for s1:'
ovs-ofctl dump-flows s1

#sleep 0.1

echo
echo 'Printing stats for s1 AFTER flooding:'
ovs-ofctl dump-ports s1

#echo
#echo 'Removing flows...'
#ovs-ofctl del-flows s1
#ovs-ofctl del-flows s3
#ovs-ofctl del-flows s2
#ovs-ofctl del-flows s4

