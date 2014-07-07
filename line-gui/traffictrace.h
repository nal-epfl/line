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


class TrafficTracePacketRecord
{
public:
    // Index of the traffic trace (assuming a vector of traces is given)
    qint32 traceIndex;
    // Index of the packet in the traffic trace
    qint32 packetIndex;
    // Timestamp of the actual time the packet was injected
    quint64 injectionTime;
    // Timestamp of the actual time the packet was received by the next link, or 0 if the packet was dropped
    quint64 exitTime;
    // The theoretical delay experienced by the packet, equal to queuing delay + transmission delay + propagation delay
    quint64 theoreticalDelay;
};

QDataStream& operator>>(QDataStream& s, TrafficTracePacket& d);
QDataStream& operator<<(QDataStream& s, const TrafficTracePacket& d);

class TrafficTraceRecord
{
public:
    QVector<TrafficTracePacketRecord> events;

    bool save(QString fileName);
    bool load(QString fileName);
};

QDataStream& operator>>(QDataStream& s, TrafficTraceRecord& d);
QDataStream& operator<<(QDataStream& s, const TrafficTraceRecord& d);

#endif // TRAFFICTRACE_H
