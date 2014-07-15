#include "traffictrace.h"

#include <stdint.h>

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

bool TrafficTrace::loadFromPcap()
{
	packets.clear();

	QString fileName;

	if (QFile::exists(pcapFileName)) {
		fileName = pcapFileName;
	} else if (QFile::exists(pcapFullFilePath)) {
		fileName = pcapFullFilePath;
	} else if (QFile::exists(QDir::homePath() + "/" + pcapFileName)) {
		fileName = QDir::homePath() + "/" + pcapFileName;
	} else {
		qDebug() << "Could not open pcap file" << pcapFileName << pcapFullFilePath;
		return false;
	}

	PcapReader pcapReader(fileName);

	quint64 tsStart = 0;
	while (pcapReader.isOk() && !pcapReader.atEnd()) {
		PcapPacketHeader packetHeader;
		QByteArray packet;
		if (pcapReader.readPacket(packetHeader, packet)) {
			quint64 ts = quint64(packetHeader.ts_sec) * 1000ULL * 1000ULL * 1000ULL +
						 quint64(packetHeader.ts_nsec);
			if (tsStart == 0) {
				tsStart = ts;
			}
			ts -= tsStart;

			TrafficTracePacket tracePacket;
			tracePacket.timestamp = ts;
			tracePacket.size = packetHeader.orig_len;
			packets << tracePacket;
		}
	}

	return pcapReader.isOk();
}

void TrafficTrace::setPcapFilePath(QString pcapFilePath)
{
	pcapFullFilePath = pcapFilePath;
	pcapFileName = QString(pcapFilePath).split("/").last();
}

void TrafficTrace::clear()
{
	packets.clear();
}

TrafficTrace TrafficTrace::generateFromPcap(QString pcapFileName, qint32 link, bool &ok)
{
	ok = true;
	TrafficTrace trace(link);
	trace.setPcapFilePath(pcapFileName);
	trace.loadFromPcap();
	return trace;
}

QDataStream& operator<<(QDataStream& s, const TrafficTrace& d)
{
	qint8 ver = 3;

	s << ver;

	s << d.packets;
	s << d.link;
	s << d.pcapFileName;
	s << d.pcapFullFilePath;

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

	if (ver >= 3) {
		s >> d.pcapFileName;
		s >> d.pcapFullFilePath;
	} else {
		d.pcapFileName.clear();
		d.pcapFullFilePath.clear();
	}

	if (ver > 3) {
		qDebug() << __FILE__ << __LINE__ << "read error";
		s.setStatus(QDataStream::ReadCorruptData);
	}
	return s;
}

QDataStream& operator<<(QDataStream& s, const TrafficTracePacketRecord& d)
{
	qint8 ver = 1;

	s << ver;

	s << d.traceIndex;
	s << d.packetIndex;
	s << d.injectionTime;
	s << d.exitTime;
	s << d.theoreticalDelay;

	return s;
}

QDataStream& operator>>(QDataStream& s, TrafficTracePacketRecord& d)
{
	qint8 ver = 0;

	s >> ver;

	if (ver <= 1) {
		s >> d.traceIndex;
		s >> d.packetIndex;
		s >> d.injectionTime;
		s >> d.exitTime;
		s >> d.theoreticalDelay;
	}

	if (ver > 1) {
		qDebug() << __FILE__ << __LINE__ << "read error";
		s.setStatus(QDataStream::ReadCorruptData);
	}
	return s;
}

QDataStream& operator<<(QDataStream& s, const TrafficTraceRecord& d)
{
	qint8 ver = 2;

	s << ver;

	s << d.events;
	s << d.tsStart;

	return s;
}

QDataStream& operator>>(QDataStream& s, TrafficTraceRecord& d)
{
	qint8 ver = 0;

	s >> ver;

	if (1 <= ver && ver <= 2) {
		s >> d.events;
	}
	if (2 <= ver && ver <= 2) {
		s >> d.tsStart;
	}

	if (ver > 2) {
		qDebug() << __FILE__ << __LINE__ << "read error";
		s.setStatus(QDataStream::ReadCorruptData);
	}
	return s;
}


bool TrafficTraceRecord::save(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << *this;

	if (out.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error writing file:" << file.fileName();
		return false;
	}
	return true;
}

bool TrafficTraceRecord::load(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;

	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error reading file:" << file.fileName();
		return false;
	}
	return true;
}

bool TrafficTraceRecord::rawLoad(const char *fileName)
{
	{
		events.clear();
	}

	FILE *f = fopen(fileName, "rb");
	if (!f) {
		fprintf(stderr, "Could not open file\n");
		return false;
	}

	u_int8_t version;
	if (fread(&version, sizeof(version), 1, f) != 1) {
		fprintf(stderr, "Read error\n");
		fclose(f);
		return false;
	}

	if (version != 2) {
		fprintf(stderr, "Read error\n");
		fclose(f);
		return false;
	}

	u_int32_t numEvents;
	if (fread(&numEvents, sizeof(numEvents), 1, f) != 1) {
		fprintf(stderr, "Read error\n");
		fclose(f);
		return false;
	}
	numEvents = be32toh(numEvents);

	for (u_int32_t iEvent = 0; iEvent < numEvents; iEvent++) {
		u_int8_t version;
		if (fread(&version, sizeof(version), 1, f) != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}

		if (version != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}

		// Index of the traffic trace (assuming a vector of traces is given)
		int32_t traceIndex;
		if (fread(&traceIndex, sizeof(traceIndex), 1, f) != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}
		traceIndex = be32toh(traceIndex);

		// Index of the packet in the traffic trace
		int32_t packetIndex;
		if (fread(&packetIndex, sizeof(packetIndex), 1, f) != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}
		packetIndex = be32toh(packetIndex);

		// Timestamp of the actual time the packet was injected
		u_int64_t injectionTime;
		if (fread(&injectionTime, sizeof(injectionTime), 1, f) != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}
		injectionTime = be64toh(injectionTime);

		// Timestamp of the actual time the packet was received by the next link, or 0 if the packet was dropped
		u_int64_t exitTime;
		if (fread(&exitTime, sizeof(exitTime), 1, f) != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}
		exitTime = be64toh(exitTime);

		// The theoretical delay experienced by the packet, equal to queuing delay + transmission delay + propagation delay
		u_int64_t theoreticalDelay;
		if (fread(&theoreticalDelay, sizeof(theoreticalDelay), 1, f) != 1) {
			fprintf(stderr, "Read error\n");
			fclose(f);
			return false;
		}
		theoreticalDelay = be64toh(theoreticalDelay);

		{
			TrafficTracePacketRecord record;
			record.traceIndex = traceIndex;
			record.packetIndex = packetIndex;
			record.injectionTime = injectionTime;
			record.exitTime = exitTime;
			record.theoreticalDelay = theoreticalDelay;
			events.append(record);
		}
	}

	// The time at which the simulation starts
	u_int64_t timestampStart;
	if (fread(&timestampStart, sizeof(timestampStart), 1, f) != 1) {
		fprintf(stderr, "Read error\n");
		fclose(f);
		return false;
	}
	timestampStart = be64toh(timestampStart);

	{
		tsStart = timestampStart;
	}

	fclose(f);
	return true;
}
