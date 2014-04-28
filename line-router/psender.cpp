/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	Checksum code from the Linux kernel (see the files on http://www.kernel.org/ for more details):
 *		linux/arch/x86/include/asm/checksum_64.h
 *			Checksums for x86-64
 *			Copyright 2002 by Andi Kleen, SuSE Labs
 *			with some code from asm-x86/checksum.h
 *		linux/include/net/checksum.h
 *			Checksumming functions for IP, TCP, UDP and so on
 *			Authors:
 *				Jorge Cwik, <jorge@laser.satlink.net>
 *				Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *			Borrows very liberally from tcp.c and ip.c, see those
 *			files for more names.
 *		linux/lib/checksum.c
 *			IP/TCP/UDP checksumming routines
 *			Authors:
 *				Jorge Cwik, <jorge@laser.satlink.net>
 *				Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *				Tom May, <ftom@netcom.com>
 *				Andreas Schwab, <schwab@issan.informatik.uni-dortmund.de>
 *			Lots of code moved from tcp.c and ip.c; see those files
 *			for more names.
 *		linux/net/core/utils.c
 *			Generic address resultion entity
 *			Authors:
 *				net_random Alan Cox
 *				net_ratelimit Andi Kleen
 *				in{4,6}_pton YOSHIFUJI Hideaki, Copyright (C)2006 USAGI/WIDE Project
 *			Created by Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
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

#include "psender.h"
#include "pconsumer.h"
#include "../remote_config.h"
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <unistd.h>
#include <pfring.h>

#include "../util/ovector.h"

SyncQueueType<Packet*> packetsOut;


#define __force
#define u32 __u32
#define u16 __u16
#define u8 __u8

/**
 * csum_fold - Fold and invert a 32bit checksum.
 * sum: 32bit unfolded sum
 *
 * Fold a 32bit running checksum to 16bit and invert it. This is usually
 * the last step before putting a checksum into a packet.
 * Make sure not to mix with 64bit checksums.
 */
static inline __sum16 csum_fold(__wsum sum)
{
	asm("  addl %1,%0\n"
	"  adcl $0xffff,%0"
	: "=r" (sum)
			: "r" ((__force u32)sum << 16),
			  "0" ((__force u32)sum & 0xffff0000));
	return (__force __sum16)(~(__force u32)sum >> 16);
}

static inline __wsum csum_unfold(__sum16 n)
{
	return (__force __wsum)n;
}

#ifndef do_csum
static inline unsigned short from32to16(unsigned int x)
{
	/* add up 16-bit and 16-bit for 16+c bit */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

static unsigned int do_csum(const unsigned char *buff, int len)
{
	int odd, count;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
#ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	count = len >> 1;		/* nr of 16-bit words.. */
	if (count) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			count--;
			len -= 2;
			buff += 2;
		}
		count >>= 1;		/* nr of 32-bit words.. */
		if (count) {
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *) buff;
				count--;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (count);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
#ifdef __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}
#endif

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
__wsum csum_partial(const void *buff, int len, __wsum wsum)
{
	unsigned int sum = (__force unsigned int)wsum;
	unsigned int result = do_csum((unsigned char*)buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	if (sum > result)
		result += 1;
	return (__force __wsum)result;
}

void inet_proto_csum_replace4(__sum16 *sum, __be32 from, __be32 to)
{
	__be32 diff[] = { ~from, to };
	*sum = csum_fold(csum_partial(diff, sizeof(diff), ~csum_unfold(*sum)));
}

static inline void csum_replace4(__sum16 *sum, __be32 from, __be32 to)
{
	__be32 diff[] = { ~from, to };

	*sum = csum_fold(csum_partial(diff, sizeof(diff), ~csum_unfold(*sum)));
}

void fix_addresses(Packet *p)
{
	struct ethhdr *eh;
	struct iphdr *ip;
	__be32 newaddr;

	eh = (struct ethhdr *)(p->buffer);
	for (int i = 0; i < ETH_ALEN; i++) {
		qSwap(eh->h_dest[i], eh->h_source[i]);
	}

	ip = (struct iphdr *)(p->buffer + p->offsets.l3_offset);

#if DEBUG_PACKETS
	unsigned int lsrc;
	unsigned int ldst;

	printf("fix_addresses: packet srcip %d.%d.%d.%d, dstip %d.%d.%d.%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip)); fflush(stdout);
	printf("Offsets: eth=%d, vlan=%d, L3 = %d, L4 = %d\n", p->offsets.eth_offset, p->offsets.vlan_offset, p->offsets.l3_offset, p->offsets.l4_offset);
	for (int i = 0; i < 50; i++) {
		printf("%4d", i);
	}
	printf("\n");

	for (int i = 0; i < 50; i++) {
		printf("%4x", p->buffer[i]);
	}
	printf("\n");

	// try to decode a few fields
	printf("IP version:           %d\n", p->buffer[p->offsets.l3_offset] >> 4);
	printf("IP header length (B): %d\n", (p->buffer[p->offsets.l3_offset] % 16) * 4);
	printf("IP diffserv:          %d\n", p->buffer[p->offsets.l3_offset + 1]);
	printf("IP total length (B):  %d\n", ntohs(p->buffer[p->offsets.l3_offset + 3] * 256 + p->buffer[p->offsets.l3_offset + 2]));
	printf("IP ID:                %d\n", ntohs(p->buffer[p->offsets.l3_offset + 5] * 256 + p->buffer[p->offsets.l3_offset + 4]));
	printf("IP flag R:            %d\n", p->buffer[p->offsets.l3_offset + 6] & (1 << 7));
	printf("IP flag DF:           %d\n", p->buffer[p->offsets.l3_offset + 6] & (1 << 6));
	printf("IP flag MF:           %d\n", p->buffer[p->offsets.l3_offset + 6] & (1 << 5));
	printf("IP fragment offset:   %d\n", ntohs(p->buffer[p->offsets.l3_offset + 7] + (p->buffer[p->offsets.l3_offset + 6]& 0x1F)));
	printf("IP TTL:               %d\n", p->buffer[p->offsets.l3_offset + 8]);
	printf("IP protocol:          %d\n", p->buffer[p->offsets.l3_offset + 9]);
	printf("IP header checksum:   %d\n", ntohs(p->buffer[p->offsets.l3_offset + 11] * 256 + p->buffer[p->offsets.l3_offset + 10]));
	printf("IP source address:    %d.%d.%d.%d\n", NIPQUAD(p->buffer[p->offsets.l3_offset + 12]));
	printf("IP dest.  address:    %d.%d.%d.%d\n", NIPQUAD(p->buffer[p->offsets.l3_offset + 16]));
	if (p->buffer[p->offsets.l3_offset + 9] == 17) {
		// UDP
		printf("UDP source port:      %d\n", ntohs(p->buffer[p->offsets.l4_offset + 1] * 256 + p->buffer[p->offsets.l4_offset + 0]));
		printf("UDP dest.  port:      %d\n", ntohs(p->buffer[p->offsets.l4_offset + 3] * 256 + p->buffer[p->offsets.l4_offset + 2]));
		printf("UDP length:           %d\n", ntohs(p->buffer[p->offsets.l4_offset + 5] * 256 + p->buffer[p->offsets.l4_offset + 4]));
		printf("UDP checksum:         %d\n", ntohs(p->buffer[p->offsets.l4_offset + 7] * 256 + p->buffer[p->offsets.l4_offset + 6]));
		// udpping
		printf("UDPP seqno:           %lld\n", *(quint64*)(&p->buffer[p->offsets.l4_offset + 8]));
	}

	lsrc = ntohl(ip->saddr);
	ldst = ntohl(ip->daddr);
	printf("fix_addresses: packet srcip %d.%d.%d.%d, dstip %d.%d.%d.%d, ttl %d\n", HIPQUAD(lsrc), HIPQUAD(ldst), ip->ttl); fflush(stdout);
#endif

    newaddr = ip->saddr | NAT_FOREIGN;
	if (ip->protocol == IPPROTO_UDP) {
		struct udphdr *udp = (struct udphdr *)(((u8*)ip) + ip->ihl*4);
		if (udp->check)
			inet_proto_csum_replace4(&udp->check, ip->saddr, newaddr);
	} else if (ip->protocol == IPPROTO_TCP) {
		struct tcphdr *tcp = (struct tcphdr *)(((u8*)ip) + ip->ihl*4);
		inet_proto_csum_replace4(&tcp->check, ip->saddr, newaddr);
	}
	csum_replace4(&ip->check, ip->saddr, newaddr);
	ip->saddr = newaddr;

    newaddr = ip->daddr & ~(NAT_FOREIGN);
	if (ip->protocol == IPPROTO_UDP) {
		struct udphdr *udp = (struct udphdr *)(((u8*)ip) + ip->ihl*4);
		if (udp->check)
			inet_proto_csum_replace4(&udp->check, ip->daddr, newaddr);
	} else if (ip->protocol == IPPROTO_TCP) {
		struct tcphdr *tcp = (struct tcphdr *)(((u8*)ip) + ip->ihl*4);
		inet_proto_csum_replace4(&tcp->check, ip->daddr, newaddr);
	}
	csum_replace4(&ip->check, ip->daddr, newaddr);
	ip->daddr = newaddr;

#if DEBUG_PACKETS
	lsrc = ntohl(ip->saddr);
	ldst = ntohl(ip->daddr);
	printf("fix_addresses: packet srcip %d.%d.%d.%d, dstip %d.%d.%d.%d, ttl %d\n", HIPQUAD(lsrc), HIPQUAD(ldst), ip->ttl); fflush(stdout);
#endif
}

void set_ip_ecn_bit(Packet *p)
{
	struct iphdr *ip = (struct iphdr *)(p->buffer + p->offsets.l3_offset);

#if DEBUG_PACKETS
	unsigned int lsrc;
	unsigned int ldst;

	printf("set_ip_ecn_bit: packet srcip %d.%d.%d.%d, dstip %d.%d.%d.%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip)); fflush(stdout);
	printf("Offsets: eth=%d, vlan=%d, L3 = %d, L4 = %d\n", p->offsets.eth_offset, p->offsets.vlan_offset, p->offsets.l3_offset, p->offsets.l4_offset);
	for (int i = 0; i < 50; i++) {
		printf("%4d", i);
	}
	printf("\n");

	for (int i = 0; i < 50; i++) {
		printf("%4x", p->buffer[i]);
	}
	printf("\n");

	// try to decode a few fields
	printf("IP version:           %d\n", p->buffer[p->offsets.l3_offset] >> 4);
	printf("IP header length (B): %d\n", (p->buffer[p->offsets.l3_offset] % 16) * 4);
	printf("IP diffserv:          %d\n", p->buffer[p->offsets.l3_offset + 1]);
	printf("IP total length (B):  %d\n", ntohs(p->buffer[p->offsets.l3_offset + 3] * 256 + p->buffer[p->offsets.l3_offset + 2]));
	printf("IP ID:                %d\n", ntohs(p->buffer[p->offsets.l3_offset + 5] * 256 + p->buffer[p->offsets.l3_offset + 4]));
	printf("IP flag R:            %d\n", p->buffer[p->offsets.l3_offset + 6] & (1 << 7));
	printf("IP flag DF:           %d\n", p->buffer[p->offsets.l3_offset + 6] & (1 << 6));
	printf("IP flag MF:           %d\n", p->buffer[p->offsets.l3_offset + 6] & (1 << 5));
	printf("IP fragment offset:   %d\n", ntohs(p->buffer[p->offsets.l3_offset + 7] + (p->buffer[p->offsets.l3_offset + 6]& 0x1F)));
	printf("IP TTL:               %d\n", p->buffer[p->offsets.l3_offset + 8]);
	printf("IP protocol:          %d\n", p->buffer[p->offsets.l3_offset + 9]);
	printf("IP header checksum:   %d\n", ntohs(p->buffer[p->offsets.l3_offset + 11] * 256 + p->buffer[p->offsets.l3_offset + 10]));
	printf("IP source address:    %d.%d.%d.%d\n", NIPQUAD(p->buffer[p->offsets.l3_offset + 12]));
	printf("IP dest.  address:    %d.%d.%d.%d\n", NIPQUAD(p->buffer[p->offsets.l3_offset + 16]));
	if (p->buffer[p->offsets.l3_offset + 9] == 17) {
		// UDP
		printf("UDP source port:      %d\n", ntohs(p->buffer[p->offsets.l4_offset + 1] * 256 + p->buffer[p->offsets.l4_offset + 0]));
		printf("UDP dest.  port:      %d\n", ntohs(p->buffer[p->offsets.l4_offset + 3] * 256 + p->buffer[p->offsets.l4_offset + 2]));
		printf("UDP length:           %d\n", ntohs(p->buffer[p->offsets.l4_offset + 5] * 256 + p->buffer[p->offsets.l4_offset + 4]));
		printf("UDP checksum:         %d\n", ntohs(p->buffer[p->offsets.l4_offset + 7] * 256 + p->buffer[p->offsets.l4_offset + 6]));
		// udpping
		printf("UDPP seqno:           %lld\n", *(quint64*)(&p->buffer[p->offsets.l4_offset + 8]));
	}

	lsrc = ntohl(ip->saddr);
	ldst = ntohl(ip->daddr);
	printf("set_ip_ecn_bit: packet srcip %d.%d.%d.%d, dstip %d.%d.%d.%d, ttl %d\n", HIPQUAD(lsrc), HIPQUAD(ldst), ip->ttl); fflush(stdout);
#endif

	if (ip->tos & 2) {
		__be32 old_word = *(__be32*)(ip);

		struct iphdr ip_new = *ip;
		ip_new.tos |= 1;

		__be32 new_word = *(__be32*)(&ip_new);

		csum_replace4(&ip->check, old_word, new_word);
		ip->tos |= 1;
	}

#if DEBUG_PACKETS
	lsrc = ntohl(ip->saddr);
	ldst = ntohl(ip->daddr);
	printf("set_ip_ecn_bit: packet srcip %d.%d.%d.%d, dstip %d.%d.%d.%d, ttl %d\n", HIPQUAD(lsrc), HIPQUAD(ldst), ip->ttl); fflush(stdout);
#endif
}

quint64 packetsSent;
quint64 tsFirstSentPacket;
quint64 packetsSentStatsCount;
quint64 packetsSentErr10p;
quint64 packetsSentErr25p;
quint64 packetsSentErr50p;
quint64 packetsSentErrpMax;
quint64 packetsSentErrAvg;
quint64 packetsSentSendDelayAvg;
quint64 packetsSentSendDelayMax;
quint64 packetsSentSendDelayRelAvg;
quint64 packetsSentSendDelayRelMax;

bool send_packet(pfring *pd, Packet *p)
{
	quint64 ts_now = get_current_time();
	p->ts_send = ts_now;

	if (!p->preparedForSend) {
		fix_addresses(p);
		if (p->ecn_bit_set) {
			set_ip_ecn_bit(p);
		}
		p->preparedForSend = true;
	}

	while (1) {
		// 1 = Flush possible transmission queues. If set to 0, you will decrease
		// your CPU usage but at thecost of sending packets in trains and thus at
		// larger latency
		const int flush_packets = 0;
		int rc = pfring_send(pd, (char*)p->buffer, p->length, flush_packets);
		if (rc == PF_RING_ERROR_INVALID_ARGUMENT) {
			printf("Could not send packet: PF_RING_ERROR_INVALID_ARGUMENT\n");
			exit(EXIT_FAILURE);
		} else if (rc < 0) {
			// Not enough space in buffer
			usleep(1);
			continue;
		}
		break;
	}

	if (tsFirstSentPacket == 0) {
		tsFirstSentPacket = ts_now;
		__sync_synchronize();
	}
	packetsSent++;

	if (DEBUG_PACKETS)
		printf("Sent packet with length %d\n",
			   p->length - p->offsets.l3_offset);

	if (DEBUG_PACKETS)
		printf("Packet theor.delay = %llu ns, actual delay = %llu ns, error = %llu ns\n",
			   p->theoretical_delay,
			   p->ts_send - p->ts_userspace_rx,
			   p->ts_send - p->ts_userspace_rx - p->theoretical_delay);

	// update delay error stats
	if (ts_now - tsFirstSentPacket > RECORD_STATS_DELAY) {
		packetsSentStatsCount++;

		quint64 err = p->ts_start_send - p->ts_userspace_rx - p->theoretical_delay;
		quint64 errPercent = (err * 100)/p->theoretical_delay;
		packetsSentErrAvg += errPercent;

		if (errPercent >= 10) {
			packetsSentErr10p++;
		}

		if (errPercent >= 25) {
			packetsSentErr25p++;
		}

		if (errPercent >= 50) {
			packetsSentErr50p++;
			// printf("Packet theor.delay =  "TS_FORMAT" , actual delay =  "TS_FORMAT" , error =  "TS_FORMAT"  (%llu%%)\n", TS_FORMAT_PARAM(p->theoretical_delay), TS_FORMAT_PARAM(p->ts_send - p->ts_userspace_rx), TS_FORMAT_PARAM(err), errPercent);
		}

		packetsSentErrpMax = qMax(packetsSentErrpMax, errPercent);

		quint64 sendDelay = p->ts_send - p->ts_start_send;
		packetsSentSendDelayAvg += sendDelay;
		packetsSentSendDelayMax = qMax(packetsSentSendDelayMax, sendDelay);

		quint64 rel = (sendDelay * 100) / p->theoretical_delay;
		packetsSentSendDelayRelAvg += rel;
		packetsSentSendDelayRelMax = qMax(packetsSentSendDelayRelMax, rel);
	}

	return true;
}

static quint64 bytesSent;
static quint64 tsStart;
static quint64 emulationDuration;

void* packet_sender_thread(void* )
{
	barrierInit.wait();
	__sync_synchronize();

	pthread_setname_np(pthread_self(), "line-packet-sender");

	u_int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
	u_long core_id = CORE_SENDER;

	if (bind2core(core_id) == 0) {
		printf("Set thread sender affinity to core %lu/%u\n", core_id, numCPU);
	} else {
		printf("Failed to set thread sender affinity to core %lu/%u\n", core_id, numCPU);
	}

	warmMallocCache();

	pfring *pd = pfring_open(REMOTE_DEDICATED_IF_ROUTER, 1500, 0);
	if (pd == NULL) {
		printf("pfring_open %s error [%s]\n", REMOTE_DEDICATED_IF_ROUTER, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!pd->send && pd->send_ifindex) {
		printf("if index problem\n");
		exit(EXIT_FAILURE);
	}

	pfring_set_socket_mode(pd, send_only_mode);

	if (pfring_enable_ring(pd) != 0) {
		printf("Unable to enable ring :-(\n");
		pfring_close(pd);
		exit(EXIT_FAILURE);
	}

	packetsSent = 0;
	packetsSentStatsCount = 0;
	packetsSentErr10p = 0;
	packetsSentErr25p = 0;
	packetsSentErr50p = 0;
	packetsSentErrpMax = 0;
	packetsSentErrAvg = 0;
	packetsSentSendDelayAvg = 0;
	packetsSentSendDelayMax = 0;
	packetsSentSendDelayRelAvg = 0;
	packetsSentSendDelayRelMax = 0;
    bytesSent = 0;

	OVector<Packet*> newPackets;
    newPackets.reserve(1000);

	barrierInitDone.wait();
	barrierStart.wait();

	tsStart = get_current_time();

	while (1) {
		if (do_shutdown) {
			break;
		}

		// process new packets
		packetsOut.dequeueAll(newPackets);

		if (!newPackets.isEmpty()) {
			for (int iPacket = 0; iPacket < newPackets.count(); iPacket++) {
				Packet *p = newPackets[iPacket];
				if (!p->dropped && send_packet(pd, p)) {
					bytesSent += p->length;
				}
			}
			packetPool.enqueue(newPackets);
			newPackets.clear();
		} else {
			//sched_yield();
		}
	}
	malloc_profile_pause_wrapper();

	pfring_close(pd);

	emulationDuration = get_current_time() - tsStart;

    return NULL;
}

void print_sender_stats()
{
    printf("===== Sender stats =====\n");
	printf("Total packets sent: %s\n",
		   withCommas(packetsSent));
	printf("Packets sent per second: %s p/s\n",
		   withCommas(qreal(packetsSent) * 1.0e9 / emulationDuration));
	printf("Total bytes sent: %s\n",
		   withCommas(bytesSent));
	printf("Bits sent per second: %s Mbps\n",
		   withCommas(qreal(bytesSent) * 8 * 1.0e3 / emulationDuration));
	printf("Total packets sent with delay error > 10%%: %s (%f%% of total packets)\n",
		   withCommas(packetsSentErr10p),
		   packetsSentStatsCount ? (packetsSentErr10p * 100.0)/packetsSentStatsCount : 0);
	printf("Total packets sent with delay error > 25%%: %s (%f%% of total packets)\n",
		   withCommas(packetsSentErr25p),
		   packetsSentStatsCount ? (packetsSentErr25p * 100.0)/packetsSentStatsCount : 0);
	printf("Total packets sent with delay error > 50%%: %s (%f%% of total packets)\n",
		   withCommas(packetsSentErr50p),
		   packetsSentStatsCount ? (packetsSentErr50p * 100.0)/packetsSentStatsCount : 0);
	printf("Average relative delay error: %f%%, Maximum relative delay error: %llu%%\n",
		   packetsSentStatsCount ? (packetsSentErrAvg / (float)packetsSentStatsCount) : 0,
		   packetsSentErrpMax);
	printf("Send delay: avg " TS_FORMAT ", max " TS_FORMAT "\n",
		   TS_FORMAT_PARAM(packetsSentSendDelayAvg / qMax(packetsSentStatsCount, 1ULL)),
		   TS_FORMAT_PARAM(packetsSentSendDelayMax));
	printf("Send delay (relative to theoretical, ideally 0): avg %llu%%, max %llu%%\n",
		   packetsSentSendDelayRelAvg / qMax(packetsSentStatsCount, 1ULL),
		   packetsSentSendDelayRelMax);
}
