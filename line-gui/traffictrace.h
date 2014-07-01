#ifndef TRAFFICTRACE_H
#define TRAFFICTRACE_H

#include <QtCore>

class TrafficTracePacket
{
public:
	quint64 timestamp;
	quint16 size;
};

QDataStream& operator>>(QDataStream& s, TrafficTracePacket& d);
QDataStream& operator<<(QDataStream& s, const TrafficTracePacket& d);

class TrafficTrace
{
public:
	TrafficTrace(qint32 link = -1);

	QVector<TrafficTracePacket> packets;
	qint32 link;

	static TrafficTrace generateFromPcap(QString pcapFileName, qint32 link, bool &ok);
};

QDataStream& operator>>(QDataStream& s, TrafficTrace& d);
QDataStream& operator<<(QDataStream& s, const TrafficTrace& d);

#endif // TRAFFICTRACE_H
