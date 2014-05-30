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

#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/poll.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <net/ethernet.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pfring.h>

#include <QtCore>
#include <QtXml>

// This is defined in the .pro file
// #define USE_TC_MALLOC
#ifdef USE_TC_MALLOC
#include "google/malloc_extension.h"
#endif

#include "pscheduler.h"
#include "pconsumer.h"
#include "psender.h"
#include "qpairingheap.h"
#include "bitarray.h"
#include "../util/ovector.h"
#include "../util/util.h"
#include "../tomo/tomodata.h"

/// topology stuff

#define DEBUG_CALLS 0
#define DEBUG_FOREIGN_PACKETS 0
#define BUSY_WAITING 1

#define PACKET_EVENT_QUEUED  1
#define PACKET_EVENT_QDROP   2
#define PACKET_EVENT_RDROP   3
#define PACKET_EVENT_DDROP   4

#define QUEUEING_ECN_ENABLED 0

QosBufferScaling qosBufferScaling;

QueuingDiscipline gQueuingDiscipline;

// 1 means no bloat, 2 means double buffers, etc
// recommended 1 if you want to see some congestion
// Set by the parameter --scale_buffers, default: 1.0
qreal bufferBloatFactor;

// 1 for enabled, 0 for disabled
#define POLICING_ENABLED 1

// 1 for enabled, 0 for disabled
#define WFQ_ENABLED 1

NetGraph *netGraph;

void NetGraphEdge::prepareEmulation(int npaths)
{
    this->npaths = npaths;
	rate_Bps = 1000.0 * bandwidth;
	lossRate_int = (int) (RAND_MAX * lossBernoulli);
	queueLength = bufferBloatFactor * queueLength;
    qcapacity = queueLength * ETH_FRAME_LEN;

	qload = 0;
	qts_head = 0;

	packets_in = 0;
	bytes = 0;
	qdrops = 0;
	rdrops = 0;
	total_qdelay = 0;

	packets_in_perpath.resize(npaths);
	qdrops_perpath.resize(npaths);
	rdrops_perpath.resize(npaths);
	bytes_in_perpath.resize(npaths);
	qdelay_perpath.resize(npaths);

	if (recordSampledTimeline) {
        EdgeTimelineItem &current = timelineSampled.append();
		current.clear();

		quint64 ts_now = get_current_time();
		current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;

		// preallocate
		quint64 estimateSize = (estimatedDuration / timelineSamplingPeriod) * 1.1;
		timelineSampled.reserve(estimateSize);
	}

	if (recordFullTimeline) {
		// preallocate for some number of packets
		quint64 estimateSize = 10000;
		estimateSize = qMin(estimateSize, 100000ULL);
		timelineFull.reserve(estimateSize);
	}

    tsMin = ULLONG_MAX;
    tsMax = 0;

	// Create traffic policers
	if (!POLICING_ENABLED) {
		// A little brutal but it works
		policerCount = 1;
	}
	Q_ASSERT_FORCE(policerCount >= 1);
	policers.clear();
	for (int f = 0; f < policerCount; f++) {
		policers.append(TokenBucket(*this, f));
	}
	if (policerCount == 1 && policerWeights[0] == 1.0) {
		hasPolicing = false;
	} else {
		hasPolicing = true;
	}

	// Create traffic shapers
    if (!WFQ_ENABLED) {
        // A little brutal but it works
        queueCount = 1;
    }
    Q_ASSERT_FORCE(queueCount >= 1);
    queues.clear();
    for (int q = 0; q < queueCount; q++) {
        queues.append(NetGraphEdgeQueue(*this, q));
    }
}

void NetGraphEdge::postEmulation()
{
	for (int f = 0; f < policers.count(); f++) {
		packets_in += policers[f].packets_in;
		bytes += policers[f].bytes;
		qdrops += policers[f].drops;
		for (int p = 0; p < npaths; p++) {
			packets_in_perpath[p] += policers[f].packets_in_perpath[p];
			bytes_in_perpath[p] += policers[f].bytes_in_perpath[p];
			qdrops_perpath[p] += policers[f].drops_perpath[p];
		}
	}
    for (int q = 0; q < queues.count(); q++) {
        qdrops += queues[q].qdrops;
        rdrops += queues[q].rdrops;
        total_qdelay += queues[q].total_qdelay;
        for (int p = 0; p < npaths; p++) {
            qdrops_perpath[p] += queues[q].qdrops_perpath[p];
            rdrops_perpath[p] += queues[q].rdrops_perpath[p];
            qdelay_perpath[p] += queues[q].qdelay_perpath[p];
        }
		// timelineFull.append(queues[q].timelineFull);
		// timelineSampled.append(queues[q].timelineSampled);
        tsMin = qMin(tsMin, queues[q].tsMin);
        tsMax = qMax(tsMax, queues[q].tsMax);
    }
    qSort(timelineFull.begin(), timelineFull.end(), comparePacketEvent);
    qSort(timelineSampled.begin(), timelineSampled.end(), compareEdgeTimelineItem);
}

NetGraphEdgeQueue::NetGraphEdgeQueue()
{
}

NetGraphEdgeQueue::NetGraphEdgeQueue(const NetGraphEdge &edge, qint32 index)
{
    edgeIndex = edge.index;
    queueIndex = index;

    delay_ms = edge.delay_ms;
    lossBernoulli = edge.lossBernoulli;
	qreal weight = 1.0;
	if (edge.queueWeights.count() == edge.queueCount) {
		qreal sum = 0;
		{
			foreach (qreal item, edge.queueWeights) {
				sum += item;
			}
			if (sum >= 1.001) {
				qDebug() << __FILE__ << __LINE__ << "Sum of queue weights higher than 1.0:" << sum;
			}
		}
		weight = edge.queueWeights[index] / qMax(1.0, sum);
    } else {
		if (edge.queueCount > 1) {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ <<
						"Using default queuing weigths for non-neutral link" <<
						(edge.index + 1);
		}
		weight = 1.0 / edge.queueCount;
    }
	queueLength = edge.queueLength;
#if 0
	if (qosBufferScaling == QosBufferScalingNone) {
		// nothing to do
	} else if (qosBufferScaling == QosBufferScalingDown) {
		queueLength *= weight;
	} else if (qosBufferScaling == QosBufferScalingUp) {
		queueLength /= weight;
	} else {
		Q_ASSERT_FORCE(false);
	}
#else
    queueLength *= weight;
#endif

	qcapacity = queueLength * ETH_FRAME_LEN;
	bandwidth = edge.bandwidth * weight;
	rate_Bps = edge.rate_Bps * weight;
	lossRate_int = edge.lossRate_int;
	queuingDiscipline = gQueuingDiscipline;

	queued_packets.reserve(qcapacity / 64);
	asyncDrains.reserve(qcapacity / 64);

    npaths = edge.npaths;
    recordSampledTimeline = edge.recordSampledTimeline;
    timelineSamplingPeriod = edge.timelineSamplingPeriod;
    recordFullTimeline = edge.recordFullTimeline;

    qload = 0;
    qts_head = 0;

    packets_in = 0;
    bytes = 0;
    qdrops = 0;
    rdrops = 0;
    total_qdelay = 0;

    packets_in_perpath.resize(npaths);
    qdrops_perpath.resize(npaths);
    rdrops_perpath.resize(npaths);
    bytes_in_perpath.resize(npaths);
    qdelay_perpath.resize(npaths);

    if (recordSampledTimeline) {
        EdgeTimelineItem current;
		current.clear();

        quint64 ts_now = get_current_time();
        current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;
		timelineSampled.append(current);

		// preallocate
		quint64 estimateSize = (estimatedDuration / timelineSamplingPeriod) * 1.1;
		timelineSampled.reserve(estimateSize);
    }

    if (recordFullTimeline) {
        // preallocate for some number of packets
        quint64 estimateSize = 10000;
        estimateSize = qMin(estimateSize, 100000ULL);
		timelineFull.reserve(estimateSize);
    }

    tsMin = ULLONG_MAX;
    tsMax = 0;
}

void pathTimelineItem::clear()
{
	timestamp = 0;
	arrivals_p = 0;
	arrivals_B = 0;
	exits_p = 0;
	exits_B = 0;
	drops_p = 0;
	drops_B = 0;
	delay_total = 0;
	delay_max = 0;
	delay_min = 0;
}

