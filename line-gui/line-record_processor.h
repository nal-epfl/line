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
	// Index of the packet in RecordedData::recordedPacketData
	qint64 packetIndex;
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

	static qint64 getSerializedSize();
};
QDataStream& operator<<(QDataStream& s, const FlowPacket& d);
QDataStream& operator>>(QDataStream& s, FlowPacket& d);

class Flow {
public:
	Flow(quint16 sourcePort = 0, quint16 destPort = 0, QString protocolString = "");
	quint16 sourcePort;
	quint16 destPort;
	QString protocolString;
	// ordered by tsSent
	// value is an index in the list of FlowPacket stored in the flow-packets.data file
	QList<qint64> packets;
};
QDataStream& operator<<(QDataStream& s, const Flow& d);
QDataStream& operator>>(QDataStream& s, Flow& d);

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

	QList<Flow> flows();
};
QDataStream& operator<<(QDataStream& s, const Conversation& d);
QDataStream& operator>>(QDataStream& s, Conversation& d);

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
QDataStream& operator<<(QDataStream& s, const PathConversations& d);
QDataStream& operator>>(QDataStream& s, PathConversations& d);

// Processes the captures recorded by the emulator (packet headers and queue events).
// rootPath = path to the root of the repository
// expPath = path to the experiment directory
bool processLineRecord(QString rootPath, QString expPath, int &packetCount, int &queueEventCount);

bool postProcessLineRecord(QString expPath, QString srcDir, QString tag);

class NetGraph;
bool loadGraph(QString expPath, NetGraph &g);
int getOriginalPacketLength(QByteArray buffer);
quint32 getPacketFlow(QByteArray buffer, bool &freshFlow);
qint64 getSeqNo(QByteArray buffer, qint64 lastKnownSeqNo = -1);

QColor getLoadColor(qreal load, qreal loss);

#endif // LINERECORD_PROCESSOR_H
