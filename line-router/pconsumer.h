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

#ifndef PFILTER_H
#define PFILTER_H

extern "C" {
#include <pfring.h>
}
#include <QtCore>
#include "spinlockedqueue.h"

#include "../line-gui/intervalmeasurements.h"
#include "../line-gui/line-record.h"
#include "../line-gui/netgraph.h"
#include "../line-gui/flowevent.h"
#include "../util/spinlockedqueue.h"
#include "../util/waitfreequeuedvyukov.h"
#include "../util/waitfreequeuefolly.h"
#include "../util/waitfreequeuemoody.h"
#include "../util/qbarrier.h"
#include "../util/ovector.h"
#include "../malloc_profile/malloc_profile_wrapper.h"

// masks from libipaddr.so
#define NAT_SUBNET   htonl(0x0a000000)  /* 10.0.0.0/8 */
#define NAT_MASK     htonl(0xff000000)  /* 10.0.0.0/8 */
#define NAT_FOREIGN  htonl(0x00800000)  /* 0000 0000 . 1000 0000 . 0000 0000 . 0000 0000 which gives 10.128.0.0/9 */
#define NAT_HOSTMASK 0x7FFFFF           /* 0000 0000 . 0111 1111 . 1111 1111 . 1111 1111 */

class Packet {
public:
	Packet() {
		init();
	}

	void init() {
		ts_driver_rx = 0;
		ts_userspace_rx = 0;
		ts_start_proc = 0;
		ts_start_send = 0;
		ts_send = 0;
		theoretical_delay = 0;
		preparedForSend = false;
		length = 0;
		memset(&offsets, 0, sizeof(offsets));
		src_ip = 0;
		dst_ip = 0;
		l4_protocol = 0;
		l4_src_port	= 0;
		l4_dst_port = 0;
		tcpFlags = 0;
		tcpSeqNum = 0;
		tcpAckNum = 0;
		trace.clear();
		trace.reserve(50);
		src_id = -1;
		dst_id = -1;
		path_id = -1;
		interface = -1;
		id = 0;
		traffic_class = 0;
		queue_id = -1;
        connection_index = -1;
		ts_expected_exit = 0;
		dropped = false;
		ecn_bit_set = false;
	}

	void generateNewId() {
		id = next_packet_unique_id;
		next_packet_unique_id++;
	}

    // The packet contents. Use this->offsets to find the offsets of each header.
	quint8 buffer[2048];

    // All timestamps are in nanoseconds.
    // Timestamp for the moment when the driver received the packet (if not available, set to ts_userspace_rx).
	// This is shit, ignore it.
	quint64 ts_driver_rx;
    // Timestamp for the moment when line-router read the packet.
	quint64 ts_userspace_rx;
    // Timestamp for the moment when the packet scheduler started processing the packet.
	quint64 ts_start_proc;
    // Timestamp for the moment when the packet scheduler finished processing the packet and queued it to be sent.
	quint64 ts_start_send;
	// Timestamp for the moment when the packet sender sent the packet, or for when the packet was dropped.
	quint64 ts_send;
    // Sum of the theoretical delays incurred by the packet on each link.
    // Ideally, equal to (ts_start_send - ts_start_proc).
    quint64 theoretical_delay;
	bool preparedForSend;

    // frame length
    int length;
	struct pkt_offset offsets;
	in_addr_t src_ip;
	in_addr_t dst_ip;
	quint8 l4_protocol;
	quint16 l4_src_port;
	quint16 l4_dst_port;
	// TCP flags (0 if not available)
	quint8 tcpFlags;
	// TCP sequence number
	quint32 tcpSeqNum;
	quint32 tcpAckNum;
    // List of node IDs that the packet traversed. Includes the first and last nodes.
	OVector<qint32> trace;
    // ID of source NetGraphNode
    qint32 src_id;
    // ID of destination NetGraphNode
    qint32 dst_id;
    // ID of the path
    qint32 path_id;
	qint32 interface;
    // Unique packet ID.
    quint64 id;
    qint32 traffic_class;