void NetGraphPath::prepareEmulation()
{
	packets_in = 0;
	packets_out = 0;
	bytes_in = 0;
	bytes_out = 0;
	total_theor_delay = 0;
	total_actual_delay = 0;

	if (recordSampledTimeline) {
		pathTimelineItem current;
		current.clear();

		quint64 ts_now = get_current_time();
		current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;
		timelineSampled.append(current);

		// preallocate for 60 seconds (but not more than some limit)
		quint64 estimateSize = (estimatedDuration / timelineSamplingPeriod) * 1.1;
		timelineSampled.reserve(estimateSize);
	}
}

void NetGraph::prepareEmulation()
{
    flattenConnections();
    assignPorts();

	edgeCache.clear();
	for (int i = 0; i < edges.count(); i++) {
		edges[i].prepareEmulation(paths.count());
		edgeCache.insert(QPair<qint32,qint32>(edges[i].source, edges[i].dest), i);
	}

	pathCache.clear();
	for (int i = 0; i < paths.count(); i++) {
		paths[i].prepareEmulation();
		pathCache.insert(QPair<qint32,qint32>(paths[i].source, paths[i].dest), i);
	}

	destID2Index.resize(nodes.count());
	QList<NetGraphNode> hosts = getHostNodes();
	for (int i = 0; i < hosts.count(); i++) {
		destID2Index[hosts[i].index] = i;
	}

	loadBalancedRouteCache.clear();

	routeCache.clear();
	routeCache.resize(nodes.count());
	for (int n = 0; n < nodes.count(); n++) {
		routeCache[n].resize(hosts.count());
		for (int i = 0; i < hosts.count(); i++) {
			int h = hosts[i].index;
			QList<int> nextHops = getNextHop(n, h);
			if (nextHops.isEmpty()) {
				routeCache[n][i] = NO_ROUTE;
			} else if (nextHops.count() == 1) {
				routeCache[n][i] = nextHops.first();
			} else {
				routeCache[n][i] = LOAD_BALANCED_ROUTE_MASK | loadBalancedRouteCache.count();
				loadBalancedRouteCache.append(nextHops.toVector());
			}
		}
	}
}

void loadTopology(QString graphFileName)
{
	netGraph = new NetGraph();
	netGraph->setFileName(graphFileName);
	netGraph->loadFromFile();
	netGraph->prepareEmulation();
}

TokenBucket::TokenBucket()
{
	tsLastUpdate = 0;
	capacity = 0;
	fillRate = 0;
	policerIndex = 0;
	packets_in = 0;
	bytes = 0;
	drops = 0;
}

TokenBucket::TokenBucket(const NetGraphEdge &edge, qint32 index)
{
	tsLastUpdate = 0;
	packets_in = 0;
	bytes = 0;
	drops = 0;

	policerIndex = index;

	qreal weight = 1.0;
	if (edge.policerWeights.count() == edge.policerCount) {
		{
			qreal sum = 0;
			foreach (qreal item, edge.policerWeights) {
				sum += item;
			}
			if (sum >= 1.001) {
				qDebug() << __FILE__ << __LINE__ << "Sum of token bucket weights higher than 1.0:" << sum;
			}
		}
		weight = edge.policerWeights[index];
	} else {
		if (edge.policerCount > 1) {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ <<
						"Using default policing weigths for non-neutral link" <<
						(edge.index + 1);
		}
		weight = 1.0 / edge.queueCount;
	}
	capacity = ETH_FRAME_LEN * edge.queueLength;

#if 0
	// Scale buffers if configured
	if (qosBufferScaling == QosBufferScalingNone) {
		// nothing to do
	} else if (qosBufferScaling == QosBufferScalingDown) {
		capacity *= weight;
	} else if (qosBufferScaling == QosBufferScalingUp) {
		capacity /= weight;
	} else {
		Q_ASSERT_FORCE(false);
	}
#endif

	fillRate = (edge.bandwidth / qreal(MSEC_TO_NSEC)) * weight;

	currentLevel = capacity;

	packets_in_perpath.resize(edge.npaths);
	bytes_in_perpath.resize(edge.npaths);
	drops_perpath.resize(edge.npaths);
}

void TokenBucket::init(quint64 ts_now)
{
	currentLevel = capacity;
	tsLastUpdate = ts_now;
}

void TokenBucket::update(quint64 ts_now)
{
	Q_ASSERT_FORCE(ts_now >= tsLastUpdate);
	if (tsLastUpdate == 0) {
		init(ts_now);
	} else {
		quint64 tsDelta = ts_now - tsLastUpdate;
		currentLevel += tsDelta * fillRate;
		currentLevel = qMin(currentLevel, capacity);
	}
	tsLastUpdate = ts_now;
}

bool TokenBucket::filter(Packet *p, quint64 ts_now, bool forcePass)
{
	Q_UNUSED(ts_now);
#if POLICING_ENABLED
	// Update the bucket for the current time
	update(ts_now);
#endif

	packets_in++;
	bytes += p->length;

	packets_in_perpath[p->path_id]++;
	bytes_in_perpath[p->path_id] += p->length;

#if POLICING_ENABLED
	if (forcePass)
		return true;
	// Check if the packet passes
	if (currentLevel >= p->length) {
		currentLevel -= p->length;
		return true;
	} else {
		drops++;
		drops_perpath[p->path_id]++;
		return false;
	}
#else
	return true;
#endif
}

void NetGraphEdgeQueue::drain(quint64 ts_now, OVector<Packet *> &result)
{
	for (int i = 0; i < asyncDrains.count(); i++) {
		Packet *p = asyncDrains[i];
		if (p->queue_id == this->edgeIndex) {
			result.append(p);
		}
	}
	asyncDrains.clear();
	while (!queued_packets.isEmpty()) {
		if (queued_packets.first().ts_exit <= ts_now) {
			Packet *p = queued_packets.first().packet;
			queued_packets.remove(0);
			result.append(p);
		} else {
			break;
		}
	}
}

