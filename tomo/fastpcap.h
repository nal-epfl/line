#ifndef FASTPCAP_H
#define FASTPCAP_H

#include <QtCore>

#include "pcap-common.h"

qint64 fastpcapCountPackets(QString fileName);

class FastPcapReader
{
	public:
	FastPcapReader(QString fileName);
	~FastPcapReader();

	void init(QString fileName);

	bool isOk();
	bool atEnd();
	bool readPacket(PcapPacketHeader &packetHeader, QByteArray &packet);
	int getIPv4Offset(QByteArray &packet);

	PcapHeader getPcapHeader();

	protected:
	bool end;
	bool ok;
	PcapHeader header;
	void *pcap_handle_in;
};

qint64 fastpcapReaderCountPackets(QString fileName);

void testPcap(QString fileName);

#endif // FASTPCAP_H