	// Current queue ID where the packet is buffered; -1 if not available
	qint32 queue_id;
	// The time when the packet (the last byte) should reach the next link
	quint64 ts_expected_exit;
    // The index of the connection in the graph or -1 if not available
    qint32 connection_index;
	// True if the packet is dropped, important if it happens after queuing (e.g. with drop-head)
	bool dropped;
	bool ecn_bit_set;

    // Global counter used to generate unique packet IDs.
	static quint64 next_packet_unique_id;
};

extern RecordedData *recordedData;
extern ExperimentIntervalMeasurements *pathIntervalMeasurements;
extern ExperimentIntervalMeasurements *flowIntervalMeasurements;
extern SampledPathFlowEvents *sampledPathFlowEvents;
extern NetGraph *netGraph;

extern quint64 estimatedDuration;

extern pfring *pd;
extern quint8 wait_for_packet; // 1 = blocking read, 0 = busy waiting
extern quint8 dna_mode;
extern quint8 do_shutdown;

extern QString simulationId;

// All queue sized are multiplied by this number
extern qreal bufferBloatFactor;

extern bool flowTracking;

extern QueuingDiscipline gQueuingDiscipline;

enum QosBufferScaling {
	QosBufferScalingNone = 0,
	QosBufferScalingDown,
	QosBufferScalingUp
};

extern QosBufferScaling qosBufferScaling;

extern quint64 simulationStartTime;
extern quint64 tsFirstSentPacket; // 0 = invalid
// We only record performance statistics after RECORD_STATS_DELAY nanoseconds
// since the first sent packet.
#define RECORD_STATS_DELAY (1 * SEC_TO_NSEC)

// malloc and free memory to have the blocks cached
void warmMallocCache();

int getInterfaceSpeedMbps(const char *interfaceName);

// CPU affinity
#define CORE_CONSUMER 1

#define SEC_TO_NSEC  1000000000ULL
#define MSEC_TO_NSEC 1000000ULL
#define USEC_TO_NSEC 1000ULL

#define TS_FORMAT "%llu %3llus %3llum %3lluu %3llun"
#define TS_FORMAT_PARAM(X) (X)/SEC_TO_NSEC/1000, ((X)/SEC_TO_NSEC)%1000, ((X)/MSEC_TO_NSEC)%1000, ((X)/USEC_TO_NSEC)%1000, (X)%1000

#define DEBUG_PACKETS 0

int runPacketFilter(int argc, char **argv);

void* packet_consumer_thread(void* );
void print_consumer_stats();
void* packet_scheduler_thread(void* );
void print_scheduler_stats();

int bind2core(u_int core_id);

quint64 get_current_time();

void loadTopology(QString graphFileName);


#define QUEUE_IMPL_SPIN 0
#define QUEUE_IMPL_MOODY 1
#define QUEUE_IMPL_DVYUKOV 2
#define QUEUE_IMPL_FOLLY 3

#define QUEUE_IMPL QUEUE_IMPL_FOLLY

#if QUEUE_IMPL == QUEUE_IMPL_SPIN
#  define SyncQueueType SpinlockedQueue
#elif QUEUE_IMPL == QUEUE_IMPL_MOODY
#  define SyncQueueType WaitFreeQueueMoody
#elif QUEUE_IMPL == QUEUE_IMPL_DVYUKOV
#  define SyncQueueType WaitFreeQueueDVyukov
#elif QUEUE_IMPL == QUEUE_IMPL_FOLLY
#  define SyncQueueType WaitFreeQueueFolly
#else
#error "QUEUE_IMPL"
#endif

extern SyncQueueType<Packet*> packetsIn;
extern SyncQueueType<Packet*> packetPool;
extern QBarrier barrierInit;
extern QBarrier barrierInitDone;
extern QBarrier barrierStart;

// Display an IP address in readable format.
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#if defined(__LITTLE_ENDIAN)
#define HIPQUAD(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]
#elif defined(__BIG_ENDIAN)
#define HIPQUAD NIPQUAD
#else
#error "Please fix endian.h"
#endif /* __LITTLE_ENDIAN */

#endif // PFILTER_H