bool NetGraphEdgeQueue::enqueue(Packet *p, quint64 ts_now, quint64 &ts_exit)
{
	int decision = DECISION_QUEUE;
	quint64 qdelay;
	int randomVal;
	int queuedIndex = -1;
	bool droppedOther = false;

	// update the link ingress stats
	packets_in++;
	packets_in_perpath[p->path_id]++;
	bytes += p->length;
	bytes_in_perpath[p->path_id] += p->length;

	if (ts_now < qts_head) {
		// This should never happen
		if (DEBUG_PACKETS) {
            printf("Moving clock forward by %s ns\n",
                   withCommas(qts_head - ts_now));
		}
		ts_now = qts_head;
	}

	// update the queue
	if (qload > 0) {
		quint64 delta_t = ts_now - qts_head;
		// how many bytes were transmitted during delta_t
		quint64 delta_B = (delta_t * rate_Bps) / SEC_TO_NSEC;
		delta_B = qMin(delta_B, qload);
		qload -= delta_B;
	}
	while (!queued_packets.isEmpty()) {
		quint64 ts_expected_exit = queued_packets.first().ts_exit;
		if (ts_expected_exit <= ts_now) {
			asyncDrains.append(queued_packets.first().packet);
			queued_packets.remove(0);
		} else {
			break;
		}
	}

    //printf("%s: ts delta = + "TS_FORMAT"  Link %d: qload = %llu (%llu%%)\n", p->trace.count() == 1 ? "ARRIVAL" : "EVENT  ", TS_FORMAT_PARAM(ts_now - qts_head), id, qload, (qload*100)/qcapacity);

	qts_head = ts_now;

	// random drop?
	randomVal = rand();
	if (lossRate_int > 0 && randomVal < lossRate_int) {
		rdrops++;
		rdrops_perpath[p->path_id]++;
		if (DEBUG_PACKETS)
            printf("Link: Drop: %d.%d.%d.%d -> %d.%d.%d.%d: lossRate_int = %d, randomVal = %d\n",
				   NIPQUAD(p->src_ip),
				   NIPQUAD(p->dst_ip),
				   lossRate_int,
				   randomVal);
		decision = DECISION_RDROP;
		p->dropped = true;
		p->ts_send = ts_now;
		goto stats;
	}

	// queue drop?
	if (qcapacity - qload < (quint64) p->length) {
		bool kept = false;
		if (queuingDiscipline == QueuingDisciplineDropHead && queued_packets.count() > 1) {
			for (int i = 1; i < qMin(3, queued_packets.count()); i++) {
				Packet *p_front = queued_packets[i].packet;
				p_front->dropped = true;
				p_front->ts_send = ts_now;
				asyncDrains.append(p_front);
				qload -= p_front->length;
				qdrops++;
				qdrops_perpath[p_front->path_id]++;
				if (DEBUG_PACKETS)
                    printf("Link: Drop: %d.%d.%d.%d -> %d.%d.%d.%d: plen = %d, qload = %llu, qcap = %llu\n",
						   NIPQUAD(p_front->src_ip),
						   NIPQUAD(p_front->dst_ip),
						   p_front->length,
						   qload,
						   qcapacity);
				kept = true;
				queuedIndex = i;
				droppedOther = true;
				break;
			}
		} else if (queuingDiscipline == QueuingDisciplineDropRand && queued_packets.count() > 1) {
			for (int iter = 0; iter < 3; iter++) {
				int i = 1 + rand() % (queued_packets.count() - 1);
				Packet *p_front = queued_packets[i].packet;
				p_front->dropped = true;
				p_front->ts_send = ts_now;
				asyncDrains.append(p_front);
				qload -= p_front->length;
				qdrops++;
				qdrops_perpath[p_front->path_id]++;
				if (DEBUG_PACKETS)
                    printf("Link: Drop: %d.%d.%d.%d -> %d.%d.%d.%d: plen = %d, qload = %llu, qcap = %llu\n",
						   NIPQUAD(p_front->src_ip),
						   NIPQUAD(p_front->dst_ip),
						   p_front->length,
						   qload,
						   qcapacity);
				kept = true;
				queuedIndex = i;
				droppedOther = true;
				break;
			}
		}
		if (!kept) {
			qdrops++;
			qdrops_perpath[p->path_id]++;
			if (DEBUG_PACKETS)
                printf("Link: Drop: %d.%d.%d.%d -> %d.%d.%d.%d: plen = %d, qload = %llu, qcap = %llu\n",
					   NIPQUAD(p->src_ip),
					   NIPQUAD(p->dst_ip),
					   p->length,
					   qload,
					   qcapacity);
			decision = DECISION_QDROP;
			p->dropped = true;
			p->ts_send = ts_now;
			goto stats;
		}
	}

	// we are enqueuing this packet
	qload += p->length;

	// add transmission delay
	qdelay = (qload * SEC_TO_NSEC) / rate_Bps;
	ts_exit = qts_head + qdelay;
	total_qdelay += qdelay;
	qdelay_perpath[p->path_id] += qdelay;

	if (DEBUG_PACKETS) {
		printf("Queuing delay: %s ns\n", withCommas(qdelay));
	}

	// add propagation delay
	ts_exit += delay_ms * MSEC_TO_NSEC;

	if (DEBUG_PACKETS) {
		printf("Propagation delay: %s ns\n", withCommas(delay_ms * MSEC_TO_NSEC));
	}

	p->theoretical_delay += ts_exit - ts_now;

	p->ts_expected_exit = ts_exit;
	p->queue_id = edgeIndex;
	if (queuedIndex >= 0) {
		if (queued_packets[queuedIndex].recordedQueuedPacketDataIndex >= 0) {
			// Update recorded data
			recordedData->recordedQueuedPacketData[queued_packets[queuedIndex].recordedQueuedPacketDataIndex].decision = DECISION_QDROP;
		}
		queued_packets.remove(queuedIndex);
		queuedIndex = -1;
	}
	{
		QueueItem queueItem;
		queueItem.packet = p;
		queueItem.ts_exit = p->ts_expected_exit;
		queueItem.recordedQueuedPacketDataIndex = -1;
		queued_packets.append(queueItem);
	}
	if (QUEUEING_ECN_ENABLED) {
		if (qload > qcapacity / 2) {
			p->ecn_bit_set = true;
		}
	}

	if (DEBUG_PACKETS) {
        printf("Total delay: %s ns\n", withCommas(p->theoretical_delay));
	}

stats:

	if (DEBUG_PACKETS) {
		printf("Queuing finished. Queue load: %llu/%llu (%llu%%)\n",
			   qload,
			   qcapacity,
			   (qload * 100 / qcapacity));
		printf("\n");
	}

	// update stats
    if (tsMin == ULLONG_MAX) {
        tsMin = ts_now;
    }
    tsMax = ts_now;
	if (recordedData->recordPackets &&
		recordedData->recordedQueuedPacketData.count() < recordedData->recordedQueuedPacketData.capacity()) {
		RecordedQueuedPacketData recordedQueuedPacketData;
		recordedQueuedPacketData.packet_id = p->id;
        recordedQueuedPacketData.edge_index = edgeIndex;
		recordedQueuedPacketData.ts_enqueue = ts_now;
		recordedQueuedPacketData.qcapacity = qcapacity;
		recordedQueuedPacketData.qload = qload;
		recordedQueuedPacketData.decision = decision;
		recordedQueuedPacketData.ts_exit = ts_exit;
		recordedData->recordedQueuedPacketData.append(recordedQueuedPacketData);
		queued_packets.last().recordedQueuedPacketDataIndex = recordedData->recordedQueuedPacketData.count() - 1;
	}
	if (recordSampledTimeline) {
		if (ts_now >= timelineSampled.last().timestamp + timelineSamplingPeriod) {
			// new time bracket, insert new aggregate
            EdgeTimelineItem &current = timelineSampled.append();
			current.clear();

			current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;
			current.queue_sampled = qload;
		}
		// we're in the same time bracket, update the last item
		timelineSampled.last().arrivals_p++;
		timelineSampled.last().arrivals_B += p->length;
		if (decision == DECISION_QDROP || droppedOther) {
			timelineSampled.last().qdrops_p++;
			timelineSampled.last().qdrops_B += p->length;
		}
		if (decision == DECISION_RDROP) {
			timelineSampled.last().rdrops_p++;
			timelineSampled.last().rdrops_B += p->length;
		}
		timelineSampled.last().queue_avg += qload;
		timelineSampled.last().queue_max = qMax(timelineSampled.last().queue_max, qload);
		if (flowTracking) {
			FlowIdentifier flow(p);
			timelineSampled.last().flows.insert(flow);
		}
	}

	if (recordFullTimeline) {
		packetEvent current;
		current.timestamp = ts_now;
		current.type = (decision == DECISION_QUEUE) ? PACKET_EVENT_QUEUED :
													  (decision == DECISION_QDROP) ? PACKET_EVENT_QDROP :
																					 PACKET_EVENT_RDROP;
		timelineFull.append(current);
		if (decision == DECISION_QUEUE && droppedOther) {
			current.type = PACKET_EVENT_DDROP;
			timelineFull.append(current);
		}
	}

	Q_ASSERT_FORCE(qload <= qcapacity);

	// return true if queued, false if dropped
	return (decision == DECISION_QUEUE);
}

/**
 * Enqueue a packet on a link.
 *
 * @p : the packet to be enqueued
 * @ts_now : the current time in ns
 * @ts_exit : on successful enqueuing, the time at which the packet ends transmission
 *
 * Returns true if the packet was enqueued, false if it was dropped.
*/

