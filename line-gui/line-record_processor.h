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

#ifndef LINERECORD_PROCESSOR_H
#define LINERECORD_PROCESSOR_H

#include <QtCore>
#include "../tomo/readpacket.h"

typedef QPair<QString, QPair<quint16, quint16> > ProtoPortPair;
typedef QPair<qint32, qint32> NodePair;

class FlowPacket {
public:
	// matches RecordedPacketData::packet_id
	quint64 packetId;
	quint64 tsSent;
	// the packet reached the destination
	bool received;
	// the packet was dropped, implied received == false
	// Note: it IS possible to have received == false and dropped == false at the same time.
	bool dropped;
	// defined if dropped == true
	qint32 dropEdgeId;
	// defined if dropped == true
	quint64 tsDrop;
	// defined if received == true
	quint64 tsReceived;
	IPHeader ipHeader;
	TCPHeader tcpHeader;
	UDPHeader udpHeader;
};

class Flow {
public:
	Flow(quint16 sourcePort = 0, quint16 destPort = 0, QString protocolString = "");
	quint16 sourcePort;
	quint16 destPort;
	QString protocolString;
	// ordered by tsSent
	QList<FlowPacket> packets;
};

class Conversation {
public:
	Conversation(quint16 sourcePort = 0, quint16 destPort = 0, QString protocolString = "");
	// fwdFlow is the initiator, i.e. the node that sends the first packet
	Flow fwdFlow;
	Flow retFlow;
	// source and dest ports of the forward flow
	quint16 sourcePort;
	quint16 destPort;
	QString protocolString;
	// internal: bool finished; // FIN
};

class PathConversations {
public:
	PathConversations(qint32 sourceNodeId = -1, qint32 destNodeId = -1);
	qint32 sourceNodeId;
	qint32 destNodeId;
	QHash<ProtoPortPair, QList<Conversation> > conversations;
	// Bandwidth of the bottleneck link in B/s
	qreal maxPossibleBandwidthFwd;
	qreal maxPossibleBandwidthRet;
};

// Processes the captures recorded by the emulator (packet headers and queue events).
bool processLineRecord(QString rootPath, QString expPath, int &packetCount, int &queueEventCount);

#endif // LINERECORD_PROCESSOR_H
