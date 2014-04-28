/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "readpacket.h"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

QString ipProto2name(quint8 proto)
{
	if (proto == IPPROTO_ICMP)
		return "ICMP";
	if (proto == IPPROTO_TCP)
		return "TCP";
	if (proto == IPPROTO_UDP)
		return "UDP";
	return QString();
}

bool decodePacket(QByteArray buffer, IPHeader &iph, TCPHeader &tcph, UDPHeader &udph)
{
	if (buffer.count() < (int)sizeof(struct iphdr)) {
		return false;
	}

	const struct iphdr *ip = (const struct iphdr *)buffer.constData();

	iph.source[3] = (ip->saddr >> 24) & 0xFF;
	iph.source[2] = (ip->saddr >> 16) & 0xFF;
	iph.source[1] = (ip->saddr >> 8) & 0xFF;
	iph.source[0] = (ip->saddr >> 0) & 0xFF;
	iph.sourceString = QString("%1.%2.%3.%4").arg(iph.source[0]).arg(iph.source[1]).arg(iph.source[2]).arg(iph.source[3]);

	iph.dest[3] = (ip->daddr >> 24) & 0xFF;
	iph.dest[2] = (ip->daddr >> 16) & 0xFF;
	iph.dest[1] = (ip->daddr >> 8) & 0xFF;
	iph.dest[0] = (ip->daddr >> 0) & 0xFF;
	iph.destString = QString("%1.%2.%3.%4").arg(iph.dest[0]).arg(iph.dest[1]).arg(iph.dest[2]).arg(iph.dest[3]);

	iph.totalLength = ntohs(ip->tot_len);
	iph.headerLength = 4*ip->ihl;

	if (iph.headerLength > iph.totalLength) {
		return false;
	}

	iph.payloadLength = iph.totalLength - iph.headerLength;

	iph.id = ntohs(ip->id);

	iph.fragDontFragment = (ntohs(ip->frag_off) >> 14) & 1;
	iph.fragMoreFragments = (ntohs(ip->frag_off) >> 13) & 1;
	iph.fragDontFragment = (ntohs(ip->frag_off) & 0x1Fff)*8;

	iph.ttl = ip->ttl;

	iph.protocol = ip->protocol;
	iph.protocolString = ipProto2name(ip->protocol);

	const char *ip_payload = ((const char*)ip) + iph.headerLength;

	if (iph.protocolString == "TCP") {
		if (iph.payloadLength < (quint16)sizeof(tcphdr)) {
			return false;
		}
		const struct tcphdr *tcp = (const struct tcphdr *)ip_payload;

		tcph.sourcePort = ntohs(tcp->source);
		tcph.destPort = ntohs(tcp->dest);

		tcph.seq = ntohl(tcp->seq);
		tcph.ack = ntohl(tcp->ack_seq);

		tcph.flagSyn = tcp->syn;
		tcph.flagAck = tcp->ack;
		tcph.flagRst = tcp->rst;
		tcph.flagFin = tcp->fin;
		tcph.flagPsh = tcp->psh;
		tcph.flagUrg = tcp->urg;

		tcph.windowRaw = ntohs(tcp->window);

		tcph.sacks.clear();

		tcph.totalLength = iph.payloadLength;
		tcph.headerLength = 4*tcp->doff;

		if (tcph.headerLength < 20 || tcph.headerLength > iph.payloadLength) {
			return false;
		}

		tcph.payloadLength = tcph.totalLength - tcph.headerLength;
		tcph.windowScaleFactor = 0;
		tcph.mss = 0;

		const unsigned char *tcpopt = (const unsigned char*)(tcp + 1);
		const unsigned char *tcppayload = (const unsigned char*)(tcp) + tcph.headerLength;
		int tcpoptlen = tcppayload - tcpopt;

		while (tcpoptlen > 0) {
			if (*tcpopt == 0) {
				// End of option list
				break;
			} else if (*tcpopt == 1) {
				// No operation
				tcpopt++, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 2) {
				// Maximum segment size
				if (tcpoptlen < 4) {
					return false;
				}
				tcph.mss = ntohs(*((const quint16*)(tcpopt+2)));
				tcpopt += 4, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 3) {
				// Window scale factor
				if (tcpoptlen < 3) {
					return false;
				}
				tcph.windowScaleFactor = (1 << tcpopt[2]);
				tcpopt += 3, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 4) {
				// Sack permitted
				tcpopt += 2, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 5) {
				// Sack
				if (tcpoptlen < 2) {
					return false;
				}
				unsigned char length = tcpopt[1];
				if (length > tcpoptlen) {
					return false;
				}
				if (length >= 2) {
					unsigned char blockslength = length - 2;
					quint32 *block = (quint32 *)(tcpopt + 2);
					while (blockslength >= 8) {
						qint64 blockStart = ntohl(*block);
						block++, blockslength -= 4;
						qint64 blockEnd = ntohl(*block);
						block++, blockslength -= 4;
						tcph.sacks.append(QPair<qint64, qint64>(blockStart, blockEnd));
					}
				}
				tcpopt += length, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 6) {
				// Echo
				tcpopt += 6, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 7) {
				// Echo reply
				tcpopt += 6, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 8) {
				// Timestamp
				tcpopt += 10, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 9) {
				// Partial Order Connection permitted
				tcpopt += 2, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 10) {
				// Partial Order service profile
				tcpopt += 3, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 11) {
				// Connection Count
				tcpopt += 6, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 12) {
				// CC.NEW
				tcpopt += 6, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 13) {
				// CC.ECHO
				tcpopt += 6, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 14) {
				// Alternate checksum request
				tcpopt += 3, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 15) {
				// Alternate checksum data
				// not implemented
				break;
			} else if (*tcpopt == 16) {
				// Skeeter
				// not implemented
				break;
			} else if (*tcpopt == 17) {
				// Bubba
				// not implemented
				break;
			} else if (*tcpopt == 18) {
				// Trailer checksum
				tcpopt += 3, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 19) {
				// MD5 hash
				tcpopt += 18, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 20) {
				// not implemented
				break;
			} else if (*tcpopt == 21) {
				// SNACK
				break;
			} else if (*tcpopt == 22) {
				// not implemented
				break;
			} else if (*tcpopt == 23) {
				// not implemented
				break;
			} else if (*tcpopt == 24) {
				// not implemented
				break;
			} else if (*tcpopt == 25) {
				// not implemented
				break;
			} else if (*tcpopt == 26) {
				// not implemented
				break;
			} else if (*tcpopt == 27) {
				// Quick-start response
				tcpopt += 8, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 28) {
				// User timeout
				tcpopt += 4, tcpoptlen = tcppayload - tcpopt;
			} else if (*tcpopt == 29) {
				// not implemented
				break;
			} else if (*tcpopt == 30) {
				// not implemented
				break;
			} else {
				// not implemented
				break;
			}
		}
	} else if (iph.protocolString == "UDP") {
		if (iph.payloadLength < (quint16)sizeof(udphdr)) {
			return false;
		}
		const struct udphdr *udp = (const struct udphdr *)ip_payload;

		udph.sourcePort = ntohs(udp->source);
		udph.destPort = ntohs(udp->dest);

		udph.totalLength = iph.payloadLength;
		udph.headerLength = sizeof(struct udphdr);
		udph.payloadLength = udph.totalLength - udph.headerLength;
		return true;
	} else if (iph.protocolString == "ICMP") {
		// not implemented
		return false;
	} else {
		return false;
	}

	return true;
}