bool NetGraphEdge::enqueue(Packet *p, quint64 ts_now, quint64 &ts_exit)
{
	qint32 policerIndex = qMin(policerCount - 1, qMax(0, p->traffic_class));
	qint32 queueIndex = qMin(queueCount - 1, qMax(0, p->traffic_class));
	bool accepted = policers[policerIndex].filter(p, ts_now, !hasPolicing);

	bool queued;
	if (accepted) {
		queued = queues[queueIndex].enqueue(p, ts_now, ts_exit);
	} else {
		queued = false;
		p->dropped = true;
		// Add the packet to the capture
		if (recordedData->recordPackets &&
			recordedData->recordedQueuedPacketData.count() < recordedData->recordedQueuedPacketData.capacity()) {
			RecordedQueuedPacketData recordedQueuedPacketData;
			recordedQueuedPacketData.packet_id = p->id;
			recordedQueuedPacketData.edge_index = index;
			recordedQueuedPacketData.ts_enqueue = ts_now;
			recordedQueuedPacketData.qcapacity = queues[queueIndex].qcapacity;
			recordedQueuedPacketData.qload = queues[queueIndex].qload;
			recordedQueuedPacketData.decision = DECISION_QDROP;
			recordedQueuedPacketData.ts_exit = 0;
			recordedData->recordedQueuedPacketData.append(recordedQueuedPacketData);
		}
	}

    pathIntervalMeasurements->countPacketInFLightEdge(this->index, p->path_id, ts_now, ts_exit, p->length, 1);
    flowIntervalMeasurements->countPacketInFLightEdge(this->index, p->connection_index, ts_now, ts_exit, p->length, 1);
    // It is currently possible to have correct per-edge event recording only for tail-drop.
    // For disciplines that produce async drops (such as random-drop or drop-head), we cannot track the delayed drops.
    pathIntervalMeasurements->recordPacketEventEdge(this->index, p->path_id, ts_now, ts_exit, p->length, 1, queued);
    flowIntervalMeasurements->recordPacketEventEdge(this->index, p->connection_index, ts_now, ts_exit, p->length, 1, queued);
	if (!queued) {
        pathIntervalMeasurements->countPacketInFLightPath(p->path_id, p->ts_start_proc, ts_now, p->length, 1);
        flowIntervalMeasurements->countPacketInFLightPath(p->connection_index, p->ts_start_proc, ts_now, p->length, 1);
        pathIntervalMeasurements->countPacketDropped(this->index, p->path_id, p->ts_start_proc, ts_now, p->length, 1);
        flowIntervalMeasurements->countPacketDropped(this->index, p->connection_index, p->ts_start_proc, ts_now, p->length, 1);
        pathIntervalMeasurements->recordPacketEventPath(p->path_id, p->ts_start_proc, ts_now, p->length, 1, queued);
        flowIntervalMeasurements->recordPacketEventPath(p->connection_index, p->ts_start_proc, ts_now, p->length, 1, queued);
        if (flowTracking) {
            sampledPathFlowEvents->handlePacket(p, ts_now);
        }
	}

	if (recordSampledTimeline) {
		quint64 overallQload = 0;
		for (int iq = 0; iq < queueCount; iq++) {
			overallQload += queues[iq].qload;
		}
		if (ts_now >= timelineSampled.last().timestamp + timelineSamplingPeriod) {
			// new time bracket, insert new aggregate
            EdgeTimelineItem &current = timelineSampled.append();
			current.clear();

			current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;
			current.queue_sampled = overallQload;
		}
		// we're in the same time bracket, update the last item
		timelineSampled.last().arrivals_p++;
		timelineSampled.last().arrivals_B += p->length;
		if (!queued) {
			timelineSampled.last().qdrops_p++;
			timelineSampled.last().qdrops_B += p->length;
		}
		timelineSampled.last().queue_avg += overallQload;
		timelineSampled.last().queue_max = qMax(timelineSampled.last().queue_max, overallQload);
		if (flowTracking) {
			FlowIdentifier flow(p);
			timelineSampled.last().flows.insert(flow);
		}
	}

	if (recordFullTimeline) {
		packetEvent current;
		current.timestamp = ts_now;
		current.type = queued ? PACKET_EVENT_QUEUED : PACKET_EVENT_QDROP;
		timelineFull.append(current);
	}

	return queued;
}

#define PKT_QUEUED    0
#define PKT_DROPPED   1
#define PKT_FORWARDED 2

#define BYPASS_QUEUES 0
#define BYPASS_SCHEDULER 0
int routePacket(Packet *p, quint64 ts_now, quint64 &ts_next)
{
	if (p->path_id < 0) {
		p->path_id = netGraph->pathCache[QPair<qint32,qint32>(p->src_id, p->dst_id)];
	}

	NetGraphPath &path = netGraph->paths[p->path_id];

	// is this a new packet?
	if (p->trace.isEmpty()) {
		// yes, update path ingress stats
		p->trace.append(p->src_id);
		if (DEBUG_PACKETS)
			printf("New packet %d.%d.%d.%d -> %d.%d.%d.%d\n",
				   NIPQUAD(p->src_ip),
				   NIPQUAD(p->dst_ip));

		path.packets_in++;
		path.bytes_in += p->length;

		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem &current = path.timelineSampled.append();
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
			}
			path.timelineSampled.last().arrivals_p++;
			path.timelineSampled.last().arrivals_B += p->length;
		}
	} else {
#if BYPASS_QUEUES
		p->ts_start_send = ts_now;
		return PKT_FORWARDED;
#endif
	}

#if BYPASS_QUEUES
	ts_next = ts_now + 50ULL * MSEC_TO_NSEC;
	p->theoretical_delay = 50ULL * MSEC_TO_NSEC;
	return PKT_QUEUED;
#endif

    // did it reach the destination?
	if (p->trace.last() == p->dst_id) {
		// yes, forward the packet
		p->ts_start_send = ts_now;
		if (DEBUG_PACKETS)
			printf("Forwarding packet %d.%d.%d.%d -> %d.%d.%d.%d\n",
				   NIPQUAD(p->src_ip),
				   NIPQUAD(p->dst_ip));

		// update path egress stats
		path.packets_out++;
		path.bytes_out += p->length;
		path.total_theor_delay += p->theoretical_delay;
		path.total_actual_delay += p->ts_start_send - p->ts_userspace_rx;
		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem &current = path.timelineSampled.append();
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
				current.delay_min = ULLONG_MAX;
			}
			path.timelineSampled.last().exits_p++;
			path.timelineSampled.last().exits_B += p->length;
			path.timelineSampled.last().delay_total += p->theoretical_delay;
			path.timelineSampled.last().delay_max = qMax(path.timelineSampled.last().delay_max, p->theoretical_delay);
			path.timelineSampled.last().delay_min = qMin(path.timelineSampled.last().delay_min, p->theoretical_delay);
		}
        if (flowTracking) {
            sampledPathFlowEvents->handlePacket(p, ts_now);
        }

        pathIntervalMeasurements->countPacketInFLightPath(p->path_id, p->ts_start_proc, ts_now, p->length, 1);
        flowIntervalMeasurements->countPacketInFLightPath(p->connection_index, p->ts_start_proc, ts_now, p->length, 1);
        pathIntervalMeasurements->recordPacketEventPath(p->path_id, p->ts_start_proc, ts_now, p->length, 1, true);
        flowIntervalMeasurements->recordPacketEventPath(p->connection_index, p->ts_start_proc, ts_now, p->length, 1, true);
		return PKT_FORWARDED;
	}

	// was the packet dropped in the meantime?
	if (p->dropped) {
		// packet dropped, update path stats
		if (DEBUG_PACKETS)
			printf("Random drop for packet %d.%d.%d.%d -> %d.%d.%d.%d, node=%d\n",
				   NIPQUAD(p->src_ip),
				   NIPQUAD(p->dst_ip),
				   p->trace.last());
		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem &current = path.timelineSampled.append();
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
			}
			path.timelineSampled.last().drops_p++;
			path.timelineSampled.last().drops_B += p->length;
		}
        if (flowTracking) {
            sampledPathFlowEvents->handlePacket(p, ts_now);
        }
        pathIntervalMeasurements->countPacketInFLightPath(p->path_id, p->ts_start_proc, ts_now, p->length, 1);
        flowIntervalMeasurements->countPacketInFLightPath(p->connection_index, p->ts_start_proc, ts_now, p->length, 1);
        pathIntervalMeasurements->countPacketDropped(p->queue_id, p->path_id, p->ts_start_proc, ts_now, p->length, 1);
        flowIntervalMeasurements->countPacketDropped(p->queue_id, p->connection_index, p->ts_start_proc, ts_now, p->length, 1);
        pathIntervalMeasurements->recordPacketEventPath(p->path_id, p->ts_start_proc, ts_now, p->length, 1, false);
        flowIntervalMeasurements->recordPacketEventPath(p->connection_index, p->ts_start_proc, ts_now, p->length, 1, false);
		return PKT_DROPPED;
	}

	// we need to forward it, find the route
	quint32 nextHop = netGraph->routeCache[p->trace.last()][netGraph->destID2Index[p->dst_id]];
	if (nextHop == NO_ROUTE) {
		// no route, drop and update path stats
		if (DEBUG_PACKETS)
			printf("No route for packet %d.%d.%d.%d -> %d.%d.%d.%d, node=%d\n",
				   NIPQUAD(p->src_ip),
				   NIPQUAD(p->dst_ip),
				   p->trace.last());
		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem &current = path.timelineSampled.append();
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
			}
			path.timelineSampled.last().drops_p++;
			path.timelineSampled.last().drops_B += p->length;
		}
		return PKT_DROPPED;
	} else {
		if ((nextHop & LOAD_BALANCED_ROUTE_MASK) == 1) {
			nextHop = nextHop & LOAD_BALANCED_VALUE_MASK;
			nextHop = netGraph->loadBalancedRouteCache[nextHop].at(rand() % netGraph->loadBalancedRouteCache[nextHop].count());
		}
		NetGraphEdge &e = netGraph->edgeByNodeIndex(p->trace.last(), nextHop);
		if (DEBUG_PACKETS)
            printf("Found route for packet %d.%d.%d.%d -> %d.%d.%d.%d, node=%d, next hop=%d, link=%d\n",
				   NIPQUAD(p->src_ip),
				   NIPQUAD(p->dst_ip),
				   p->trace.last(),
				   nextHop,
				   e.index);
		p->trace.append(nextHop);
		if (e.enqueue(p, ts_now, ts_next)) {
			return PKT_QUEUED;
		} else {
			// packet dropped, update path stats
			if (path.recordSampledTimeline) {
				if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
					pathTimelineItem &current = path.timelineSampled.append();
					memset(&current, 0, sizeof(current));
					current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
				}
				path.timelineSampled.last().drops_p++;
				path.timelineSampled.last().drops_B += p->length;
			}
			return PKT_DROPPED;
		}
	}
}

