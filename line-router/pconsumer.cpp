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

#include "pconsumer.h"

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
#include <net/ethernet.h>     /* the L2 protocols */
#include <sys/time.h>
#include <bits/time.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

extern "C" {
#include <pfring.h>
}

#include <QtCore>

#include "../remote_config.h"
#include "../line-gui/netgraphnode.h"
#include "../util/ovector.h"

#define PROFILE_PCONSUMER 0

quint64 Packet::next_packet_unique_id = 0;

SyncQueueType<Packet*> packetsIn;
SyncQueueType<Packet*> packetPool;
QBarrier barrierInit(3);
QBarrier barrierInitDone(3);
QBarrier barrierStart(3);

quint64 get_current_time()
{
	struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

	return ((quint64)ts.tv_sec) * 1000ULL * 1000ULL * 1000ULL + ((quint64)ts.tv_nsec);
}

static quint64 packetsReceived;
static quint64 bytesReceived;
static quint64 miniJumbosReceived;
static quint64 jumbosReceived;
static quint64 tsStart;
static quint64 emulationDuration;

void* packet_consumer_thread(void* ) {
	barrierInit.wait();
	__sync_synchronize();

	pthread_setname_np(pthread_self(), "line-packet-capture");

	u_int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
	u_long core_id = CORE_CONSUMER;

	if (bind2core(core_id) == 0) {
		printf("Set thread consumer affinity to core %lu/%u\n", core_id, numCPU);
	} else {
		printf("Failed to set thread consumer affinity to core %lu/%u\n", core_id, numCPU);
	}

	warmMallocCache();

    packetsReceived = 0;
    bytesReceived = 0;
    miniJumbosReceived = 0;
    jumbosReceived = 0;

	struct pfring_pkthdr hdr;
	memset(&hdr, 0, sizeof(hdr));

#if PROFILE_PCONSUMER
	quint64 ts_prev = 0;
#endif

    barrierInitDone.wait();
    malloc_profile_reset_wrapper();
    barrierStart.wait();

    tsStart = get_current_time();
	tsFirstSentPacket = 0;

	Packet *p = nullptr;
	while (1) {
		if (do_shutdown)
			break;

		if (p == nullptr) {
			if (packetPool.tryDequeue(p)) {
				p->init();
			} else {
				p = new Packet();
			}
		}

		quint8 *buffer = p->buffer;
		quint8 **buffer_ptr = &buffer;

		if (pfring_recv(pd, buffer_ptr, sizeof(p->buffer), &hdr, 0) > 0) {
			if (do_shutdown)
				break;
			bytesReceived += hdr.len;
			if (hdr.len > 1514) {
				if (hdr.len > 1518) {
					jumbosReceived++;
					if (DEBUG_PACKETS) {
						if (hdr.extended_hdr.parsed_pkt.ip_version == 4) {
							printf("Long packet (%d B) %d.%d.%d.%d -> %d.%d.%d.%d is dropped!\n",
								   hdr.len,
								   HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_src.v4),
								   HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_dst.v4));
						}
					}
					continue;
				} else {
					miniJumbosReceived++;
					// these are caused by path MTU discovery, the deployment script should have turned it off!!!
					continue;
				}
			}
			if (hdr.caplen != hdr.len) {
				qDebug() << "hdr.caplen != hdr.len:" << hdr.caplen << hdr.len;
				continue;
			}
			if (hdr.extended_hdr.parsed_pkt.ip_version == 4) {
                if (((htonl(hdr.extended_hdr.parsed_pkt.ip_src.v4) & NAT_MASK) == NAT_SUBNET) &&
                    ((htonl(hdr.extended_hdr.parsed_pkt.ip_dst.v4) & NAT_MASK) == NAT_SUBNET) &&
                    (htonl(hdr.extended_hdr.parsed_pkt.ip_dst.v4) & NAT_FOREIGN) &&
                    !(htonl(hdr.extended_hdr.parsed_pkt.ip_src.v4) & NAT_FOREIGN)) {
					if (DEBUG_PACKETS)
						printf("Accepted packet %d.%d.%d.%d -> %d.%d.%d.%d\n",
							   HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_src.v4),
							   HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_dst.v4));
					packetsReceived++;
					quint64 ts_now = get_current_time();
#if PROFILE_PCONSUMER
					printf("sw ts delta = + "TS_FORMAT" \n", TS_FORMAT_PARAM(ts_now-ts_prev));
					ts_prev = ts_now;
					//printf("hw ts =  "TS_FORMAT" \n", TS_FORMAT_PARAM((quint64)hdr.extended_hdr.timestamp_ns));
					//printf("sw ts =  "TS_FORMAT" \n", TS_FORMAT_PARAM(ts_now));
#endif
					p->generateNewId();
					p->ts_driver_rx = hdr.extended_hdr.timestamp_ns ? hdr.extended_hdr.timestamp_ns : ts_now;
					p->ts_userspace_rx = ts_now;
					p->src_ip = htonl(hdr.extended_hdr.parsed_pkt.ip_src.v4);
					p->dst_ip = htonl(hdr.extended_hdr.parsed_pkt.ip_dst.v4);
                    p->src_id = (ntohl(p->src_ip) & NAT_HOSTMASK) - IP_OFFSET;
                    p->dst_id = (ntohl(p->dst_ip) & NAT_HOSTMASK) - IP_OFFSET;
					p->l4_protocol = hdr.extended_hdr.parsed_pkt.l3_proto; // they named it worng
					p->l4_src_port = hdr.extended_hdr.parsed_pkt.l4_src_port;
					p->l4_dst_port = hdr.extended_hdr.parsed_pkt.l4_dst_port;
                    p->connection_index = netGraph->getConnectionIndex(p->l4_src_port);
                    if (p->connection_index < 0) {
                        p->connection_index = netGraph->getConnectionIndex(p->l4_dst_port);
                    }
					p->tcpFlags = hdr.extended_hdr.parsed_pkt.tcp.flags;
					p->tcpSeqNum = hdr.extended_hdr.parsed_pkt.tcp.seq_num;
					p->tcpAckNum = hdr.extended_hdr.parsed_pkt.tcp.ack_num;
					p->offsets = hdr.extended_hdr.parsed_pkt.offset;
					p->length = hdr.len;
                    p->traffic_class = hdr.extended_hdr.parsed_pkt.ip_tos >> 3;
					{
						p->interface = hdr.extended_hdr.if_index;
						struct ethhdr *eh;
						eh = (struct ethhdr *)(p->buffer);
						for (int i = 0; i < ETH_ALEN; i++) {
							eh->h_source[i] = hdr.extended_hdr.parsed_pkt.smac[i];
							eh->h_dest[i] = hdr.extended_hdr.parsed_pkt.dmac[i];
							eh->h_proto = htons(ETH_P_IP);
						}
					}
					if (recordedData->recordPackets && recordedData->recordedPacketData.count() < recordedData->recordedPacketData.capacity()) {
						RecordedPacketData data(p);
						recordedData->recordedPacketData.append(data);
					}
					packetsIn.enqueue(p);
					p = nullptr;
				} else {
					if (DEBUG_PACKETS)
						printf("Dropped packet %d.%d.%d.%d -> %d.%d.%d.%d\n",
							   HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_src.v4),
							   HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_dst.v4));
				}
			}
		} else {
			//sched_yield();
		}
	}
	malloc_profile_pause_wrapper();
    emulationDuration = get_current_time() - tsStart;

	return(NULL);
}

