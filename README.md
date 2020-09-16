# Handover
This project is the testbed implementation of a Software Defined Networking (SDN) framework for Enterprise WLANs (SDEW). The SDEW framework provides seamless, client-unaware handover for a mobile device moving from one AP to another. To understand the details of the SDEW framework, the seamless handover and the testbed implementation, kindly refer to the following papers:
1) [An SDN framework for seamless mobility in enterprise WLANs](https://doi.org/10.1109/PIMRC.2015.7343624)
2) [A NAT based seamless handover for Software Defined Enterprise WLANs](https://doi.org/10.1007/978-3-030-30523-9_7)
3) [Testbed evaluation of a seamless handover mechanism for an SDN-based enterprise WLAN](https://doi.org/10.1007/s12046-019-1229-3)

The project uses hostapd (version 2.8), Floodlight OpenFlow Controller (version 0.90) and Click Modular Router (version 2.1). Additionally, dnsmasq also needs to be installed for the project to work.

**Note: The project was tested in Ubuntu 18.04 LTS.**
## Modifications/Additions
For implementation of the proposed SDEW framework and the seamless handover mechanism, various modifications were made to the source files of the softwares. Additionally, many new files were also added.
### hostapd
The directory *ofSwitchAP* was added inside the *hostapd-2.8/src* directory. All the relevant files, required for the implementation, were added to the *ofSwitchAP* directory. Additionally, various exisitng files were also modified. The following `#define` directives in the **controller_interface.c** file inside the *ofSwitchAP* directory needs to be changed to the appropriate values before compilation:
```C
#define SOCK_ADDR "127.0.0.1"    // Change "127.0.0.1" to Floodlight OpenFlow Controller's IP address
#define IFACE "wlan0"            // Change "wlan0" to the name of the wireless interface of the AP
#define NATIFACE "eth0"          // Change "eth0" to the name of the ethernet interface of the AP
#define NATSUBNET "192.168.5.0"  // Change "192.168.5.0" to the subnet which the NATIFACE belongs to
```
### Floodlight OpenFlow Controller
The directory *ofSwitchAP* was added inside the *Floodlightv0.90/floodlight/src/main/java/net/floodlightcontroller* directory. All the relevant FloodLight modules were added to the *ofSwitchAP* directory. Additionally, various exisitng files were also modified.
### Click Modular Router
The *PrintSignalStrength.cc* and *PrintSignalStrength.hh* files were added to the *click/elements/wifi* directory.
### Gateway
All the relevant files, required for the implementation of the **Gateway** were added to the *Gateway* directory. The following `#define` directives in the **gatewayNAT.c** file inside the *Gateway* directory needs to be changed to the appropriate values before compilation:
```C
#define SOCK_ADDR "127.0.0.1"    // Change "127.0.0.1" to Floodlight OpenFlow Controller's IP address
#define IFACE "eth0"             // Change "eth0" to the ethernet interface towards the APs
```
## Compilation
Kindly follow the compilation and installation guides for each of the softwares (hostapd, Floodlight OpenFlow Controller and Click Modular Router). Remember to clean the already compiled files first. 
### Gateway
For compiling the Gateway use the following commands:
```
$ make clean
$ make
```
## Configuration
### hostapd
A *.config* file is already created in the *hostapd-2.8/hostapd* directory. The file should not require any changes.

The *hostapd.conf* file contains all the configurations for hostapd. The **ssid** field denotes the SSID that will be created when hostapd will be run, and the **wep_key0** field is the password of the created SSID. 
### Click Modular Router
The **wifi** module in Click Modular Router needs to be enabled for the project to work. Use the following command inside the *click* directory:
```
$ ./configure --enable-wifi
```
## Execution
The Floodlight OpenFlow Controller needs to be started first, before running hostapd and/or Gateway. If hostapd and/or Gateway are/is started first, then it may result in *Segmentation Fault*.
### Floodlight OpenFlow Controller
For running Floodlight OpenFlow Controller, run the following command inside *Floodlightv0.90/floodlight* directory:
```
$ java -jar target/floodlight.jar
```
### hostapd
For running hostapd, run the following command inside *Gateway* directory:
```
$ sudo ./run_SDEW.sh <wireless interface> <ethernet interface>
```
For example, if the name of the wireless interface is **wlan0** and that of the ethernet interface is **eth0**, then the above command will be run as follows:
```
$ sudo ./run_SDEW.sh wlan0 eth0 
```
### Gateway
For running Gateway, run the following command inside *Gateway* directory:
```
$ sudo ./run_gw_nat.sh <ethernet interface towards the APs> <ethernet interface towards the remote host> <IP address of ethernet interface towards the APs>
```
For example if the name of the ethernet interface towards the APs is **eth0**, and the name of the ethernet interface towards the remote host is **eth1** and the IP address of **eth0** is 192.168.100.1, then the above command will be run as follows:
```
$ sudo ./run_gw_nat.sh eth0 eth1 192.168.100.1
```