EdgeTimeline computeEdgeTimelinesBinary(NetGraphEdge e, int queue, quint64 tsMin, quint64 tsMax)
{
    EdgeTimeline timeline;

    // edge timeline range
    if (!e.timelineSampled.isEmpty()) {
        tsMin = qMin(tsMin, e.timelineSampled.first().timestamp);
    }

    tsMin = e.timelineSamplingPeriod ? (tsMin / e.timelineSamplingPeriod) * e.timelineSamplingPeriod : tsMin;

    timeline.tsMin = tsMin;
    timeline.tsMax = tsMax;

    // edge properties
    timeline.timelineSamplingPeriod = e.timelineSamplingPeriod;
    timeline.rate_Bps = e.rate_Bps;
    timeline.delay_ms = e.delay_ms;
    timeline.qcapacity = e.qcapacity;

    const OVector<EdgeTimelineItem> &timelineSampled = queue < 0 ? e.timelineSampled :
                                                                   e.queues[queue].timelineSampled;

    // unroll RLE data
    quint64 lastTs = 0;
    quint64 samplingPeriod = e.timelineSamplingPeriod;
    quint64 lastQueueSampled = 0;
    quint64 lastQueueAvg = 0;
    quint64 lastQueueMax = 0;

    foreach (EdgeTimelineItem item, timelineSampled) {
        // "extrapolate"
        while (item.timestamp > tsMin + lastTs + samplingPeriod) {
            quint64 delta = ((e.rate_Bps * samplingPeriod) / SEC_TO_NSEC);
            lastQueueSampled = (delta < lastQueueSampled) ? lastQueueSampled-delta : 0;
            lastQueueAvg = (delta < lastQueueAvg) ? lastQueueAvg-delta : 0;
            lastQueueMax = (delta < lastQueueMax) ? lastQueueMax-delta : 0;

            lastTs += samplingPeriod;
            EdgeTimelineItem newItem;
            newItem.clear();
            newItem.timestamp = lastTs;
            newItem.queue_sampled = lastQueueSampled;
            newItem.queue_avg = lastQueueAvg;
            newItem.queue_max = lastQueueMax;
            timeline.items.append(newItem);
        }

        lastTs = item.timestamp - tsMin;
        lastQueueSampled = item.queue_sampled;
        lastQueueAvg = item.arrivals_p ? item.queue_avg / item.arrivals_p : 0;
        lastQueueMax = item.queue_max;

        EdgeTimelineItem newItem;
        newItem.clear();

        newItem.timestamp = lastTs;
        newItem.arrivals_p = item.arrivals_p;
        newItem.arrivals_B = item.arrivals_B;
        newItem.qdrops_p = item.qdrops_p;
        newItem.qdrops_B = item.qdrops_B;
        newItem.rdrops_p = item.rdrops_p;
        newItem.rdrops_B = item.rdrops_B;
        newItem.queue_sampled = lastQueueSampled;
        newItem.queue_avg = lastQueueAvg;
        newItem.queue_max = lastQueueMax;
        newItem.flows = item.flows;
        timeline.items.append(newItem);
    }

    return timeline;
}

void saveEdgeTimelinesBinary(quint64 tsMin, quint64 tsMax)
{
    QFile file(QString("edge-timelines.dat"));
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_4_0);

    foreach (NetGraphEdge e, netGraph->edges) {
        if (!e.timelineSampled.isEmpty()) {
            tsMin = qMin(tsMin, e.timelineSampled.first().timestamp);
        }
    }

    EdgeTimelines edgeTmelines;

    foreach (NetGraphEdge e, netGraph->edges) {
        QVector<EdgeTimeline> edgeQueueTimelines;
        for (int queue = -1; queue < e.queueCount; queue++) {
            edgeQueueTimelines << computeEdgeTimelinesBinary(e, queue, tsMin, tsMax);
        }
        edgeTmelines.timelines << edgeQueueTimelines;
    }

    out << edgeTmelines;
}

