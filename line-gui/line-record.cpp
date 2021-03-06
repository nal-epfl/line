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

#ifdef LINE_EMULATOR
#include "../line-router/pconsumer.h"
#endif

#include "line-record.h"
#include "debug.h"

RecordedData::RecordedData()
{
	recordPackets = false;
	saturated = false;
	samplingPeriod = 0;

#ifndef LINE_EMULATOR
	file = NULL;
#endif
}

RecordedData::~RecordedData()
{
#ifndef LINE_EMULATOR
	delete file;
	file = NULL;
#endif
}

#ifdef LINE_EMULATOR
bool RecordedData::save(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << recordPackets;
	out << recordedPacketData;
	out << recordedQueuedPacketData;
	saturated = recordedPacketData.count() == recordedPacketData.capacity() ||
				recordedQueuedPacketData.count() == recordedQueuedPacketData.capacity();
	out << saturated;

	if (out.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error writing file:" << file.fileName();
		return false;
	}
	return true;
}
#else
bool RecordedData::load(QString fileName)
{
	delete file;
	file = new QFile(fileName);
	if (!file->open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file->fileName();
		return false;
	}

	QDataStream in(file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> recordPackets;
	recordedPacketData.init(file, RecordedPacketData::getSerializedSize());
	recordedQueuedPacketData.init(file, RecordedQueuedPacketData::getSerializedSize());
	in >> saturated;

	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error reading file:" << file->fileName();
		return false;
	}
	return true;
}
#endif
RecordedPacketData RecordedData::packetByID(quint64 packetID)
{
	qint64 i = packetIndexByID(packetID);
	if (i < 0)
		return RecordedPacketData();
	return recordedPacketData[i];
	return RecordedPacketData();
}

qint64 RecordedData::packetIndexByID(quint64 packetID)
{
	if (packetID2Index.isEmpty()) {
		for (qint64 i = 0; i < recordedPacketData.count(); i++) {
			packetID2Index[recordedPacketData[i].packet_id] = i;
		}
	}
	if (packetID2Index.contains(packetID)) {
		return packetID2Index[packetID];
	}
	return -1;
}

QList<RecordedQueuedPacketData> RecordedData::queueEventsByPacketID(quint64 packetID)
{
	if (packetID2QueueEvents.isEmpty()) {
		for (qint64 i = 0; i < recordedQueuedPacketData.count(); i++) {
			packetID2QueueEvents[recordedQueuedPacketData[i].packet_id] << recordedQueuedPacketData[i];
		}
	}
	if (packetID2QueueEvents.contains(packetID)) {
		return packetID2QueueEvents[packetID];
	}
	return QList<RecordedQueuedPacketData>();
}

RecordedPacketData::RecordedPacketData() {
	packet_id = 0xffFFffFFffFFffFFULL;
	src_id = 0xffFFffFF;
	dst_id = 0xffFFffFF;
	ts_userspace_rx = 0xffFFffFFffFFffFFULL;
	memset(buffer, 0, CAPTURE_LENGTH);
}

bool RecordedPacketData::isNull()
{
	return packet_id == 0xffFFffFFffFFffFFULL;
}

#ifdef LINE_EMULATOR
RecordedPacketData::RecordedPacketData(Packet *p) {
	packet_id = p->id;
	src_id = p->src_id;
	dst_id = p->dst_id;
	ts_userspace_rx = p->ts_userspace_rx;
	quint8 *ip = p->buffer + p->offsets.l3_offset;
	memcpy(buffer, ip, CAPTURE_LENGTH);
}
#endif

QDataStream& operator<<(QDataStream& s, const RecordedPacketData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.packet_id;
	s << d.src_id;
	s << d.dst_id;

	s << d.ts_userspace_rx;
	QByteArray bytes((const char*)d.buffer, CAPTURE_LENGTH);
	s << bytes;

	return s;
}

QDataStream& operator>>(QDataStream& s, RecordedPacketData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.packet_id;
	s >> d.src_id;
	s >> d.dst_id;

	s >> d.ts_userspace_rx;
	QByteArray bytes;
	s >> bytes;
	memcpy(d.buffer, bytes.constData(), CAPTURE_LENGTH);

	return s;
}

qint64 RecordedPacketData::getSerializedSize()
{
	QByteArray buffer;
	QDataStream stream(&buffer, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_4_0);
	RecordedPacketData dummy;
	stream << dummy;
	return buffer.length();
}

QDataStream& operator<<(QDataStream& s, const RecordedQueuedPacketData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.packet_id;
	s << d.edge_index;
	s << d.ts_enqueue;
	s << d.qcapacity;
	s << d.qload;
	s << d.decision;
	s << d.ts_exit;

	return s;
}

QDataStream& operator>>(QDataStream& s, RecordedQueuedPacketData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.packet_id;
	s >> d.edge_index;
	s >> d.ts_enqueue;
	s >> d.qcapacity;
	s >> d.qload;
	s >> d.decision;
	s >> d.ts_exit;

	return s;
}

qint64 RecordedQueuedPacketData::getSerializedSize()
{
	QByteArray buffer;
	QDataStream stream(&buffer, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_4_0);
	RecordedQueuedPacketData dummy;
	stream << dummy;
	return buffer.length();
}
