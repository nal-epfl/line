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

#ifndef NETGRAPHEDGE_H
#define NETGRAPHEDGE_H

#include <QtCore>

#include "netgraphcommon.h"
#include "queuing-decisions.h"
#include "util.h"

#define ETH_DATA_LEN 1500
// max. bytes in frame without FCS
#define ETH_FRAME_LEN 1514

// 10 is recommended by Juniper Networks. Determining Proper Burst Size for Traffic Policers. 2013
// https://www.juniper.net/techpubs/en_US/junos/topics/concept/policer-mx-m120-m320-burstsize-determining.html
// 2013
#define MIN_QUEUE_LEN_FRAMES 10

#ifndef LINE_EMULATOR
//#define LINE_EMULATOR
#endif
#ifdef LINE_EMULATOR
#include "../util/bitarray.h"
#include "../util/ovector.h"
#endif

#include <qrgb-line.h>

class Packet;
class NetGraph;

class FlowIdentifier {
public:
    FlowIdentifier(Packet *p = 0);
    quint32 ipSrc;
    quint32 ipDst;
    quint16 portSrc;
    quint16 portDst;
    quint32 protocol;
    bool operator==(const FlowIdentifier &other) const;
    bool operator!=(const FlowIdentifier &other) const;
};

uint qHash(const FlowIdentifier& object);

QDataStream& operator>>(QDataStream& s, FlowIdentifier& d);

QDataStream& operator<<(QDataStream& s, const FlowIdentifier& d);


class EdgeTimelineItem {
public:
    void clear();
    quint64      timestamp;
    quint64      arrivals_p;    // number of packet arrivals
    quint64      arrivals_B;    // ingress bytes
    quint64      qdrops_p;      // number of packets dropped by queueing
    quint64      qdrops_B;      // bytes dropped by queueing
    quint64      rdrops_p;      // number of packets dropped randomly
    quint64      rdrops_B;      // bytes dropped randomly
    quint64      queue_sampled; // queue utilization sampled at timestamp
    quint64      queue_avg;     // divide this by arrivals_p (if that is zero, this is zero) to get the average queue size, sampled at packet arrivals
    quint64      queue_max;     // the maximum queue size over this time interval
    quint64      numFlows;      // should be the same as flows.count() except for legacy experiments, which have flows empty but numFlows >= 0
    QSet<FlowIdentifier> flows; // set of the flows from which packets have arrived on the link
};

bool compareEdgeTimelineItem(const EdgeTimelineItem &a, const EdgeTimelineItem &b);

QDataStream& operator>>(QDataStream& s, EdgeTimelineItem& d);

QDataStream& operator<<(QDataStream& s, const EdgeTimelineItem& d);


class EdgeTimeline {
public:
    EdgeTimeline();
    qint32 edgeIndex;
    qint32 queueIndex; // -1 means this is the edge timeline, >= 0 means this is a queue timeline
    quint64 tsMin;
    quint64 tsMax;
    quint64 timelineSamplingPeriod;
    quint64 rate_Bps;
    qint32 delay_ms;
    quint64 qcapacity;  // queue size in bytes
    QVector<EdgeTimelineItem> items;
};

QDataStream& operator>>(QDataStream& s, EdgeTimeline& d);

QDataStream& operator<<(QDataStream& s, const EdgeTimeline& d);

class EdgeTimelines {
public:
    QVector<QVector<EdgeTimeline> > timelines;
};

QDataStream& operator>>(QDataStream& s, EdgeTimelines& d);

QDataStream& operator<<(QDataStream& s, const EdgeTimelines& d);

bool readEdgeTimelines(EdgeTimelines &d, NetGraph *g, QString workingDir);

#ifdef LINE_EMULATOR

class Packet;

struct packetEvent {
	quint64      timestamp;
	int          type;
};

bool comparePacketEvent(const packetEvent &a, const packetEvent &b);

class NetGraphEdge;