void saveEdgeStats() {
    QFile edgeStatsFile("edgestats.txt");
    if (edgeStatsFile.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        QTextStream edgeStats(&edgeStatsFile);

        foreach (NetGraphEdge e, netGraph->edges) {
            if (e.packets_in == 0) {
                edgeStats << QString("===== Link %1: no traffic").arg(e.index + 1) << endl;
                continue;
            }

            edgeStats << QString("===== Link %1").arg(e.index + 1) << endl;
            edgeStats << QString("  === Global stats") << endl;
            edgeStats << QString("    = Bandwidth: %1 KB/s (%2 Mbps)").
                         arg(e.bandwidth).
                         arg(e.bandwidth * 8.0 / 1000.0) << endl;
            edgeStats << QString("    = Propagation delay: %1 ms").arg(e.delay_ms) << endl;
            edgeStats << QString("    = Queue length: %1 frames").arg(e.queueLength) << endl;
            edgeStats << QString("    =") << endl;
            edgeStats << QString("    = Bernoulli loss: %1").arg(e.lossBernoulli) << endl;
            edgeStats << QString("    = Bernoulli loss (int): %1").arg(e.lossRate_int) << endl;
            edgeStats << QString("    =") << endl;
            edgeStats << QString("    = Bandwidth: %1 B/s").arg(e.rate_Bps) << endl;
            edgeStats << QString("    = Queue length: %1 bytes").arg(e.qcapacity) << endl;
            edgeStats << QString("    = Queue length: %1 bits").arg(e.qcapacity * 8) << endl;
            edgeStats << QString("    = Queue length: %1 Mb").arg(e.qcapacity * 8 / 1.0e6) << endl;
            edgeStats << QString("    = Queue length: %1 ms").arg(e.qcapacity * 1.0e3 / qreal(e.rate_Bps)) << endl;
            edgeStats << QString("    = Queue load (end): %1 bytes").arg(e.qload) << endl;
            edgeStats << QString("    =") << endl;
            edgeStats << QString("    = Packets received: %1 (%2 p/s)").arg(e.packets_in).arg(e.packets_in ? qreal(SEC_TO_NSEC) * qreal(e.packets_in) / (e.tsMax - e.tsMin) : 0) << endl;
            edgeStats << QString("    = Bytes received: %1 (%2 KB/s, %3 Mbps)").
                         arg(e.bytes).
                         arg(e.packets_in ? e.bytes / 1.0e3 * qreal(SEC_TO_NSEC) / (e.tsMax - e.tsMin) : 0).
                         arg(e.packets_in ? e.bytes * 8.0 / 1.0e6 * qreal(SEC_TO_NSEC) / (e.tsMax - e.tsMin) : 0) << endl;
            edgeStats << QString("    = Queue drops: %1 (%2)").arg(e.qdrops).arg(e.packets_in ? e.qdrops / qreal(e.packets_in) : 0) << endl;
            edgeStats << QString("    = Bernoulli drops: %1 (%2)").arg(e.rdrops).arg(e.packets_in ? e.rdrops / qreal(e.packets_in) : 0) << endl;
            edgeStats << QString("    = Average queuing delay: %1 ms").arg(e.packets_in ? e.total_qdelay * 1.0e3 / qreal(SEC_TO_NSEC) / qreal(e.packets_in) : 0) << endl;
            edgeStats << QString("    =") << endl;
            edgeStats << QString("    = Event span: %1 s").arg(e.tsMin < e.tsMax ? (e.tsMax - e.tsMin) / qreal(SEC_TO_NSEC) : 0) << endl;

            for (int q = 0; q < e.queueCount; q++) {
                edgeStats << QString("    === Queue %1").arg(q) << endl;
                edgeStats << QString("      = Bandwidth: %1 KB/s (%2 Mbps)").
                             arg(e.queues[q].bandwidth).
                             arg(e.queues[q].bandwidth / 1000.0 * 8.0)
                          << endl;
                edgeStats << QString("      = Propagation delay: %1 ms").arg(e.queues[q].delay_ms) << endl;
                edgeStats << QString("      = Queue length: %1 frames").arg(e.queues[q].queueLength) << endl;
                edgeStats << QString("      =") << endl;
                edgeStats << QString("      = Bernoulli loss: %1").arg(e.queues[q].lossBernoulli) << endl;
                edgeStats << QString("      = Bernoulli loss (int): %1").arg(e.queues[q].lossRate_int) << endl;
                edgeStats << QString("      =") << endl;
                edgeStats << QString("      = Bandwidth: %1 B/s").arg(e.queues[q].rate_Bps) << endl;
                edgeStats << QString("      = Queue length: %1 bytes").arg(e.queues[q].qcapacity) << endl;
                edgeStats << QString("      = Queue length: %1 bits").arg(e.queues[q].qcapacity * 8) << endl;
                edgeStats << QString("      = Queue length: %1 Mb").arg(e.queues[q].qcapacity * 8 / 1.0e6) << endl;
                edgeStats << QString("      = Queue length: %1 ms").arg(e.queues[q].qcapacity * 1.0e3 / qreal(e.queues[q].rate_Bps)) << endl;
                edgeStats << QString("      = Queue discipline: %1").arg(e.queues[q].queuingDiscipline == QueuingDisciplineDropTail
                                                                         ? "drop-tail"
                                                                         : e.queues[q].queuingDiscipline == QueuingDisciplineDropHead
                                                                           ? "drop-head"
                                                                           : e.queues[q].queuingDiscipline == QueuingDisciplineDropRand
                                                                             ? "drop-rand"
                                                                             : QString::number(e.queues[q].queuingDiscipline)) << endl;
                edgeStats << QString("      = Queue load (end): %1 bytes").arg(e.queues[q].qload) << endl;
                edgeStats << QString("      =") << endl;
                edgeStats << QString("      = Packets received: %1 (%2 p/s)").arg(e.queues[q].packets_in).arg(e.queues[q].packets_in ? qreal(SEC_TO_NSEC) * qreal(e.queues[q].packets_in) / (e.tsMax - e.tsMin) : 0) << endl;
                edgeStats << QString("      = Bytes received: %1 (%2 KB/s, %3 Mbps)").
                             arg(e.queues[q].bytes).
                             arg(e.queues[q].packets_in ? e.queues[q].bytes / 1.0e3 * qreal(SEC_TO_NSEC) / (e.tsMax - e.tsMin) : 0).
                             arg(e.queues[q].packets_in ? e.queues[q].bytes * 8.0 / 1.0e6 * qreal(SEC_TO_NSEC) / (e.tsMax - e.tsMin) : 0) << endl;
                edgeStats << QString("      = Queue drops: %1 (%2)").arg(e.queues[q].qdrops).arg(e.queues[q].packets_in ? e.queues[q].qdrops / qreal(e.queues[q].packets_in) : 0) << endl;
                edgeStats << QString("      = Bernoulli drops: %1 (%2)").arg(e.queues[q].rdrops).arg(e.queues[q].packets_in ? e.queues[q].rdrops / qreal(e.queues[q].packets_in) : 0) << endl;
                edgeStats << QString("      = Average queuing delay: %1 ms").arg(e.queues[q].packets_in ? e.queues[q].total_qdelay * 1.0e3 / qreal(SEC_TO_NSEC) / qreal(e.queues[q].packets_in) : 0) << endl;
                edgeStats << QString("      =") << endl;
                edgeStats << QString("      = Event span: %1 s").arg(e.queues[q].tsMin < e.queues[q].tsMax ? (e.queues[q].tsMax - e.queues[q].tsMin) / qreal(SEC_TO_NSEC) : 0) << endl;
            }
            for (int p = 0; p < e.policerCount; p++) {
                edgeStats << QString("    === Policer %1").arg(p) << endl;
                edgeStats << QString("      = Bandwidth: %1 KB/s (%2 Mbps)").
                             arg(e.policers[p].fillRate * 1.0e6).
                             arg(e.policers[p].fillRate * 1.0e3 * 8.0)
                          << endl;
                edgeStats << QString("      = Capacity: %1 frames").arg(e.policers[p].capacity / qreal(ETH_FRAME_LEN)) << endl;
                edgeStats << QString("      = Capacity: %1 bytes").arg(e.policers[p].capacity) << endl;
                edgeStats << QString("      = Capacity: %1 ms").arg(e.policers[p].capacity / qreal(e.policers[p].fillRate) / MSEC_TO_NSEC) << endl;
                edgeStats << QString("      =") << endl;
                edgeStats << QString("      = Load (end): %1 bytes (%2 frames)").
                             arg(e.policers[p].currentLevel).
                             arg(e.policers[p].currentLevel / ETH_FRAME_LEN) << endl;
                edgeStats << QString("      =") << endl;
                edgeStats << QString("      = Packets received: %1 (%2 p/s)").arg(e.policers[p].packets_in).arg(e.policers[p].packets_in ? qreal(SEC_TO_NSEC) * qreal(e.policers[p].packets_in) / (e.tsMax - e.tsMin) : 0) << endl;
                edgeStats << QString("      = Bytes received: %1 (%2 KB/s, %3 Mbps)").
                             arg(e.policers[p].bytes).
                             arg(e.policers[p].packets_in ? e.policers[p].bytes / 1.0e3 * qreal(SEC_TO_NSEC) / (e.tsMax - e.tsMin) : 0).
                             arg(e.policers[p].packets_in ? e.policers[p].bytes * 8.0 / 1.0e6 * qreal(SEC_TO_NSEC) / (e.tsMax - e.tsMin) : 0) << endl;
                edgeStats << QString("      = Drops: %1 (%2)").arg(e.policers[p].drops).arg(e.policers[p].packets_in ? e.policers[p].drops / qreal(e.policers[p].packets_in) : 0) << endl;
                edgeStats << QString("      =") << endl;
            }
        }
        edgeStats.flush();
    }
    edgeStatsFile.close();
}

