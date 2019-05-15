#!/bin/bash
#Initial wifi interface configuration
ifconfig $1 down
ifconfig $1 hw ether 00:00:00:00:00:01
ifconfig $1 up 192.168.5.1 netmask 255.255.255.0
iw phy phy0 interface add mon0 type monitor
ifconfig mon0 up
sleep 2
 
###########Start dnsmasq, modify if required##########
if [ -z "$(ps -e | grep dnsmasq)" ]
then
rm dnsmasq.conf
echo "no-resolv" >> dnsmasq.conf
echo "bind-interfaces" >> dnsmasq.conf
echo "# Interface to bind to" >> dnsmasq.conf
echo "interface="$1 >> dnsmasq.conf
echo "# Specify starting_range,end_range,lease_time" >> dnsmasq.conf
echo "dhcp-range=192.168.5.2,192.168.5.20,72h" >> dnsmasq.conf
echo "# dns addresses to send to the clients" >> dnsmasq.conf
echo "server=10.6.0.12" >> dnsmasq.conf
echo "server=10.6.0.11" >> dnsmasq.conf
echo "server=8.8.8.8" >> dnsmasq.conf
echo "server=8.8.4.4" >> dnsmasq.conf
 dnsmasq -C ./dnsmasq.conf
fi
###########
 
#Enable NAT
iptables --flush
#~ iptables --table nat --flush
iptables --table raw --flush
iptables --delete-chain
iptables --table nat --delete-chain
#~ iptables --table nat --append POSTROUTING --out-interface $2 -j MASQUERADE
#~ iptables --append FORWARD --in-interface $1 -j ACCEPT
 
#Thanks to lorenzo
#Uncomment the line below if facing problems while sharing PPPoE, see lorenzo's comment for more details
#iptables -I FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

iptables -A FORWARD -m conntrack --ctstate UNTRACKED -j NFQUEUE --queue-num 0 --in-interface $1
iptables -t raw -A PREROUTING -j NFQUEUE --queue-num 1 --in-interface $2
iptables -t raw -A PREROUTING -j NOTRACK -i $1
 
sysctl -w net.ipv4.ip_forward=1

rm dataSignal.click
echo "from_dev :: FromDevice(mon0)" >> dataSignal.click
echo "-> RadiotapDecap()" >> dataSignal.click
echo "-> phyerr_filter :: FilterPhyErr()" >> dataSignal.click
echo "-> tx_filter :: FilterTX()" >> dataSignal.click
echo "-> dupe :: WifiDupeFilter() " >> dataSignal.click
echo "-> wifi_cl :: Classifier(0/08%0c 1/01%03, //data" >> dataSignal.click
echo "-);" >> dataSignal.click
			 
echo "wifi_cl [0]" >> dataSignal.click
echo "-> PrintSignalStrength("$1")" >> dataSignal.click
echo "-> Discard" >> dataSignal.click

echo "wifi_cl [1]" >> dataSignal.click
echo "-> Discard" >> dataSignal.click

click dataSignal.click &

./hostapd ./hostapd.conf

killall click

rm dataSignal.click

ifconfig $1 0
ifconfig $1 down
ifconfig $1 hw ether $(ethtool -P $1 | awk '{print $3}')
ifconfig $1 up
iw dev mon0 del
killall dnsmasq
rm dnsmasq.conf
iptables --flush
#~ iptables --table nat --flush
iptables --table raw --flush
iptables --delete-chain
iptables --table nat --delete-chain 
sysctl -w net.ipv4.ip_forward=0