class QueueItem
{
public:
	Packet *packet;
	quint64 ts_exit;
	// index in recordedData->recordedQueuedPacketData
	// defined only if recordedData->recordPackets is true, and if
	// the item has been created
	int recordedQueuedPacketDataIndex;
};

enum QueuingDiscipline {
	QueuingDisciplineDropTail = 0,
	QueuingDisciplineDropHead = 1,
	QueuingDisciplineDropRand = 2
};

class NetGraphEdgeQueue
{
public:
    NetGraphEdgeQueue();
    NetGraphEdgeQueue(const NetGraphEdge &edge, qint32 index);

    qint32 edgeIndex;        // edge index in graph (0..e-1)
    qint32 queueIndex;       // queue index in edge

    qint32 delay_ms;         // propagation delay in ms
    qreal lossBernoulli;     // bernoulli loss rate (before queueing)
    qint32 queueLength;      // queue length in #Ethernet frames
    qreal bandwidth;         // bandwidth in KB/s

    qint32 queuingDiscipline;

    bool recordSampledTimeline;
    quint64 timelineSamplingPeriod; // nanoseconds
    bool recordFullTimeline;

    quint64 rate_Bps;        // link rate in bytes/s
    qint32 lossRate_int;     // packet loss rate (2^31-1 means 100% loss)

    // Queue
    quint64 qcapacity;     // queue size in bytes
    quint64 qload;         // how many bytes are used at time == qts_head
    quint64 qts_head;      // the timestamp at which the first byte begins transmitting
	OVector<QueueItem> queued_packets; // the packets in the queue, with some attributes
	OVector<Packet*> asyncDrains;

    // Statistics
    qint32 npaths;
    // Total number of packets that arrived on this link
    quint64 packets_in;
    // Total number of bytes that arrived on this link
    quint64 bytes;
    // Total number of packets dropped because of queuing
    quint64 qdrops;
    // Total number of packets dropped randomly.
    // Note: x in TomoData is computed as (e.packets_in - e.qdrops - e.rdrops) / (qreal)(e.packets_in).
    quint64 rdrops;
    // Total queueing delay
    quint64 total_qdelay;
    // Per path statistics.
	OVector<quint64> packets_in_perpath;
	OVector<quint64> qdrops_perpath;
	OVector<quint64> rdrops_perpath;
	OVector<quint64> bytes_in_perpath;
	OVector<quint64> qdelay_perpath;

    // Timeline
	OVector<packetEvent> timelineFull;
    OVector<EdgeTimelineItem> timelineSampled;
    quint64 tsMin;
    quint64 tsMax;

	void drain(quint64 ts_now, OVector<Packet*> &result);
    bool enqueue(Packet *p, quint64 ts_now, quint64 &ts_exit);
};

class TokenBucket {
public:
	TokenBucket();
	TokenBucket(const NetGraphEdge &edge, qint32 index);
	// Total capacity, i.e. allowed burstiness, in bytes.
	// Should be able to hold at least a few packets.
	qreal capacity;
	// Fill rate in bytes/nanosecond. Must be >= zero.
	qreal fillRate;

	// Initialize
	void init(quint64 ts_now);

	// Update the bucket for the current time
	void update(quint64 ts_now);

	// Returns true if the packet is accepted, false if dropped.
	bool filter(Packet *p, quint64 ts_now, bool forcePass = false);

	qint32 policerIndex;

	// Statistics
	// Total number of packets that arrived on this link
	quint64 packets_in;
	// Total number of bytes that arrived on this link
	quint64 bytes;
	// Total number of packets dropped because of policing
	quint64 drops;
	// Per path statistics.
	OVector<quint64> packets_in_perpath;
	OVector<quint64> bytes_in_perpath;
	OVector<quint64> drops_perpath;

	// Current level, in bytes. Initially, equal to the capacity.
	// The level is reduced each time a packet is forwarded.
	// If the level would drop below zero, the packet is dropped
	// and the level is not decreased.
	// The level increases by itself over time due to the fill rate,
	// and is upper bounded by the capacity.
	qreal currentLevel;

protected:
	// The timestamp for the last time the level was computed.
	quint64 tsLastUpdate;
};

