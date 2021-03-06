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

#ifndef LINERECORD_H
#define LINERECORD_H

#include <QtCore>
#include "ovector.h"
#include "queuing-decisions.h"
#include "netgraphedge.h"

#ifdef LINE_EMULATOR
class Packet;
#endif

#define CAPTURE_LENGTH 92

// Stores a packet header from a capture.
class RecordedPacketData {
public:
	RecordedPacketData();
#ifdef LINE_EMULATOR
	RecordedPacketData(Packet *p);
#endif

    // Some unique ID.
	quint64 packet_id;
    // ID of source NetGraphNode
    qint32 src_id;
    // ID of destination NetGraphNode
    qint32 dst_id;
	// First CAPTURE_LENGTH bytes starting with the IP header, possibbly padded with zeros.
	quint8 buffer[CAPTURE_LENGTH];
    // Timestamp for the moment of the capture.
	quint64 ts_userspace_rx;

    // Returns true if nothing is stored.
	bool isNull();

	static qint64 getSerializedSize();
};
QDataStream& operator>>(QDataStream& s, RecordedPacketData& d);
QDataStream& operator<<(QDataStream& s, const RecordedPacketData& d);

// Stores data referring to the event of enqueuing a packet on a link.
class RecordedQueuedPacketData {
public:
	enum Decision {
		Queued = DECISION_QUEUE,
		QueueDrop = DECISION_QDROP,
		RandomDrop = DECISION_RDROP
	};

    // Unique packet ID, to be cross referenced with RecordedPacketData.packet_id.
    quint64 packet_id;
    // Edge ID from graph.
    qint32 edge_index;
    // Timestamp of the event, in ns.
    quint64 ts_enqueue;
    // Queue capacity (total storage space) in bytes.
    qint32 qcapacity;
    // Queue load (occupied space) in bytes.
    qint32 qload;
    // Integer encoding the decision for queuing the packet:
	// Value: see the Decision enum
    qint32 decision;
    // Timestamp for when the packet is removed from the queue (forwarding complete).
	// Defined only if decision == Queued
    quint64 ts_exit;

	static qint64 getSerializedSize();
};
QDataStream& operator>>(QDataStream& s, RecordedQueuedPacketData& d);
QDataStream& operator<<(QDataStream& s, const RecordedQueuedPacketData& d);

// Stores the packet headers and the queuing events for an experiment.
class RecordedData {
public:
	RecordedData();
	~RecordedData();
    // Flag that specifies if recording packets/events is enabled.
    // If false, no data will be stored.
	bool recordPackets;

	// If non-zero, a single packet is recoded per key (with key an arbitrary integer, e.g. a path index)
	// every samplingPeriod nanoseconds.
	// If zero, all packets are recorded.
	quint64 samplingPeriod;

	// If false, recording has been truncated due to memory limit.
	// Updated only during save() and load().
	bool saturated;

#ifdef LINE_EMULATOR
    // Packet headers. Never modify existing data in the vector, only append.
	OVector<RecordedPacketData, qint64> recordedPacketData;
    // Queuing events. Never modify existing data in the vector, only append.
	OVector<RecordedQueuedPacketData, qint64> recordedQueuedPacketData;

	// Key: arbitrary integer as used by line-router (maybe a path index).
	// Value: the last bin (packet timestamp/samplingPeriod) used for the key.
	QHash<int, quint64> lastSampleBin;

	bool save(QString fileName);
#else
	QFile *file;
	OLazyVector<RecordedPacketData, qint64> recordedPacketData;
	OLazyVector<RecordedQueuedPacketData, qint64> recordedQueuedPacketData;

	bool load(QString fileName);

	Q_DISABLE_COPY(RecordedData)
#endif


    // Lookup a packet by its unique ID.
	// This is slow at the first call, then very fast for subsequent calls
	// (for any packetID).
	RecordedPacketData packetByID(quint64 packetID);
	qint64 packetIndexByID(quint64 packetID);

	// Lookup a list of queuing events for a packet by its unique ID.
	// This is slow at the first call, then very fast for subsequent calls
	// (for any packetID).
	QList<RecordedQueuedPacketData> queueEventsByPacketID(quint64 packetID);

private:
	// index of packet in recordedPacketData
	QHash<quint64, qint64> packetID2Index;
	// list of queue events for a packet ID in recordedQueuedPacketData, in chronological order
	QHash<quint64, QList<RecordedQueuedPacketData> > packetID2QueueEvents;
};
QDataStream& operator>>(QDataStream& s, RecordedPacketData& d);
QDataStream& operator<<(QDataStream& s, const RecordedPacketData& d);

#endif // LINERECORD_H
