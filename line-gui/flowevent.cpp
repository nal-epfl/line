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

#include "flowevent.h"

#ifdef LINE_EMULATOR
#include "pconsumer.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#endif

FlowEvent::FlowEvent()
{
	tsEvent = 0;
	fordwardDelay = 0;
	tcpFlags = 0;
	tcpReceiveWindowScale = -1;
	tcpReceiveWindow = 0;
	packetsTotal = 0;
	packetsDropped = 0;
	bytesTotal = 0;
	bytesDropped = 0;
}

bool FlowEvent::isTcpSyn()
{
	return tcpFlags & 0x02;
}

SampledFlowEvents::SampledFlowEvents()
{
	tsLastSample = 0;
}

void SampledPathFlowEvents::initialize(int numPaths)
{
	pathFlows.resize(numPaths);
}

bool SampledPathFlowEvents::save(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << *this;

	if (out.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error writing file:" << file.fileName();
		return false;
	}
	return true;
}

bool SampledPathFlowEvents::load(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;

	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error reading file:" << file.fileName();
		return false;
	}
	return true;
}

void SampledPathFlowEvents::encodeKey(quint64 &key,
									  quint8 transportProtocol,
									  quint16 srcPort,
									  quint16 dstPort)
{
	key = transportProtocol;
	key <<= 16;
	key |= srcPort;
	key <<= 16;
	key |= dstPort;
}

void SampledPathFlowEvents::decodeKey(quint64 key,
									  quint8 &transportProtocol,
									  quint16 &srcPort,
									  quint16 &dstPort)
{
	dstPort = key & 0xFFFF;
	key >>= 16;
	srcPort = key & 0xFFFF;
	key >>= 16;
	transportProtocol = key & 0xFF;
}

#ifdef LINE_EMULATOR
void FlowEvent::handlePacket(Packet *p)
{
	packetsTotal++;
	bytesTotal += p->length;

	if (p->dropped) {
		packetsDropped++;
		bytesDropped += p->length;
	}

	// If at least one positive (forward) event has been recorded, exit, unless this marks a new flow
	if (fordwardDelay > 0) {
		return;
	}

	const struct tcphdr *tcp = (p->l4_protocol == IPPROTO_TCP) ?
								   (const struct tcphdr *)(p->buffer + p->offsets.l4_offset) :
								   NULL;

	if (!p->dropped) {
		fordwardDelay = p->theoretical_delay;
	}
	if (tcp) {
		tcpFlags = p->tcpFlags;
		tcpReceiveWindow = ntohs(tcp->window);
		if (tcp->syn) {
			// We need to parse the options to get the window scale
			const unsigned char *tcpopt = (const unsigned char*)(tcp + 1);
			unsigned tcpHeaderLength = 4*tcp->doff;
			if (tcpHeaderLength >= sizeof(struct tcphdr)) {
				const unsigned char *tcppayload = (const unsigned char*)(tcp) + tcpHeaderLength;
				int tcpoptlen = tcppayload - tcpopt;

				while (tcpoptlen > 0) {
					// Note that we are not implementing parsing all the possible TCP options, only the one that are
					// likely to be encountered in our experiments. If an unknown option is encountered, we abort.
					if (*tcpopt == TCPOPT_EOL) {
						// End of option list
						break;
					} else if (*tcpopt == TCPOPT_NOP) {
						// No operation
						tcpopt++, tcpoptlen = tcppayload - tcpopt;
					} else if (*tcpopt == TCPOPT_MAXSEG) {
						// Maximum segment size
						tcpopt += TCPOLEN_MAXSEG, tcpoptlen = tcppayload - tcpopt;
					} else if (*tcpopt == TCPOPT_WINDOW) {
						// Window scale factor
						if (tcpoptlen >= TCPOLEN_WINDOW) {
							tcpReceiveWindowScale = tcpopt[2];
							// This is all we wanted, so stop processing
							break;
						} else {
							break;
						}
						tcpopt += TCPOLEN_WINDOW, tcpoptlen = tcppayload - tcpopt;
					} else if (*tcpopt == TCPOPT_SACK_PERMITTED) {
						// Sack permitted
						tcpopt += TCPOLEN_SACK_PERMITTED, tcpoptlen = tcppayload - tcpopt;
					} else if (*tcpopt == TCPOPT_SACK) {
						// Sack
						if (tcpoptlen < 2) {
							break;
						}
						unsigned char length = tcpopt[1];
						if (length > tcpoptlen) {
							break;
						}
						tcpopt += length, tcpoptlen = tcppayload - tcpopt;
					} else if (*tcpopt == TCPOPT_TIMESTAMP) {
						// Timestamp
						tcpopt += TCPOLEN_TIMESTAMP, tcpoptlen = tcppayload - tcpopt;
					} else if (*tcpopt == 28) {
						// User timeout
						tcpopt += 4, tcpoptlen = tcppayload - tcpopt;
					} else {
						// not implemented
						break;
					}
				}
			}
		}
	}
}

