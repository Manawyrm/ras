RAS
=============
RAS is a pure-software remote access server for ISDN (and similar) networks.
It can be integrated into the dialplan of a PBX like Yate and will handle any incoming data calls.

Features
--------
- PPP handling (networking)
- Telnet/Raw TCP forwarding

Supported protocols
--------
- HDLC with PPP ([RFC1618](https://datatracker.ietf.org/doc/html/rfc1618))  
RAS will then start pppd on the local machine and forward any frames to/from the ISDN line.
- [X.75]([https://www.itu.int/rec/dologin_pub.asp?lang=f&id=T-REC-X.75-199610-I!!PDF-E&type=items]), no V.42bis compression
- TTY/TDD ([ITU-T V.18](https://www.itu.int/rec/dologin_pub.asp?lang=s&id=T-REC-V.18-200011-I!!PDF-E&type=items), 5bit baudot, 45baud, 1400/1800 Hz), G.711a-encoded

Yate dialplan
--------
```js
// PPP dialup
Channel.callJust("external/playrec//opt/ras/src/yate_hdlc_ppp");
// X.75 telnet forwarding
Channel.callTo("external/playrec//opt/ras/src/yate_x75 tcpserver.de 23", {"callednr": callednr, "caller": caller, "format": format});
// TTY telnet forwarding
Channel.callTo("external/playrec//opt/ras/src/yate_tty ttyserver.de 23", {"callednr": callednr, "caller": caller, "format": format});
```

Configuration for HDLC/PPP
--------
`/etc/ppp/ip-up.d/isdn_cake` (don't forget chmod +x!)

```bash
#!/bin/bash
#       $1      the interface name used by pppd (e.g. ppp3)
#       $2      the tty device name
#       $3      the tty device speed
#       $4      the local IP address for the interface
#       $5      the remote IP address
#       $6      the parameter specified by the 'ipparam' option to pppd
tc qdisc add dev $1 root cake bandwidth 128kbit
```

`/etc/ppp/options.isdnras`:
```
require-mschap-v2
multilink
mtu 1500
ms-dns 8.8.8.8
ms-dns 8.8.4.4

debug
auth
passive
nodetach
```

at each boot:
```bash
sysctl -w net.ipv4.ip_forward=1
iptables -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --set-mss 500
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

Setting the interface MTU to 1500 will allow connected devices to send very large packets (up to the external internet/interface MTU).
MSS clamping to 500 bytes will limit the maximum window size of TCP connections to 500 bytes.
500 bytes at 64kBit/s (1x b-channel) takes about 62ms to transmit.
Transmitting full 1500 byte packets will take almost 187ms (or 10 yate buffers at 20ms each), which would congest the interface for those 187ms.
Configuring the smaller MSS will improve the user experience considerably, as small packets like TCP-ACK, DNS, ICMP can be prioritized by cake and sent over the line before any large TCP packets are sent. 