void print_consumer_stats()
{
    printf("===== Consumer stats =====\n");
    printf("Total packets received: %s\n", withCommas(packetsReceived));
	printf("Packets received per second: %s pps\n", withCommas(qreal(packetsReceived) * 1.0e9 / emulationDuration));
    printf("Total bytes received: %s\n", withCommas(bytesReceived));
	qreal receiveRate = qreal(bytesReceived) * 8 * 1.0e3 / emulationDuration;
	printf("Bits received per second: %s Mbps\n", withCommas(receiveRate));
	int linkSpeedMbps = getInterfaceSpeedMbps(REMOTE_DEDICATED_IF_ROUTER);
	if (linkSpeedMbps > 0) {
		printf("Interface link speed: %s Mbps\n", withCommas(linkSpeedMbps));
		if (receiveRate >= linkSpeedMbps * 0.9) {
			printf("WARNING: receive rate approaches link rate\n");
		}
	}
    printf("Jumbos received (dropped): %s\n", withCommas(jumbosReceived));
    printf("Jumbos exceeding MTU by up to 4 received (dropped) (means PMTUD enabled): %s\n", withCommas(miniJumbosReceived));

#if QUEUE_IMPL == QUEUE_IMPL_SPIN
	printf("Inter-thread communication: spinlock-protected queue\n");
#elif QUEUE_IMPL == QUEUE_IMPL_MOODY
	printf("Inter-thread communication: wait-free queue by moodycamel\n");
#elif QUEUE_IMPL == QUEUE_IMPL_DVYUKOV
	printf("Inter-thread communication: wait-free queue by Dmitry Vyukov\n");
#elif QUEUE_IMPL == QUEUE_IMPL_FOLLY
	printf("Inter-thread communication: wait-free queue by Facebook (Folly)\n");
#else
#endif
}