void saveRecordedData()
{
	saveFile("simulation.txt", QString("graph=%1").arg(netGraph->fileName.replace(".graph", "").split('/', QString::SkipEmptyParts).last()));

    saveEdgeStats();

	TomoData tomoData;

	tomoData.m = netGraph->paths.count();
	tomoData.n = netGraph->edges.count();

	// path data: pathCount items
	foreach (NetGraphPath p, netGraph->paths) {
		// path starts: src host index
		tomoData.pathstarts << p.source;
		// path ends: dest host index
		tomoData.pathends << p.dest;
		// the edge list for each path (m items of the form path(index).edges = [id1 ... idx]; these are indices, not real edge ids!
		QList<qint32> edgelist;
		foreach (NetGraphEdge e, p.edgeList) {
			edgelist << e.index;
		}
		tomoData.pathedges << edgelist;
	}

	// compute the routing matrix, m x n of quint8 with values of either 0 or 1
	foreach (NetGraphPath p, netGraph->paths) {
		QVector<quint8> line;
		foreach (NetGraphEdge e, netGraph->edges) {
			if (p.edgeSet.contains(e)) {
				line << 1;
			} else {
				line << 0;
			}
		}
		tomoData.A << line;
	}

	// write the path transmission rate vector: m x 1 of [0..1]
	foreach (NetGraphPath p, netGraph->paths) {
		if (p.packets_in == 0) {
			tomoData.y << 1.0;
		} else {
			tomoData.y << p.packets_out / (qreal)(p.packets_in);
		}
	}

	// write the measured edge transmission rate vector: 1 x n of [0..1]
	foreach (NetGraphEdge e, netGraph->edges) {
		if (e.packets_in == 0) {
			tomoData.xmeasured << 1.0;
		} else {
			tomoData.xmeasured << (e.packets_in - e.qdrops - e.rdrops) / (qreal)(e.packets_in);
		}
	}

	// first compute the time range
	tomoData.tsMin = ULLONG_MAX;
	tomoData.tsMax = 0;

	foreach (NetGraphEdge e, netGraph->edges) {
        if (e.tsMin == ULLONG_MAX || e.tsMax == 0)
            continue;
        tomoData.tsMin = qMin(tomoData.tsMin, e.tsMin);
        tomoData.tsMax = qMax(tomoData.tsMax, e.tsMax);
	}

	tomoData.T.resize(netGraph->paths.count());
	for (int p = 0; p < netGraph->paths.count(); p++) {
		tomoData.T[p].resize(netGraph->edges.count());
		for (int e = 0; e < netGraph->edges.count(); e++) {
			NetGraphEdge &edge = netGraph->edges[e];
			tomoData.T[p][e] = (edge.packets_in_perpath[p] == 0) ? 0.0 : (edge.packets_in_perpath[p] - edge.rdrops_perpath[p] - edge.qdrops_perpath[p])/(qreal)(edge.packets_in_perpath[p]);
		}
	}

	tomoData.packetCounters.resize(netGraph->paths.count());
	for (int p = 0; p < netGraph->paths.count(); p++) {
		tomoData.packetCounters[p].resize(netGraph->edges.count());
		for (int e = 0; e < netGraph->edges.count(); e++) {
			NetGraphEdge &edge = netGraph->edges[e];
			tomoData.packetCounters[p][e] = edge.packets_in_perpath[p];
		}
	}

	tomoData.traffic.resize(netGraph->paths.count());
	for (int p = 0; p < netGraph->paths.count(); p++) {
		tomoData.traffic[p].resize(netGraph->edges.count());
		for (int e = 0; e < netGraph->edges.count(); e++) {
			NetGraphEdge &edge = netGraph->edges[e];
			tomoData.traffic[p][e] = edge.bytes_in_perpath[p];
		}
	}

	tomoData.qdelay.resize(netGraph->paths.count());
	for (int p = 0; p < netGraph->paths.count(); p++) {
		tomoData.qdelay[p].resize(netGraph->edges.count());
		for (int e = 0; e < netGraph->edges.count(); e++) {
			NetGraphEdge &edge = netGraph->edges[e];
			tomoData.qdelay[p][e] = edge.qdelay_perpath[p];
		}
	}

	tomoData.save("tomo-records.dat");

	// edge timeline aggregates/samples
    saveEdgeTimelinesBinary(tomoData.tsMin, tomoData.tsMax);
}

static quint64 max_loop_delay;
static quint64 max_sync_delay;
static quint64 max_packet_init_delay;
static quint64 total_loop_delay;
static quint64 total_loops;
static quint64 max_event_delay;
static quint64 total_event_delay;
static quint64 packetsQdropped;
static quint64 numQueuingEvents;
static quint64 numEventInversions;
static quint64 totalEventInversionDelay;
static quint64 maxEventInversionDelay;
static quint64 tsStart;
static quint64 emulationDuration;
static quint64 numActiveQueues;
// time
static OVector<quint64> highLatencyEventsTs;
// memory usage
static OVector<quint64> highLatencyEventsMem;
// thread cache
static OVector<quint64> highLatencyEventsMemThread;

bool comparePacketDrainEvents(const Packet* a, const Packet* b) {
	return a->ts_expected_exit < b->ts_expected_exit;
}

void drain(quint64 ts_now, OVector<Packet*> &result)
{
	result.clear();
	for (int iEdge = 0; iEdge < netGraph->edges.count(); iEdge++) {
		for (int iQueue = 0; iQueue < netGraph->edges[iEdge].queues.count(); iQueue++) {
			netGraph->edges[iEdge].queues[iQueue].drain(ts_now, result);
		}
	}
	qSort(result.begin(), result.end(), comparePacketDrainEvents);
}

