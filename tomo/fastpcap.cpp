//      pcap_analyser.c
//
//      Copyright 2011 Unknown <dmende@ernw.de>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#include "fastpcap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pcap.h>
#include <arpa/inet.h>
#include <assert.h>
#include "pcap-qt.h"
#include "test.h"

#define MAX_PACKET_LEN 4096

void print_help()
{
	printf("\n-h\n");
	printf("-i <infile>\n");
	printf("-o <outfile>\n");
	printf("-f <pcap-filter>\n");
	printf("-s <search-string>\n\n");
	exit(0);
}

qint64 fastpcapCountPackets(QString fileName)
{
#ifdef PCAP_TSTAMP_PRECISION_NANO
	// open input file
	char pcap_errbuf[PCAP_ERRBUF_SIZE];

	pcap_t *pcap_handle_in = pcap_open_offline_with_tstamp_precision(fileName.toLatin1().constData(), PCAP_TSTAMP_PRECISION_NANO, pcap_errbuf);
	if (pcap_handle_in == NULL) {
		fprintf(stderr, "Couldn't open file: %s\n", pcap_errbuf);
		return 2;
	}

	qint64 count = 0;

	const u_char *pcap_packet;
	struct pcap_pkthdr *pcap_header;
	while (pcap_next_ex(pcap_handle_in, &pcap_header, &pcap_packet) > 0) {
		count++;
	}

	pcap_close(pcap_handle_in);

	return count;
#else
	return -1;
#endif
}

FastPcapReader::FastPcapReader(QString fileName)
{
	ok = false;
	end = false;
	init(fileName);
}

FastPcapReader::~FastPcapReader()
{
	if (pcap_handle_in) {
		pcap_close((pcap_t*)pcap_handle_in);
		pcap_handle_in = NULL;
	}
}

void FastPcapReader::init(QString fileName)
{
	ok = true;
	end = false;

	char pcap_errbuf[PCAP_ERRBUF_SIZE];
	pcap_handle_in = pcap_open_offline(fileName.toLatin1().constData(), pcap_errbuf);
	if (pcap_handle_in == NULL) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName << ":" << pcap_errbuf;
		ok = false;
	}

	PcapReader reader(fileName);
	header = reader.getPcapHeader();
}

bool FastPcapReader::isOk()
{
	return ok;
}

bool FastPcapReader::atEnd()
{
	return !ok || end;
}

bool FastPcapReader::readPacket(PcapPacketHeader &packetHeader, QByteArray &packet)
{
	if (!isOk() || atEnd())
		return false;

	const u_char *pcap_packet;
	struct pcap_pkthdr *pcap_header;
	if (pcap_next_ex((pcap_t*)pcap_handle_in, &pcap_header, &pcap_packet) > 0) {
		packetHeader.ts_sec = pcap_header->ts.tv_sec;
		packetHeader.ts_nsec = pcap_header->ts.tv_usec * 1000;
		packetHeader.orig_len = pcap_header->len;
		packetHeader.incl_len = pcap_header->caplen;

		if (packetHeader.orig_len > 65536 ||
			packetHeader.incl_len > 65536) {
			qDebug() << __FILE__ << __LINE__ << "Read error";
			ok = false;
			return false;
		}

		if (packet.size() != (int)packetHeader.incl_len)
			packet.resize(packetHeader.incl_len);
		memcpy(packet.data(), pcap_packet, packetHeader.incl_len);
		return true;
	}

	end = true;
	return false;
}

int FastPcapReader::getIPv4Offset(QByteArray &packet)
{
	if (getPcapHeader().network == LINKTYPE_RAW)
		return 0;
	if (getPcapHeader().network == LINKTYPE_LINUX_SLL) {
		const int sllLength = 16;
		if (packet.length() < sllLength)
			return -1;
		quint16 proto = *(const quint16 *)(packet.constData() + 14);
		proto = ntohs(proto);
		if (proto == 0x0800)
			return 16;
		return -1;
	}
	if (getPcapHeader().network == LINKTYPE_ETHERNET) {
		const int ethLength = 14;
		if (packet.length() < ethLength)
			return -1;
		quint16 proto = *(const quint16 *)(packet.constData() + 12);
		proto = ntohs(proto);
		if (proto == 0x0800)
			return 14;
		return -1;
	}
	return -1;
}

PcapHeader FastPcapReader::getPcapHeader()
{
	return header;
}

qint64 fastpcapReaderCountPackets(QString fileName)
{
	FastPcapReader pcapReader(fileName);

	qint64 numPackets = 0;

	PcapPacketHeader packetHeader;
	QByteArray packet;
	while (pcapReader.isOk() && !pcapReader.atEnd()) {
		if (pcapReader.readPacket(packetHeader, packet)) {
			numPackets++;
		}
	}

	return numPackets;
}

void testPcap(QString fileName)
{
	PcapReader slowPcapReader(fileName);
	FastPcapReader fastPcapReader(fileName);

	PcapHeader slowHeader = slowPcapReader.getPcapHeader();
	PcapHeader fastHeader = fastPcapReader.getPcapHeader();

	COMPARE(slowHeader.network, fastHeader.network);

	PcapPacketHeader slowPacketHeader;
	QByteArray slowPacket;
	PcapPacketHeader fastPacketHeader;
	QByteArray fastPacket;
	for (int i = 0; i < 100; i++) {
		bool slowOk = slowPcapReader.readPacket(slowPacketHeader, slowPacket);
		bool fastOk = fastPcapReader.readPacket(fastPacketHeader, fastPacket);
		COMPARE(slowOk, fastOk);
		COMPARE(slowPacketHeader.orig_len, fastPacketHeader.orig_len);
		COMPARE(slowPacketHeader.incl_len, fastPacketHeader.incl_len);
		COMPARE(slowPacketHeader.ts_sec, fastPacketHeader.ts_sec);
		COMPARE(slowPacketHeader.ts_nsec, fastPacketHeader.ts_nsec);
		COMPARE(slowPacket, fastPacket);
		int slowOffset = slowPcapReader.getIPv4Offset(slowPacket);
		int fastOffset = fastPcapReader.getIPv4Offset(fastPacket);
		COMPARE(slowOffset, fastOffset);
	}
}
