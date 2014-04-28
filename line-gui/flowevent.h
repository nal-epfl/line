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

#ifndef FLOWEVENT_H
#define FLOWEVENT_H

#ifdef LINE_EMULATOR
extern "C" {
#include <pfring.h>
}
#endif

#include <QtCore>

#ifdef LINE_EMULATOR
class Packet;
#endif

class FlowEvent {
public:
	FlowEvent();

	// Timestamp for this event (stats cover this time + period, with period set to probably 1 second), ns
	quint64 tsEvent;
	// This is set only if there has been at least one packet that was forwarded successfully, otherwise it is zero, ns
	quint64 fordwardDelay;
	// The TCP variables are zero for non-TCP protocols.
	// If SYN is set, it marks the beginning of a new flow.
	quint8 tcpFlags;
	// Set to -1 if unknown (only packets with SYN set have a known value). Note that this is the exponent!
	qint8 tcpReceiveWindowScale;
	// Set to the size of the receive window (not scaled)
	quint16 tcpReceiveWindow;
	// Number of packets/bytes sent/dropped until the next event
	quint32 packetsTotal;
	quint32 packetsDropped;
	quint32 bytesTotal;
	quint32 bytesDropped;

	bool isTcpSyn();

#ifdef LINE_EMULATOR
	void handlePacket(Packet *p);
#endif
};
QDataStream& operator<<(QDataStream& s, const FlowEvent& d);
QDataStream& operator>>(QDataStream& s, FlowEvent& d);

class SampledFlowEvents {
public:
	SampledFlowEvents();

	// The timestamp the last sample was recorded for the current flow
	quint64 tsLastSample;

	// Flow events, sampled. Events that mark the beginning of a new flow (e.g. SYN flag set) should always be
	// recorded, regardless of sampling. This because the receive window scale factor is only present in packets
	// with the SYN flag set.
	QVector<FlowEvent> flowEvents;

#ifdef LINE_EMULATOR
	void handlePacket(Packet *p, quint64 tsNow);
#endif
};
QDataStream& operator<<(QDataStream& s, const SampledFlowEvents& d);
QDataStream& operator>>(QDataStream& s, SampledFlowEvents& d);

class SampledPathFlowEvents {
public:
	void initialize(int numPaths);

	// Index = path index
	// See encode/decode key below to understand what the key means
	QVector<QHash<quint64, SampledFlowEvents> > pathFlows;

	bool save(QString fileName);
	bool load(QString fileName);

	static void encodeKey(quint64 &key, const quint8 transportProtocol, const quint16 srcPort, const quint16 dstPort);
	static void decodeKey(const quint64 key, quint8 &transportProtocol, quint16 &srcPort, quint16 &dstPort);

#ifdef LINE_EMULATOR
	void handlePacket(Packet *p, quint64 tsNow);
#endif
};
QDataStream& operator<<(QDataStream& s, const SampledPathFlowEvents& d);
QDataStream& operator>>(QDataStream& s, SampledPathFlowEvents& d);

#endif // FLOWEVENT_H