void* packet_scheduler_thread(void* )
{
	barrierInit.wait();
	__sync_synchronize();

	pthread_setname_np(pthread_self(), "line-packet-scheduler");

	u_int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
	u_long core_id = CORE_SCHEDULER;

	if (bind2core(core_id) == 0) {
		printf("Set thread scheduler affinity to core %lu/%u\n", core_id, numCPU);
	} else {
		printf("Failed to set thread scheduler affinity to core %lu/%u\n", core_id, numCPU);
	}

	warmMallocCache();

    max_loop_delay = 0;
    total_loop_delay = 0;
    total_loops = 0;
	max_sync_delay = 0;
	max_packet_init_delay = 0;
    packetsQdropped = 0;
    numQueuingEvents = 0;
	max_event_delay = 0;
	total_event_delay = 0;
	numEventInversions = 0;
	totalEventInversionDelay = 0;
	maxEventInversionDelay = 0;
	// last event that was processed
	quint64 ts_last_event = 0;
    tsStart = get_current_time();

	OVector<Packet*> localPacketsToSend;
	localPacketsToSend.reserve(10000);
	highLatencyEventsTs.reserve(100000);
	highLatencyEventsMem.reserve(100000);
	highLatencyEventsMemThread.reserve(100000);

	OVector<Packet*> events;
	events.reserve(10000);
	OVector<Packet*> newPackets;
	newPackets.reserve(10000);

	barrierInitDone.wait();
	barrierStart.wait();

#if DUMP_STACKTRACE_ON_MALLOC
	malloc_profile_set_trace_cpu_wrapper(1);
#endif

	while (1) {
		if (do_shutdown) {
			break;
		}

		quint64 ts_now = get_current_time();

		if (!localPacketsToSend.isEmpty()) {
			packetsOut.enqueue(localPacketsToSend/*, 1ULL * MSEC_TO_NSEC*/);
			localPacketsToSend.clear();
		}

		// process new packets
		packetsIn.dequeueAll(newPackets/*, 1ULL * MSEC_TO_NSEC*/);

		quint64 ts_after_sync = get_current_time();
		max_sync_delay = qMax(max_sync_delay, ts_after_sync - ts_now);
		ts_now = ts_after_sync;

#if BYPASS_SCHEDULER
		foreach (Packet *p, newPackets) {
			p->theoretical_delay = 1;
		}
		localPacketsToSend << newPackets;
		continue;
#endif

		bool receivedPackets = !newPackets.isEmpty();
		for (int iPacket = 0; iPacket < newPackets.count(); iPacket++) {
			// new packet arrived
			Packet *p = newPackets[iPacket];
			p->ts_start_proc = ts_now;
			p->ts_expected_exit = 0;
			if (p->src_id < 0 || p->src_id >= netGraph->nodes.count() ||
				p->dst_id < 0 || p->dst_id >= netGraph->nodes.count()) {
				// foreign packet
				if (DEBUG_PACKETS)
					printf("Bad packet %d.%d.%d.%d -> %d.%d.%d.%d (src = %d, dst = %d)\n",
						   NIPQUAD(p->src_ip),
						   NIPQUAD(p->dst_ip),
						   p->src_id,
						   p->dst_id);
				p->dropped = true;
				localPacketsToSend.append(p);
				continue;
			}
			numQueuingEvents++;
			quint64 ts_next_event;
			int pkt_state = routePacket(p, p->ts_userspace_rx, ts_next_event);
			if (pkt_state == PKT_QUEUED) {
				if (DEBUG_PACKETS)
					printf("Enqueue: %d.%d.%d.%d -> %d.%d.%d.%d, for time = +%llu ns, tracelen = %d\n",
						   NIPQUAD(p->src_ip),
						   NIPQUAD(p->dst_ip),
						   ts_next_event - ts_now,
						   p->trace.count());
			} else if (pkt_state == PKT_DROPPED) {
				if (DEBUG_PACKETS)
					printf("Drop: %d.%d.%d.%d -> %d.%d.%d.%d\n",
						   NIPQUAD(p->src_ip),
						   NIPQUAD(p->dst_ip));
				packetsQdropped++;
				p->dropped = true;
				localPacketsToSend.append(p);
			} else if (pkt_state == PKT_FORWARDED) {
				localPacketsToSend.append(p);
			}
		}
		newPackets.clear();

		max_packet_init_delay = qMax(max_packet_init_delay, get_current_time() - ts_now);

		// process events
		bool receivedEvents = false;
		for (drain(ts_now, events); !events.isEmpty(); drain(ts_now, events)) {
			for (int iPacket = 0; iPacket < events.count(); iPacket++) {
				Packet *p = events[iPacket];
				QPair<Packet*, quint64> event(p, p->ts_expected_exit);
				if (!p->dropped) {
					receivedEvents = true;
					numQueuingEvents++;
					ts_last_event = qMax(ts_last_event, event.second);
					if (event.second < ts_last_event) {
						// not good
						numEventInversions++;
						quint64 inversionDelay = ts_last_event - event.second;
						maxEventInversionDelay = qMax(maxEventInversionDelay, inversionDelay);
						totalEventInversionDelay += inversionDelay;
					}
					quint64 event_delay = ts_now - event.second;
					max_event_delay = qMax(max_event_delay, event_delay);
					total_event_delay += event_delay;
				}
				quint64 ts_next_event;
				int pkt_state = routePacket(p, event.second, ts_next_event);
				if (pkt_state == PKT_QUEUED) {
					if (DEBUG_PACKETS)
						printf("Enqueue: %d.%d.%d.%d -> %d.%d.%d.%d, for time = +%llu ns, tracelen = %d\n",
							   NIPQUAD(p->src_ip),
							   NIPQUAD(p->dst_ip),
							   ts_next_event - ts_now,
							   p->trace.count());
				} else if (pkt_state == PKT_DROPPED) {
					if (DEBUG_PACKETS)
						printf("Drop: %d.%d.%d.%d -> %d.%d.%d.%d\n",
							   NIPQUAD(p->src_ip),
							   NIPQUAD(p->dst_ip));
					packetsQdropped++;
					p->dropped = true;
					localPacketsToSend.append(p);
				} else if (pkt_state == PKT_FORWARDED) {
					localPacketsToSend.append(p);
				}
			}
		}

		// begin stats
		if (receivedPackets || receivedEvents) {
			if (tsFirstSentPacket == 0) {
				__sync_synchronize();
			}
			if (tsFirstSentPacket > 0) {
				quint64 ts_after = get_current_time();
				quint64 loop_delay = ts_after - ts_now;
				if (ts_after - tsFirstSentPacket > RECORD_STATS_DELAY) {
					max_loop_delay = qMax(max_loop_delay, loop_delay);
					total_loop_delay += ts_after - ts_now;
					total_loops++;
				}
				if (loop_delay >= MSEC_TO_NSEC &&
					highLatencyEventsTs.count() < 100) {
					highLatencyEventsTs << ts_now;
#ifdef USE_TC_MALLOC
					size_t tmp;
					if (MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes", &tmp)) {
						highLatencyEventsMem << tmp;
					} else {
						highLatencyEventsMem << 0;
					}
					if (MallocExtension::instance()->GetNumericProperty("tcmalloc.current_total_thread_cache_bytes", &tmp)) {
						highLatencyEventsMemThread << tmp;
					} else {
						highLatencyEventsMemThread << 0;
					}
#else
					highLatencyEventsMem << 0;
					highLatencyEventsMemThread << 0;
#endif
				}
			}
		}
		// end stats
		// qDebug() << "Loop took < " << max_loop_delay << "ns";
	}

	malloc_profile_pause_wrapper();

    emulationDuration = get_current_time() - tsStart;

    for (int e = 0; e < netGraph->edges.count(); e++) {
        netGraph->edges[e].postEmulation();
    }

    numActiveQueues = 0;
    for (int e = 0; e < netGraph->edges.count(); e++) {
        for (int q = 0; q < netGraph->edges[e].queues.count(); q++) {
            if (netGraph->edges[e].queues[q].packets_in > 0) {
                numActiveQueues++;
            }
        }
    }

	saveRecordedData();

    return NULL;
}

void print_scheduler_stats()
{
    printf("===== Scheduler stats ====\n");
	printf("Scheduler non-idle loop took: avg  " TS_FORMAT " , max  " TS_FORMAT " \n",
		   TS_FORMAT_PARAM(total_loop_delay / (total_loops ? total_loops : 1)),
		   TS_FORMAT_PARAM(max_loop_delay));
	printf("Event delay: avg  " TS_FORMAT " , max  " TS_FORMAT " \n",
		   TS_FORMAT_PARAM(total_event_delay / (numQueuingEvents ? numQueuingEvents : 1)),
		   TS_FORMAT_PARAM(max_event_delay));
	printf("Sync delay: max  " TS_FORMAT " \n",
		   TS_FORMAT_PARAM(max_sync_delay));
	printf("Packet init delay: max  " TS_FORMAT " \n",
		   TS_FORMAT_PARAM(max_packet_init_delay));
	printf("Event inversions: %llu\n", numEventInversions);
	printf("Event inversions delay: avg  " TS_FORMAT " , max  " TS_FORMAT " \n",
		   TS_FORMAT_PARAM(totalEventInversionDelay / (numEventInversions ? numEventInversions : 1)),
		   TS_FORMAT_PARAM(maxEventInversionDelay));
	printf("Total packets qdropped: %s\n",
		   withCommas(packetsQdropped));
	printf("Active queues: %s\n",
		   withCommas(numActiveQueues));
	printf("Queuing events per second: %s\n",
		   withCommas(qreal(numQueuingEvents) * 1.0e9 / emulationDuration));

	if (gQueuingDiscipline == QueuingDisciplineDropTail) {
        printf("Default queuing discipline: tail drop\n");
	} else if (gQueuingDiscipline == QueuingDisciplineDropHead) {
        printf("Default queuing discipline: head drop\n");
	} else if (gQueuingDiscipline == QueuingDisciplineDropRand) {
        printf("Default queuing discipline: random drop\n");
    } else {
        printf("Default queuing discipline: unknown?!\n");
    }

	printf("Queuing buffer size multiplier: %f\n", bufferBloatFactor);

    if (QUEUEING_ECN_ENABLED) {
        printf("ECN: enabled\n");
    } else {
        printf("ECN: disabled\n");
    }

	if (qosBufferScaling == QosBufferScalingNone) {
		printf("QoS buffer scaling: no scaling (same size as the link's buffer)\n");
	} else if (qosBufferScaling == QosBufferScalingDown) {
		printf("QoS buffer scaling: scaled down (smaller weight -> smaller buffer)\n");
	} else if (qosBufferScaling == QosBufferScalingUp) {
		printf("QoS buffer scaling: scaled up (smaller weight -> larger buffer)\n");
	}

	if (POLICING_ENABLED) {
		printf("Traffic policing (token buckets): enabled\n");
	} else {
		printf("Traffic policing (token buckets): disabled\n");
	}

    if (WFQ_ENABLED) {
        printf("Traffic shaping (WFQ): enabled\n");
    } else {
        printf("Traffic shaping (WFQ): disabled\n");
    }

	for (int i = 0; i < highLatencyEventsTs.count(); i++) {
		quint64 t = highLatencyEventsTs[i];
		quint64 mem = highLatencyEventsMem[i];
		quint64 tc = highLatencyEventsMemThread[i];
		printf("High latency event at t =  " TS_FORMAT " : mem usage = %s B, thread cache = %s B\n",
			   TS_FORMAT_PARAM(t - tsStart), withCommas(mem), withCommas(tc));
	}
}