void SampledFlowEvents::handlePacket(Packet *p, quint64 tsNow)
{
	// Points to a TCP structure if this is a TCP packet, or NULL otherwise
	const struct tcphdr *tcp = (p->l4_protocol == IPPROTO_TCP) ?
								   (const struct tcphdr *)(p->buffer + p->offsets.l4_offset) :
								   NULL;

	// Create a new event on periodic tick, no existing events or new TCP flow
	if ((flowEvents.isEmpty()) ||
		(tsNow >= tsLastSample + SEC_TO_NSEC) ||
		(tcp && tcp->syn)) {
		FlowEvent event;
		event.tsEvent = tsNow;
		flowEvents.append(event);
		tsLastSample = tsNow;
	}

	flowEvents.last().handlePacket(p);
}

void SampledPathFlowEvents::handlePacket(Packet *p, quint64 tsNow)
{
	if (p->path_id < 0 || p->path_id >= pathFlows.count()) {
		qDebug() << __FILE__ << __LINE__ << "Bad index!!!!";
		return;
	}

	if (DEBUG_PACKETS)
		printf("Handling packet %d.%d.%d.%d -> %d.%d.%d.%d, path %d, dropped %s\n",
			   NIPQUAD(p->src_ip),
			   NIPQUAD(p->dst_ip),
			   p->path_id,
			   p->dropped ? "true" : "false");

	quint64 key;
	encodeKey(key, p->l4_protocol, p->l4_src_port, p->l4_dst_port);
	pathFlows[p->path_id][key].handlePacket(p, tsNow);
}
#endif

QDataStream& operator<<(QDataStream& s, const FlowEvent& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.tsEvent;
	s << d.fordwardDelay;
	s << d.tcpFlags;
	s << d.tcpReceiveWindow;
	s << d.tcpReceiveWindowScale;
	s << d.packetsTotal;
	s << d.packetsDropped;
	s << d.bytesTotal;
	s << d.bytesDropped;

	return s;
}

QDataStream& operator>>(QDataStream& s, FlowEvent& d)
{
	qint32 ver;

	s >> ver;

	s >> d.tsEvent;
	s >> d.fordwardDelay;
	s >> d.tcpFlags;
	s >> d.tcpReceiveWindow;
	s >> d.tcpReceiveWindowScale;
	s >> d.packetsTotal;
	s >> d.packetsDropped;
	s >> d.bytesTotal;
	s >> d.bytesDropped;

	return s;
}

QDataStream& operator<<(QDataStream& s, const SampledFlowEvents& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.flowEvents;

	return s;
}

QDataStream& operator>>(QDataStream& s, SampledFlowEvents& d)
{
	qint32 ver;

	s >> ver;

	s >> d.flowEvents;

	return s;
}

QDataStream& operator<<(QDataStream& s, const SampledPathFlowEvents& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.pathFlows;

	return s;
}

QDataStream& operator>>(QDataStream& s, SampledPathFlowEvents& d)
{
	qint32 ver;

	s >> ver;

	s >> d.pathFlows;

	return s;
}
