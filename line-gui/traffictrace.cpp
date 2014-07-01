#include "traffictrace.h"

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
	// TODO
	return trace;
}

QDataStream& operator<<(QDataStream& s, const TrafficTrace& d)
{
	qint8 ver = 1;

	s << ver;

	s << d.packets;

	return s;
}

QDataStream& operator>>(QDataStream& s, TrafficTrace& d)
{
	qint8 ver = 0;

	s >> ver;

	if (ver <= 1) {
		s >> d.packets;
	}

	if (ver > 1) {
		qDebug() << __FILE__ << __LINE__ << "read error";
		s.setStatus(QDataStream::ReadCorruptData);
	}
	return s;
}
