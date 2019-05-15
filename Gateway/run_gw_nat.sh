#!/bin/bash

#Enable NAT

iptables -A FORWARD -m conntrack --ctstate UNTRACKED -j NFQUEUE --queue-num 0 --in-interface $1
iptables -t raw -A PREROUTING -j NFQUEUE --queue-num 1 --in-interface $2
iptables -t raw -A PREROUTING -j NOTRACK -i $1
 
sysctl -w net.ipv4.ip_forward=1

nice --20 ./gatewayNAT $2 $3

#~ iptables --table nat --flush
iptables --table raw --flush
iptables --delete-chain
iptables --table nat --delete-chain 
sysctl -w net.ipv4.ip_forward=0
