#include "traffictrace.h"

#include "../tomo/pcap-qt.h"

QDataStream& operator<<(QDataStream& s, const TrafficTracePacket& d)
{
	qint8 ver = 1;

	s << ver;

	s << d.timestamp;
	s << d.size;

	return s;
}

QDataStream& operator>>(QDataStream& s, TrafficTracePacket& d)
{
	qint8 ver = 0;

	s >> ver;

	if (ver <= 1) {
		s >> d.timestamp;
		s >> d.size;
	}

	if (ver > 1) {
		qDebug() << __FILE__ << __LINE__ << "read error";
		s.setStatus(QDataStream::ReadCorruptData);
	}
	return s;
}

TrafficTrace::TrafficTrace(qint32 link)
	: link(link)
{}

TrafficTrace TrafficTrace::generateFromPcap(QString pcapFileName, qint32 link, bool &ok)
{
	ok = true;
	TrafficTrace trace(link);

	PcapReader pcapReader(pcapFileName);

	quint64 tsStart = 0;

	while (pcapReader.isOk() && !pcapReader.atEnd()) {
		PcapPacketHeader packetHeader;
		QByteArray packet;
		if (pcapReader.readPacket(packetHeader, packet)) {
			quint64 ts = quint64(packetHeader.ts_sec) * 1000ULL * 1000ULL * 1000ULL +
						 quint64(packetHeader.ts_usec) * 1000ULL;
			if (tsStart == 0) {
				tsStart = ts;
			}
			ts -= tsStart;

			TrafficTracePacket tracePacket;
			tracePacket.timestamp = ts;
			tracePacket.size = packetHeader.orig_len;
			trace.packets << tracePacket;
		}
	}

	return trace;
}

QDataStream& operator<<(QDataStream& s, const TrafficTrace& d)
{
	qint8 ver = 2;

	s << ver;

	s << d.packets;
	s << d.link;

	return s;
}

QDataStream& operator>>(QDataStream& s, TrafficTrace& d)
{
	qint8 ver = 0;

	s >> ver;

	if (ver >= 1) {
		s >> d.packets;
	}

	if (ver >= 2) {
		s >> d.link;
	} else {
		d.link = 0;
	}

	if (ver > 2) {
		qDebug() << __FILE__ << __LINE__ << "read error";
		s.setStatus(QDataStream::ReadCorruptData);
	}
	return s;
}