#endif

class NetGraphEdge
{
public:
	NetGraphEdge();

	qint32 index;            // edge index in graph (0..e-1)
	qint32 source;           // index of the source node
	qint32 dest;             // index of the destination node

	qint32 delay_ms;         // propagation delay in ms
	qreal lossBernoulli;     // bernoulli loss rate (before queueing)
	qint32 queueLength;      // queue length in #Ethernet frames
	qreal bandwidth;         // bandwidth in KB/s (same as B/ms)

	QRgb getColor();
	QRgb color;
	qreal width;
	QString extraTooltip;

	bool used;

	bool recordSampledTimeline;
	quint64 timelineSamplingPeriod; // nanoseconds
	// TODO not supported by GUI
	bool recordFullTimeline;

	// Number of token buckets for different classes of traffic (by default 1).
	qint32 policerCount;
	// If policerWeights.count() == policerCount: each item represents a fraction
	// (0 <= item < 1) of the total bandwidth. Otherwise the bandwidth is distributed
	// evenly between policers.
	// The sum of all elements must be <= 1.0
	QVector<qreal> policerWeights;

    // Number of queues for different classes of traffic (by default 1).
    qint32 queueCount;
	// If queueWeights.count() == queueCount: each item represents a fraction
	// (0 <= item < 1) of the total bandwidth. Otherwise the bandwidth is distributed
	// evenly between queues.
	// The sum of all elements must be <= 1.0
	QVector<qreal> queueWeights;

    QString tooltip();    // shows bw, delay etc
    double metric();

	bool isNeutral();

#ifdef LINE_EMULATOR
	quint64 rate_Bps;        // link rate in bytes/s
    qint32 lossRate_int;     // packet loss rate (RAND_MAX-1 means 100% loss)

	// Queue
	quint64 qcapacity;     // queue size in bytes
	quint64 qload;         // how many bytes are used at time == qts_head
	quint64 qts_head;      // the timestamp at which the first byte begins transmitting

	// Statistics
    // Set by prepareEmulation(npaths).
    qint32 npaths;
    // Total number of packets that arrived on this link
    quint64 packets_in;
    // Total number of bytes that arrived on this link
    quint64 bytes;
    // Total number of packets dropped because of queuing
    quint64 qdrops;
    // Total number of packets dropped randomly.
    // Note: x in TomoData is computed as (e.packets_in - e.qdrops - e.rdrops) / (qreal)(e.packets_in).
    quint64 rdrops;
    // Total queueing delay
    quint64 total_qdelay;
    // Per path statistics.
	OVector<quint64> packets_in_perpath;
	OVector<quint64> qdrops_perpath;
	OVector<quint64> rdrops_perpath;
	OVector<quint64> bytes_in_perpath;
	OVector<quint64> qdelay_perpath;

	// Timeline
	OVector<packetEvent> timelineFull;
    OVector<EdgeTimelineItem> timelineSampled;
    quint64 tsMin;
    quint64 tsMax;

	// Token buckets are used for traffic policing. There is
	// always at least one bucket.
	OVector<TokenBucket> policers;
	bool hasPolicing;

	// Multiple queues are used for traffic shaping. There is
	// always at least one queue.
	OVector<NetGraphEdgeQueue> queues;

	void prepareEmulation(int npaths);
    void postEmulation();
	bool enqueue(Packet *p, quint64 ts_now, quint64 &ts_exit);
#endif

    bool operator==(const NetGraphEdge &other) const;

	QString toText();
};

inline uint qHash(const NetGraphEdge &e) {return qHash(e.index); }

QDataStream& operator>>(QDataStream& s, NetGraphEdge& e);

QDataStream& operator<<(QDataStream& s, const NetGraphEdge& e);

QDebug &operator<<(QDebug &stream, const NetGraphEdge &e);

#endif // NETGRAPHEDGE_H
