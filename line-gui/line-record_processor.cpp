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

#include "line-record_processor.h"

#include "line-record.h"
#include "../util/util.h"
#include "netgraph.h"
#include "../tomo/tomodata.h"
#include "../tomo/readpacket.h"
#include <netinet/ip.h>
#include "../tomo/pcap-qt.h"

quint64 ns2ms(quint64 time)
{
	return time / 1000000ULL;
}

quint64 ns2us(quint64 time)
{
	return time / 1000000ULL;
}

bool loadGraph(QString expPath, NetGraph &g)
{
	QString graphName;
	if (!readFile(expPath + "/" + "simulation.txt", graphName, true)) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not read simulation.txt";
		return false;
	}
	graphName = graphName.replace("graph=", "");

	g.setFileName(expPath + "/" + graphName + ".graph");
	if (!g.loadFromFile()) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not load graph";
		return false;
	}
	return true;
}

bool createPcapFiles(QString expPath, RecordedData rec, NetGraph g)
{
	// Create 3 files per link: input, dropped, output
	foreach (NetGraphEdge e, g.edges) {
		QList<RecordedQueuedPacketData> edgeEvents;
		foreach (RecordedQueuedPacketData qe, rec.recordedQueuedPacketData) {
			if (qe.edge_index == e.index) {
				edgeEvents << qe;
			}
		}

		QByteArray pcapBytesIn;
		QByteArray pcapBytesOut;
		QByteArray pcapBytesDrop;

		PcapHeader pcapHeader;
		pcapHeader.magic_number = 0xa1b2c3d4;
		pcapHeader.version_major = 2;
		pcapHeader.version_minor = 4;
		pcapHeader.thiszone = 0;
		pcapHeader.sigfigs = 0;
		pcapHeader.snaplen = CAPTURE_LENGTH;
		pcapHeader.network = LINKTYPE_RAW;

		pcapBytesIn.append((const char*)&pcapHeader, sizeof(PcapHeader));
		pcapBytesOut.append((const char*)&pcapHeader, sizeof(PcapHeader));
		pcapBytesDrop.append((const char*)&pcapHeader, sizeof(PcapHeader));

		bool warnNullPacket = true;
		foreach (RecordedQueuedPacketData qe, edgeEvents) {
			RecordedPacketData p = rec.packetByID(qe.packet_id);
			if (p.isNull()) {
				if (warnNullPacket) {
					qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
					warnNullPacket = false;
				}
				continue;
			}
			PcapPacketHeader pcapPacketHeader;
			pcapPacketHeader.ts_usec = quint32((qe.ts_enqueue / 1000ULL) % 1000000ULL);
			pcapPacketHeader.ts_sec = quint32((qe.ts_enqueue / 1000ULL) / 1000000ULL);
			pcapPacketHeader.incl_len = CAPTURE_LENGTH;
			IPHeader iph;
			TCPHeader tcph;
			UDPHeader udph;
			if (decodePacket(QByteArray((const char*)p.buffer, CAPTURE_LENGTH), iph, tcph, udph)) {
				pcapPacketHeader.orig_len = iph.totalLength;
			} else {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse the IP header for a packet";
				pcapPacketHeader.orig_len = CAPTURE_LENGTH; // not correct
			}

			// input
			pcapBytesIn.append((const char*)&pcapPacketHeader, sizeof(PcapPacketHeader));
			pcapBytesIn.append((const char*)p.buffer, CAPTURE_LENGTH);
			if (qe.decision != 0) {
				// drop
				pcapBytesDrop.append((const char*)&pcapPacketHeader, sizeof(PcapPacketHeader));
				pcapBytesDrop.append((const char*)p.buffer, CAPTURE_LENGTH);
			} else {
				// output
				pcapPacketHeader.ts_usec = quint32((qe.ts_exit / 1000ULL) % 1000000ULL);
				pcapPacketHeader.ts_sec = quint32((qe.ts_exit / 1000ULL) / 1000000ULL);
				pcapBytesOut.append((const char*)&pcapPacketHeader, sizeof(PcapPacketHeader));
				pcapBytesOut.append((const char*)p.buffer, CAPTURE_LENGTH);
			}
		}

        saveFile(expPath + "/" + QString("edge-%1-in.pcap").arg(e.index + 1), pcapBytesIn);
        saveFile(expPath + "/" + QString("edge-%1-out.pcap").arg(e.index + 1), pcapBytesOut);
        saveFile(expPath + "/" + QString("edge-%1-drop.pcap").arg(e.index + 1), pcapBytesDrop);
	}

	// Create one global pcap file with everything that entered the emulator
	{
		QByteArray pcapBytesIn;

		PcapHeader pcapHeader;
		pcapHeader.magic_number = 0xa1b2c3d4;
		pcapHeader.version_major = 2;
		pcapHeader.version_minor = 4;
		pcapHeader.thiszone = 0;
		pcapHeader.sigfigs = 0;
		pcapHeader.snaplen = CAPTURE_LENGTH;
		pcapHeader.network = LINKTYPE_RAW;
		pcapBytesIn.append((const char*)&pcapHeader, sizeof(PcapHeader));

		foreach (RecordedPacketData p, rec.recordedPacketData) {
			if (p.isNull()) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
				return false;
			}
			PcapPacketHeader pcapPacketHeader;
			pcapPacketHeader.ts_usec = quint32((p.ts_userspace_rx / 1000ULL) % 1000000ULL);
			pcapPacketHeader.ts_sec = quint32((p.ts_userspace_rx / 1000ULL) / 1000000ULL);
			pcapPacketHeader.incl_len = CAPTURE_LENGTH;
			IPHeader iph;
			TCPHeader tcph;
			UDPHeader udph;
			if (decodePacket(QByteArray((const char*)p.buffer, CAPTURE_LENGTH), iph, tcph, udph)) {
				pcapPacketHeader.orig_len = iph.totalLength;
			} else {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Warning: could not parse the IP header for a packet";
				pcapPacketHeader.orig_len = CAPTURE_LENGTH; // not correct
			}
			pcapBytesIn.append((const char*)&pcapPacketHeader, sizeof(PcapPacketHeader));
			pcapBytesIn.append((const char*)p.buffer, CAPTURE_LENGTH);
		}

		saveFile(expPath + "/" + QString("emulator-in.pcap"), pcapBytesIn);
	}

	// Create one global pcap file with everything that exited the emulator
	{
		QByteArray pcapBytesOut;

		PcapHeader pcapHeader;
		pcapHeader.magic_number = 0xa1b2c3d4;
		pcapHeader.version_major = 2;
		pcapHeader.version_minor = 4;
		pcapHeader.thiszone = 0;
		pcapHeader.sigfigs = 0;
		pcapHeader.snaplen = CAPTURE_LENGTH;
		pcapHeader.network = LINKTYPE_RAW;
		pcapBytesOut.append((const char*)&pcapHeader, sizeof(PcapHeader));

		foreach (RecordedPacketData p, rec.recordedPacketData) {
			if (p.isNull()) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
				return false;
			}

			QList<RecordedQueuedPacketData> queueEvents = rec.queueEventsByPacketID(p.packet_id);
			if (queueEvents.isEmpty()) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Missing packet events";
				return false;
			}
			bool foundPath;
			NetGraphPath path = g.pathByNodeIndexTry(p.src_id, p.dst_id, foundPath);
			if (!foundPath || path.edgeList.isEmpty()) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No path for packet (bad route)";
				return false;
			}
			if (queueEvents.last().decision == RecordedQueuedPacketData::Queued &&
				queueEvents.last().edge_index == path.edgeList.last().index) {
				// transmitted
				PcapPacketHeader pcapPacketHeader;
				pcapPacketHeader.ts_usec = quint32((queueEvents.last().ts_exit / 1000ULL) % 1000000ULL);
				pcapPacketHeader.ts_sec = quint32((queueEvents.last().ts_exit / 1000ULL) / 1000000ULL);
				pcapPacketHeader.incl_len = CAPTURE_LENGTH;
				IPHeader iph;
				TCPHeader tcph;
				UDPHeader udph;
				if (decodePacket(QByteArray((const char*)p.buffer, CAPTURE_LENGTH), iph, tcph, udph)) {
					pcapPacketHeader.orig_len = iph.totalLength;
				} else {
					qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Warning: could not parse the IP header for a packet";
					pcapPacketHeader.orig_len = CAPTURE_LENGTH; // not correct
				}
				pcapBytesOut.append((const char*)&pcapPacketHeader, sizeof(PcapPacketHeader));
				pcapBytesOut.append((const char*)p.buffer, CAPTURE_LENGTH);
			}
		}

		saveFile(expPath + "/" + QString("emulator-out.pcap"), pcapBytesOut);
	}

	return true;
}

bool extractConversations(RecordedData rec,
						  NetGraph g,
						  QHash<NodePair, PathConversations> &allPathConversations,
						  quint64 &tsStart,
						  quint64 &tsEnd)
{
	allPathConversations.clear();

	tsStart = 0xFFffFFffFFffFFffULL;
	tsEnd = 0;
	foreach (RecordedPacketData p, rec.recordedPacketData) {
		tsStart = qMin(tsStart, p.ts_userspace_rx);
		tsEnd = qMax(tsEnd, p.ts_userspace_rx);
	}
	foreach (RecordedQueuedPacketData qe, rec.recordedQueuedPacketData) {
		if (qe.decision == RecordedQueuedPacketData::Queued) {
			tsEnd = qMax(tsEnd, qe.ts_exit);
		}
	}
	if (tsStart == 0xFFffFFffFFffFFffULL) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Possibly an empty experiment";
		tsStart = 0;
	}

	foreach (RecordedPacketData p, rec.recordedPacketData) {
		IPHeader iph;
		TCPHeader tcph;
		UDPHeader udph;

		if (!decodePacket(QByteArray((const char*)p.buffer, CAPTURE_LENGTH), iph, tcph, udph)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not decode packet";
			continue;
		}
		NodePair nodePairFwd = QPair<qint32, qint32>(p.src_id, p.dst_id);
		ProtoPortPair portPairFwd;
		portPairFwd.first = iph.protocolString;
		portPairFwd.second.first = iph.protocolString == "TCP" ? tcph.sourcePort :
																 iph.protocolString == "UDP" ? udph.sourcePort : 0;
		portPairFwd.second.second = iph.protocolString == "TCP" ? tcph.destPort :
																  iph.protocolString == "UDP" ? udph.destPort : 0;
		NodePair nodePairRet = QPair<qint32, qint32>(nodePairFwd.second, nodePairFwd.first);
		ProtoPortPair portPairRet;
		portPairRet.first = portPairFwd.first;
		portPairRet.second.first = portPairFwd.second.second;
		portPairRet.second.second = portPairFwd.second.first;

		bool found = false;
		NodePair nodePair;
		ProtoPortPair portPair;
		bool forward;
		if (iph.protocolString == "TCP") {
			if (tcph.flagSyn && !tcph.flagAck) {
				nodePair = nodePairFwd;
				portPair = portPairFwd;
				if (!allPathConversations.contains(nodePair)) {
					allPathConversations[nodePair] = PathConversations(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthFwd = 1000.0 * g.pathMaximumBandwidth(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthRet = 1000.0 * g.pathMaximumBandwidth(nodePair.second, nodePair.first);
				}
				forward = true;
				found = true;
			} else if (allPathConversations.contains(nodePairFwd)) {
				PathConversations &pathConversations = allPathConversations[nodePairFwd];
				if (pathConversations.conversations.contains(portPairFwd)) {
					// TODO check FIN otherwise we confuse flows that reuse ports
					nodePair = nodePairFwd;
					portPair = portPairFwd;
					forward = true;
					found = true;
				}
			} else if (allPathConversations.contains(nodePairRet)) {
				PathConversations &pathConversations = allPathConversations[nodePairRet];
				if (pathConversations.conversations.contains(portPairRet)) {
					// TODO check FIN otherwise we confuse flows that reuse ports
					nodePair = nodePairRet;
					portPair = portPairRet;
					forward = false;
					found = true;
				}
			}
		} else if (iph.protocolString == "UDP") {
			if (allPathConversations.contains(nodePairFwd)) {
				PathConversations &pathConversations = allPathConversations[nodePairFwd];
				if (pathConversations.conversations.contains(portPairFwd)) {
					nodePair = nodePairFwd;
					portPair = portPairFwd;
					forward = true;
					found = true;
				}
			} else if (allPathConversations.contains(nodePairRet)) {
				PathConversations &pathConversations = allPathConversations[nodePairRet];
				if (pathConversations.conversations.contains(portPairRet)) {
					nodePair = nodePairRet;
					portPair = portPairRet;
					forward = false;
					found = true;
				}
			}
			if (!found) {
				nodePair = nodePairFwd;
				portPair = portPairFwd;
				if (!allPathConversations.contains(nodePair)) {
					allPathConversations[nodePair] = PathConversations(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthFwd = 1000.0 * g.pathMaximumBandwidth(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthRet = 1000.0 * g.pathMaximumBandwidth(nodePair.second, nodePair.first);
				}
				forward = true;
				found = true;
			}
		}
		if (!found) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Incomplete stream (no SYN)";
			// TODO append anyway ? not necessary in normal cases
			continue;
		}
		PathConversations &pathConversations = allPathConversations[nodePair];
		if (!pathConversations.conversations.contains(portPair)) {
			pathConversations.conversations[portPair] << Conversation(portPair.second.first, portPair.second.second, portPair.first);
		}
		Conversation &conversation = pathConversations.conversations[portPair].last();
		Flow &flow = forward ? conversation.fwdFlow : conversation.retFlow;

		FlowPacket flowPacket;
		flowPacket.packetId = p.packet_id;
		flowPacket.tsSent = p.ts_userspace_rx;
		QList<RecordedQueuedPacketData> queueEvents = rec.queueEventsByPacketID(p.packet_id);
		if (queueEvents.isEmpty()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No queueing events for packet (bad route or capture limit reached)";
			continue;
		}
		bool foundPath;
		NetGraphPath path = g.pathByNodeIndexTry(p.src_id, p.dst_id, foundPath);
		if (!foundPath || path.edgeList.isEmpty()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No path for packet (bad route)";
			continue;
		}
		flowPacket.received = queueEvents.last().decision == RecordedQueuedPacketData::Queued &&
							  queueEvents.last().edge_index == path.edgeList.last().index;
		if (flowPacket.received) {
			flowPacket.tsReceived = queueEvents.last().ts_exit;
		} else {
			flowPacket.tsReceived = 0;
		}
		flowPacket.dropped = queueEvents.last().decision != RecordedQueuedPacketData::Queued;
		if (flowPacket.dropped) {
			flowPacket.dropEdgeId = queueEvents.last().edge_index;
			flowPacket.tsDrop = queueEvents.last().ts_enqueue;
		}
		flowPacket.ipHeader = iph;
		flowPacket.tcpHeader = tcph;
		flowPacket.udpHeader = udph;
		flow.packets << flowPacket;
	}
	return true;
}

class FlowIntDataItem {
public:
	FlowIntDataItem(QString flowName = QString(), quint64 time = 0, qint64 value = 0) :
		flowName(flowName), time(time), value(value)
	{}
	// Some human readable ID of the flow
	QString flowName;
	// time relative to the start of the experiment, in ms
	quint64 time;
	// value for that time
	qint64 value;
};

enum LossCode {
	LossFwd = 0,
	LossRet = 1
};

class FlowAnnotation {
public:
	FlowAnnotation(QString flowName = QString(),
				   quint64 time = 0,
				   QString text = QString(),
				   QString shortText = QString(),
				   qint64 valueHint = -1) :
		flowName(flowName),
		time(time),
		text(text),
		shortText(shortText),
		tickHeight(30),
		valueHint(valueHint)
	{}
	// Some human readable ID of the flow
	QString flowName;
	// time relative to the start of the experiment, in ms
	quint64 time;
	// text for that time
	QString text;
	QString shortText;
	int tickHeight;
	qint64 valueHint;
};

// Human-readable identifier of a flow
// MUST NOT contain commas
QString makeFlowName(qint32 sourceNodeId,
					 qint32 destNodeId,
					 quint16 sourcePort,
					 quint16 destPort,
					 QString protocolString)
{
	return QString("Flow-%1 %2:%3 -> %4:%5").
			arg(protocolString).
			arg(sourceNodeId + 1).
			arg(sourcePort).
			arg(destNodeId + 1).
			arg(destPort);
}

bool flowNameLessThan(const QString &s1, const QString &s2)
{
	// e.g. Flow_TCP 2:52112 -> 4:8001 - heavy - Bottleneck
	QStringList tokens1 = s1.split(" ");
	QStringList tokens2 = s2.split(" ");
	if (tokens1.isEmpty() || tokens2.isEmpty()) {
		return s1 < s2;
	}
	if (tokens1.first() < tokens2.first()) {
		return true;
	} else if (tokens1.first() > tokens2.first()) {
		return false;
	}
	if (tokens1.count() >= 4) {
		QString src1 = tokens1[1];
		QString dst1 = tokens1[3];
		QString suffix1 = QStringList(tokens1.mid(5)).join(" ");
		if (tokens2.count() >= 4) {
			QString src2 = tokens2[1];
			QString dst2 = tokens2[3];
			QString suffix2 = QStringList(tokens2.mid(5)).join(" ");
			if (src1 == dst2 && src2 == dst1) {
				if (tokens1.count() >= 6) {
					if (tokens2.count() >= 6) {
						if (tokens1[5] == "heavy" && tokens2[5] == "acker") {
							return true;
						} else if (tokens2[5] == "heavy" && tokens1[5] == "acker") {
							return false;
						}
					}
				}
				return suffix1 < suffix2;
			} else {
				return qMin(src1, dst1) < qMin(src2, dst2);
			}
		} else {
			return true;
		}
	} else {
		if (tokens2.count() > 4) {
			return false;
		}
	}
	return s1 < s2;
}

QString toCsv(QMap<quint64, QList<FlowIntDataItem> > data)
{
	QString csv;
	QList<QString> columns;
	QHash<QString, int> columnName2Index;
	{
		foreach (QList<FlowIntDataItem> itemList, data.values()) {
			foreach (FlowIntDataItem item, itemList) {
				if (!columns.contains(item.flowName)) {
					columns << item.flowName;
				}
			}
		}
		qSort(columns.begin(), columns.end(), flowNameLessThan);
		for (int i = 0; i < columns.count(); i++) {
			columnName2Index[columns[i]] = i;
		}
	}
	csv += QString("Time (ms)");
	foreach (QString column, columns) {
		csv += QString(",%1").arg(column);
	}
	csv += "\n";
	foreach (quint64 time, data.uniqueKeys()) {
		csv += QString("%1").arg(time);
		// It is OK to keep the cell empty when there is no data for a series.
		QVector<QString> colData(columns.count(), "");
		foreach (FlowIntDataItem item, data[time]) {
			QString value = QString("%1").arg(item.value);
			int colIndex = columnName2Index.value(item.flowName, -1);
			if (colIndex >= 0) {
				colData[colIndex] = value;
			}
		}
		foreach (QString column, colData) {
			csv += QString(",%1").arg(column);
		}
		csv += "\n";
	}
	return csv;
}

QString toCsvJs(QMap<quint64, QList<FlowIntDataItem> > data, QString varname)
{
	QString csv = toCsv(data);
	QString result = QString("%1 = \"\";\n").arg(varname);
	while (!csv.isEmpty()) {
		QString part = csv.mid(0, 100);
		csv = csv.mid(100);
		QString partEncoded;
		QString transparents("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890,.<>/?;:|[]{}`~!@#$%^&*()-=_+ ");
		for (const QChar *c = part.unicode(); *c != '\0'; c++) {
			if (transparents.contains(*c)) {
				partEncoded += *c;
			} else if (*c == '\n') {
				partEncoded += "\\n";
			} else if (*c == '\t') {
				partEncoded += "\\t";
			} else {
				partEncoded += QString("\\u%1").arg(c->unicode(), 4, 16, QChar('0'));
			}
		}
		result += QString("%1 += \"%2\";\n").arg(varname).arg(partEncoded);
	}
	return result;
}

typedef QList<FlowAnnotation> AnnotationList;
typedef QList<FlowIntDataItem> FlowIntDataItemList;
QString toJson(QMap<quint64, QList<FlowAnnotation> > annotations,
			   QMap<quint64, QList<FlowIntDataItem> > points)
{
	// make matrix mask for keeping track of occupied spots
	const int numHBins = 100;
	const int numVBins = 100;
	QVector<QVector<bool> > matrix(numHBins);
	for (int i = 0; i < numHBins; i++) {
		matrix[i].resize(numVBins);
	}

	// compute bbox
	quint64 xmin, xmax, ymin, ymax;
	xmin = ymin = 0xffFFffFFffFFffFFULL;
	xmax = ymax = 0;
	foreach (AnnotationList annotationList, annotations.values()) {
		foreach (FlowAnnotation annotation, annotationList) {
			xmin = qMin(xmin, annotation.time);
			xmax = qMax(xmax, annotation.time);
			if (annotation.valueHint >= 0) {
				ymin = qMin(ymin, quint64(annotation.valueHint));
				ymax = qMax(ymax, quint64(annotation.valueHint));
			}
		}
	}
	foreach (FlowIntDataItemList itemList, points.values()) {
		foreach (FlowIntDataItem item, itemList) {
			xmin = qMin(xmin, item.time);
			xmax = qMax(xmax, item.time);
			ymin = qMin(ymin, quint64(item.value));
			ymax = qMax(ymax, quint64(item.value));
		}
	}

	const int heightStep = 30;

	foreach (quint64 time, annotations.uniqueKeys()) {
		QList<FlowAnnotation> &list = annotations[time];
		int x = ((time - xmin) * numHBins) / (xmax - xmin);
		x = qMax(0, qMin(numHBins - 1, x)); // just to be sure
		for (int i = 0; i < list.count(); i++) {
			FlowAnnotation &a = list[i];
			if (a.valueHint >= 0) {
				int y = ((a.valueHint - ymin) * numVBins) / (ymax - ymin);
				y = qMax(0, qMin(numVBins - 1, y)); // just to be sure
				// find an empty box
				bool positioned = false;
				for (int numtries = 0; numtries < 2; numtries++) {
					for (unsigned absdelta = 1; absdelta < ymax - ymin && !positioned; absdelta++) {
						for (int dir = 0; dir <= 1 && !positioned; dir++) {
							int delta = dir ? -absdelta : absdelta;
							int ynew = y + delta;
							if (ynew <= 3 || ynew >= numVBins - 3)
								continue;
							ynew = qMax(0, qMin(numVBins - 1, ynew));
							if (!matrix[x][ynew]) {
								matrix[x][ynew] = true;
								a.tickHeight = delta * heightStep;
								positioned = true;
							}
						}
					}
					if (!positioned) {
						// clear column and try one more time
						matrix[x].fill(false, numVBins);
					}
				}
			}
		}
	}

	QString json = "[\n";
	bool first = true;
	foreach (AnnotationList annotationList, annotations.values()) {
		foreach (FlowAnnotation annotation, annotationList) {
			if (first) {
				first = false;
			} else {
				json += ",\n";
			}
			json += "  {\n";
			json += QString("    \"series\" : \"%1\",\n").arg(annotation.flowName);
			json += QString("    \"x\" : %1,\n").arg(annotation.time);
			json += QString("    \"shortText\" : \"%1\",\n").arg(annotation.shortText);
			json += QString("    \"text\" : \"%1, before t = %2 ms\",\n").arg(annotation.text).arg(annotation.time);
			json += QString("    \"tickHeight\" : %1,\n").arg(annotation.tickHeight);
			json += QString("    \"attachAtBottom\" : false,\n");
			json += QString("    \"valueHint\" : %1\n").arg(annotation.valueHint);
			json += "  }";
		}
	}
	json += "\n";
	json += "]\n";
	return json;
}

QString toJsonJs(QMap<quint64, QList<FlowAnnotation> > annotations,
				 QMap<quint64, QList<FlowIntDataItem> > points,
				 QString varname)
{
	QString json = toJson(annotations, points);
	QString result = QString("%1 = %2;\n").arg(varname).arg(json);
	return result;
}

// Generates an annotation if necessary, using the previous timestamp,
// if there are any losses in [prevTime, currentTime)
// Use prevTime == currentTime if there was no previous timestamp
void generateLossAnnotation(QList<FlowIntDataItem> &flowLosses,
							QList<FlowAnnotation> &annotations,
							quint64 currentTime,
							quint64 prevTime,
							QString flowName,
							qint64 valueHint = -1)
{
	int numPacketsLostFwd = 0;
	int numPacketsLostRet = 0;
	while (!flowLosses.isEmpty()) {
		if (flowLosses.first().time < currentTime) {
			FlowIntDataItem lossItem = flowLosses.takeFirst();
			if (lossItem.value == LossFwd) {
				numPacketsLostFwd++;
			} else if (lossItem.value == LossRet) {
				numPacketsLostRet++;
			} else {
				Q_ASSERT_FORCE(false);
			}
		} else {
			break;
		}
	}
	if (numPacketsLostFwd + numPacketsLostRet > 0) {
		QString text;
		if (numPacketsLostFwd > 0 && numPacketsLostRet > 0) {
			text = QString("Losses on forward path: %1; "
						   "Losses on return path: %2").
				   arg(numPacketsLostFwd).
				   arg(numPacketsLostRet);
		} else if (numPacketsLostFwd > 0) {
			text = QString("Losses on forward path: %1").
				   arg(numPacketsLostFwd);
		} else if (numPacketsLostRet > 0) {
			text = QString("Losses on return path: %1").
				   arg(numPacketsLostRet);
		}
		FlowAnnotation annotation(flowName,
								  prevTime,
								  text,
								  (numPacketsLostFwd > 0 && numPacketsLostRet > 0) ? QString("%1F %2R").arg(numPacketsLostFwd).arg(numPacketsLostRet)
																				   : (numPacketsLostFwd > 0) ? QString("%1F").arg(numPacketsLostFwd)
																											 : QString("%1R").arg(numPacketsLostRet),
								  valueHint);
		annotations << annotation;
	}
}

// Data amount in bits
QList<FlowIntDataItem> computeReceivedRawForFlow(Flow flow,
												 QString flowName,
												 quint64 tsStart,
												 QList<FlowIntDataItem> flowLosses,
												 QList<FlowAnnotation> &annotations)
{
	annotations.clear();
	QList<FlowIntDataItem> result;

	// time -> size of packet received
	QMap<quint64, QList<FlowIntDataItem> > receivedRaw;
	foreach (FlowPacket p, flow.packets) {
		if (p.received) {
			FlowIntDataItem item = FlowIntDataItem(flowName, ns2ms(p.tsReceived - tsStart), p.ipHeader.totalLength * 8);
			receivedRaw[item.time] << item;
		}
	}
	// compute cumulative sum
	// note that the map automatically sorts the items by time
	quint64 totalReceivedRaw = 0;
	foreach (quint64 time, receivedRaw.uniqueKeys()) {
		for (int i = 0; i < receivedRaw[time].count(); i++) {
			totalReceivedRaw += receivedRaw[time][i].value;
			receivedRaw[time][i].value = totalReceivedRaw;
		}
	}
	// save items
	foreach (QList<FlowIntDataItem> itemList, receivedRaw.values()) {
		foreach (FlowIntDataItem item, itemList) {
			// annotate losses if necessary
			generateLossAnnotation(flowLosses,
								   annotations,
								   item.time,
								   result.isEmpty() ? item.time : result.last().time,
								   flowName,
								   result.isEmpty() ? item.value : result.last().value);
			result << item;
		}
	}
	return result;
}

// Throughput in bytes/second
QList<FlowIntDataItem> computeReceivedRawThroughputForFlow(Flow flow,
														   QString flowName,
														   quint64 tsStart,
														   QList<FlowIntDataItem> flowLosses,
														   QList<FlowAnnotation> &annotations,
														   qreal window = 0)
{
	QList<FlowIntDataItem> receivedRaw = computeReceivedRawForFlow(flow,
																   flowName,
																   tsStart,
																   QList<FlowIntDataItem>(),
																   annotations);
	annotations.clear();

	QList<FlowIntDataItem> result;
	if (receivedRaw.count() < 2)
		return result;
	if (window == 0) {
		QVector<quint64> packetSpacings(receivedRaw.count() - 1);
		for (int i = 1; i < receivedRaw.count(); i++) {
			packetSpacings[i-1] = receivedRaw[i].time - receivedRaw[i-1].time;
		}
		qSort(packetSpacings);
		quint64 medianPacketSpacing = packetSpacings[packetSpacings.count() / 2];
		if (medianPacketSpacing == 0) {
			medianPacketSpacing = 100; // 100 ms
		}

		const qreal windowSize = 3.0;
		window = windowSize * medianPacketSpacing;
	}

	for (int i = 0; i < receivedRaw.count(); i++) {
		qreal received = receivedRaw[i].value - (i > 0 ? receivedRaw[i-1].value : 0);
        qreal duration = window;
		for (int j = i - 1; j >= 0; j--) {
			if (receivedRaw[j].time < receivedRaw[i].time - window) {
				break;
			}
			received = receivedRaw[i].value - receivedRaw[j].value;
			duration = receivedRaw[i].time - receivedRaw[j].time;
		}
		qreal throughput = duration > 0 ? received / duration : 0; // b/ms
		FlowIntDataItem item = receivedRaw[i];
		item.value = throughput * 1000; // b/ms to b/s
		if (result.isEmpty() || (item.time - result.last().time > window)) {
			// annotate losses if necessary
			generateLossAnnotation(flowLosses,
								   annotations,
								   item.time,
								   result.isEmpty() ? item.time : result.last().time,
								   flowName,
								   result.isEmpty() ? item.value : result.last().value);
			result << item;
		}
	}
	return result;
}

// Receive window in bits
QList<FlowIntDataItem> computeRwinForFlow(Flow flow,
										  QString flowName,
										  quint64 tsStart,
										  QList<FlowIntDataItem> flowLosses,
										  QList<FlowAnnotation> &annotations)
{
	annotations.clear();
	QList<FlowIntDataItem> result;

	if (flow.protocolString != "TCP")
		return result;

	// time -> size of receive window
	QMap<quint64, QList<FlowIntDataItem> > rwin;
	qint64 windowScaleFactor = 1;
	foreach (FlowPacket p, flow.packets) {
		if (p.tcpHeader.windowScaleFactor > 0) {
			windowScaleFactor = p.tcpHeader.windowScaleFactor;
		}
		// The Window field in a SYN (a <SYN> or <SYN,ACK>) segment is never scaled
		FlowIntDataItem item = FlowIntDataItem(flowName, ns2ms(p.tsSent - tsStart),
											   p.tcpHeader.flagSyn ? p.tcpHeader.windowRaw * 8 :
																	 p.tcpHeader.windowRaw * 8 * windowScaleFactor);
		rwin[item.time] << item;
	}
	// save items
	foreach (QList<FlowIntDataItem> itemList, rwin.values()) {
		foreach (FlowIntDataItem item, itemList) {
			// annotate losses if necessary
			generateLossAnnotation(flowLosses,
								   annotations,
								   item.time,
								   result.isEmpty() ? item.time : result.last().time,
								   flowName,
								   result.isEmpty() ? item.value : result.last().value);
			result << item;
		}
	}
	return result;
}

// RTT in miliseconds
QList<FlowIntDataItem> computeRttForFlow(Flow fwdFlow,
										 Flow retFlow,
										 QString flowName,
										 quint64 tsStart,
										 QList<FlowIntDataItem> flowLosses,
										 QList<FlowAnnotation> &annotations)
{
	annotations.clear();
	QList<FlowIntDataItem> result;

	// time -> (fwd-delay, ret-delay) in ms
	// only one of the two might be set, zero delay means undefined
	QMap<quint64, QPair<quint64, quint64> > oneWayDelays;
	foreach (FlowPacket p, fwdFlow.packets) {
		if (p.received) {
			oneWayDelays[ns2ms(p.tsReceived - tsStart)].first = ns2ms(p.tsReceived - p.tsSent);
		}
	}
	foreach (FlowPacket p, retFlow.packets) {
		if (p.received) {
			oneWayDelays[ns2ms(p.tsSent - tsStart)].second = ns2ms(p.tsReceived - p.tsSent);
		}
	}

	// compute RTT as the sum of fwd & ret one way delays
	// pick the closest known values
	qint64 lastFwdDelay = 0;
	qint64 lastRetDelay = 0;
	foreach (quint64 time, oneWayDelays.uniqueKeys()) {
		QPair<quint64, quint64> delayPair = oneWayDelays[time];
		if (delayPair.first > 0) {
			lastFwdDelay = delayPair.first;
		}
		if (delayPair.second > 0) {
			lastRetDelay = delayPair.second;
		}
		if (lastFwdDelay > 0 && lastRetDelay > 0) {
			qint64 rtt = lastFwdDelay + lastRetDelay; // ns to ms
			// annotate losses if necessary
			generateLossAnnotation(flowLosses,
								   annotations,
								   time,
								   result.isEmpty() ? time : result.last().time,
								   flowName,
								   result.isEmpty() ? rtt : result.last().value);
			result << FlowIntDataItem(flowName, time, rtt);
		}
	}
	return result;
}

// Number of packets or bits in flight
QList<FlowIntDataItem> computePacketsInFlightForFlow(Flow flow,
													 QString flowName,
													 quint64 tsStart,
													 QList<FlowIntDataItem> flowLosses,
													 QList<FlowAnnotation> &annotations,
													 bool bytes = false)
{
	annotations.clear();
	QList<FlowIntDataItem> result;

	// time -> size of receive window
	QMap<quint64, qint64> inflightDeltaNs;
	foreach (FlowPacket p, flow.packets) {
		inflightDeltaNs[p.tsSent - tsStart] += bytes ? p.ipHeader.totalLength * 8 : 1;
		if (p.dropped) {
			inflightDeltaNs[p.tsDrop - tsStart] -= bytes ? p.ipHeader.totalLength * 8 : 1;
		}
		if (p.received) {
			inflightDeltaNs[p.tsReceived - tsStart] -= bytes ? p.ipHeader.totalLength * 8 : 1;
		}
	}
	// cumulative sum
	QMap<quint64, qint64> inflightNs;
	qint64 lastValue = 0;
	foreach (quint64 time, inflightDeltaNs.uniqueKeys()) {
		lastValue += inflightDeltaNs[time];
		if (lastValue < 0) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Negative value";
		}
		qint64 value = qMax(0LL, lastValue);
		inflightNs[time] = value;
	}
	// change resolution
	QMap<quint64, qint64> inflightMs;
	foreach (quint64 time, inflightNs.uniqueKeys()) {
		quint64 timeMs = ns2ms(time);
		inflightMs[timeMs] = qMax(inflightMs[timeMs], inflightNs[time]);
	}
	// transform into events
	foreach (quint64 time, inflightMs.uniqueKeys()) {
		FlowIntDataItem item = FlowIntDataItem(flowName,
											   time,
											   inflightMs[time]);
		// annotate losses if necessary
		generateLossAnnotation(flowLosses,
							   annotations,
							   item.time,
							   result.isEmpty() ? item.time : result.last().time,
							   flowName,
							   result.isEmpty() ? item.value : result.last().value);
		result << item;
	}
	return result;
}

// Maximum throughput (i.e. RWIN/RTT) in b/s
QList<FlowIntDataItem> computeMptrForFlow(QList<FlowIntDataItem> rwin,
										  QList<FlowIntDataItem> rtt,
										  QString flowName,
										  QList<FlowIntDataItem> flowLosses,
										  QList<FlowAnnotation> &annotations,
										  qreal window = 1000)
{
	annotations.clear();
	QList<FlowIntDataItem> result;
	flowName += " - Rwin/RTT";

	qreal lastRwin = -1;
	qreal lastRtt = -1;
	while (!rwin.isEmpty() && !rtt.isEmpty()) {
		FlowIntDataItem rwinItem = rwin.first();
		FlowIntDataItem rttItem = rtt.first();
		quint64 time = qMin(rwinItem.time, rttItem.time);
		if (rwinItem.time == time) {
			rwin.takeFirst();
			lastRwin = rwinItem.value;
		}
		if (rttItem.time == time) {
			rtt.takeFirst();
			lastRtt = rttItem.value;
		}
		if (lastRwin >= 0 && lastRtt >= 0) {
			// generate point
			qreal mptr = (lastRtt > 0 ? lastRwin / lastRtt : 0) * 1.0e3; // b/ms to b/s
			if (result.isEmpty() || (time - result.last().time > window)) {
				// annotate losses if necessary
				generateLossAnnotation(flowLosses,
									   annotations,
									   time,
									   result.isEmpty() ? time : result.last().time,
									   flowName,
									   result.isEmpty() ? mptr : result.last().value);
				result << FlowIntDataItem(flowName, time, qint64(ceil(mptr)));
			}
		}
	}
	return result;
}

// LossFwd means loss on fwd path; LossRet means loss on return path
QList<FlowIntDataItem> computeLossesForFlow(Flow fwdFlow, Flow retFlow, QString flowName, quint64 tsStart)
{
	QList<FlowIntDataItem> result;

	// time -> code (0/1 as specified above)
	QMap<quint64, QList<qint64> > losses;
	foreach (FlowPacket p, fwdFlow.packets) {
		if (p.dropped) {
			losses[ns2ms(p.tsDrop - tsStart)] << LossFwd;
		}
	}
	foreach (FlowPacket p, retFlow.packets) {
		if (p.dropped) {
			losses[ns2ms(p.tsDrop - tsStart)] << LossRet;
		}
	}

	// merge
	foreach (quint64 time, losses.uniqueKeys()) {
		foreach (qint64 code, losses[time]) {
			result << FlowIntDataItem(flowName, time, code);
		}
	}
	return result;
}

typedef QPair<QString, Flow> flowNameAndFlow;

void doSeqAckAnalysis(QString rootPath,
					  QString expPath,
					  QHash<NodePair, PathConversations> allPathConversations,
					  quint64 tsStart,
					  quint64 tsEnd)
{
	// time(ms) -> list of items for that time
	QMap<quint64, QList<FlowIntDataItem> > flowReceivedRawSorted;
	QMap<quint64, QList<FlowAnnotation> > flowReceivedRawAnnotationSorted;
	QMap<quint64, QList<FlowIntDataItem> > flowReceivedThroughputRaw100Sorted;
	QMap<quint64, QList<FlowAnnotation> > flowReceivedThroughputAnnotation100Sorted;
	QMap<quint64, QList<FlowIntDataItem> > flowReceivedThroughputRaw500Sorted;
	QMap<quint64, QList<FlowAnnotation> > flowReceivedThroughputAnnotation500Sorted;
	QMap<quint64, QList<FlowIntDataItem> > flowReceivedThroughputRaw1000Sorted;
	QMap<quint64, QList<FlowAnnotation> > flowReceivedThroughputAnnotation1000Sorted;
	QMap<quint64, QList<FlowIntDataItem> > flowRTTSorted;
	QMap<quint64, QList<FlowAnnotation> > flowRTTAnnotationsSorted;
	QMap<quint64, QList<FlowIntDataItem> > flowRwinSorted;
	QMap<quint64, QList<FlowAnnotation> > flowRwinAnnotationsSorted;
	QMap<quint64, QList<FlowIntDataItem> > flowMptrSorted;
	QMap<quint64, QList<FlowAnnotation> > flowMptrAnnotationsSorted;
	QMap<quint64, QList<FlowIntDataItem> > flowBottleneckSorted;
	QMap<quint64, QList<FlowIntDataItem> > flowPacketsInFlightSorted;
	QMap<quint64, QList<FlowAnnotation> > flowPacketsInFlightAnnotationsSorted;
	QMap<quint64, QList<FlowIntDataItem> > flowBytesInFlightSorted;
	QMap<quint64, QList<FlowAnnotation> > flowBytesInFlightAnnotationsSorted;
	foreach (NodePair nodePair, allPathConversations.uniqueKeys()) {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "PathConversations for nodes" << nodePair;
		PathConversations &pathConversations = allPathConversations[nodePair];
		foreach (ProtoPortPair portPair, pathConversations.conversations.uniqueKeys()) {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "  " << "Conversations for ports" << portPair;
			foreach (Conversation conversation, pathConversations.conversations[portPair]) {
				QList<QPair<QString, Flow> > flows;
				QString fwdFlowName = makeFlowName(pathConversations.sourceNodeId, pathConversations.destNodeId,
												   conversation.fwdFlow.sourcePort, conversation.fwdFlow.destPort,
												   conversation.protocolString) + " - heavy";
				QString retFlowName = makeFlowName(pathConversations.destNodeId, pathConversations.sourceNodeId,
												   conversation.retFlow.sourcePort, conversation.retFlow.destPort,
												   conversation.protocolString) + " - acker";
				flows << QPair<QString, Flow>(fwdFlowName,
											  conversation.fwdFlow);
				flows << QPair<QString, Flow>(retFlowName,
											  conversation.retFlow);
				// compute losses
				QList<FlowIntDataItem> flowLossesBoth = computeLossesForFlow(conversation.fwdFlow,
																			 conversation.retFlow,
																			 fwdFlowName,
																			 tsStart);
				// compute RTT(t)
				// Note: there is only one RTT for fwd/ret path since RTT is symmetrical
				QList<FlowAnnotation> flowRTTLossAnnotations;
				QList<FlowIntDataItem> flowRTT = computeRttForFlow(conversation.fwdFlow,
																   conversation.retFlow,
																   fwdFlowName,
																   tsStart,
																   flowLossesBoth,
																   flowRTTLossAnnotations);
				foreach (FlowIntDataItem item, flowRTT) {
					flowRTTSorted[item.time] << item;
				}
				foreach (FlowAnnotation annotation, flowRTTLossAnnotations) {
					flowRTTAnnotationsSorted[annotation.time] << annotation;
				}
				foreach (flowNameAndFlow namedFlow, flows) {
					// compute losses
					QList<FlowIntDataItem> flowLosses = computeLossesForFlow(namedFlow.first == fwdFlowName ? conversation.fwdFlow : conversation.retFlow,
																			 namedFlow.first == fwdFlowName ? conversation.retFlow : conversation.fwdFlow,
																			 namedFlow.first,
																			 tsStart);
					// compute raw received data(t)
					QList<FlowAnnotation> flowReceivedRawLossAnnotations;
					QList<FlowIntDataItem> flowReceivedRaw = computeReceivedRawForFlow(namedFlow.second,
																					   namedFlow.first,
																					   tsStart,
																					   flowLosses,
																					   flowReceivedRawLossAnnotations);
					foreach (FlowIntDataItem item, flowReceivedRaw) {
						flowReceivedRawSorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowReceivedRawLossAnnotations) {
						flowReceivedRawAnnotationSorted[annotation.time] << annotation;
					}
					// compute raw throughput(t) with window=100ms
					QList<FlowAnnotation> flowReceivedThroughputRawLossAnnotations;
					QList<FlowIntDataItem> flowReceivedThroughputRaw = computeReceivedRawThroughputForFlow(namedFlow.second,
																										   namedFlow.first,
																										   tsStart,
																										   flowLosses,
																										   flowReceivedThroughputRawLossAnnotations,
																										   100);
					foreach (FlowIntDataItem item, flowReceivedThroughputRaw) {
						flowReceivedThroughputRaw100Sorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowReceivedThroughputRawLossAnnotations) {
						flowReceivedThroughputAnnotation100Sorted[annotation.time] << annotation;
					}
					// compute raw throughput(t) with window=500ms
					flowReceivedThroughputRaw = computeReceivedRawThroughputForFlow(namedFlow.second,
																					namedFlow.first,
																					tsStart,
																					flowLosses,
																					flowReceivedThroughputRawLossAnnotations,
																					500);
					foreach (FlowIntDataItem item, flowReceivedThroughputRaw) {
						flowReceivedThroughputRaw500Sorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowReceivedThroughputRawLossAnnotations) {
						flowReceivedThroughputAnnotation500Sorted[annotation.time] << annotation;
					}
					// compute raw throughput(t) with window=1000ms
					flowReceivedThroughputRaw = computeReceivedRawThroughputForFlow(namedFlow.second,
																					namedFlow.first,
																					tsStart,
																					flowLosses,
																					flowReceivedThroughputRawLossAnnotations,
																					1000);
					foreach (FlowIntDataItem item, flowReceivedThroughputRaw) {
						flowReceivedThroughputRaw1000Sorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowReceivedThroughputRawLossAnnotations) {
						flowReceivedThroughputAnnotation1000Sorted[annotation.time] << annotation;
					}
					// compute receive window(t)
					QList<FlowAnnotation> flowRwinLossAnnotations;
					QList<FlowIntDataItem> flowRwin = computeRwinForFlow(namedFlow.first == fwdFlowName ? conversation.retFlow : conversation.fwdFlow,
																		 namedFlow.first,
																		 tsStart,
																		 flowLosses,
																		 flowRwinLossAnnotations);
					foreach (FlowIntDataItem item, flowRwin) {
						flowRwinSorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowRwinLossAnnotations) {
						flowRwinAnnotationsSorted[annotation.time] << annotation;
					}
					// MPTR(t)
					QList<FlowAnnotation> flowMptrLossAnnotations;
					QList<FlowIntDataItem> flowMptr = computeMptrForFlow(flowRwin,
																		 flowRTT,
																		 namedFlow.first,
																		 flowLosses,
																		 flowMptrLossAnnotations,
																		 1000);
					foreach (FlowIntDataItem item, flowMptr) {
						flowMptrSorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowMptrLossAnnotations) {
						flowMptrAnnotationsSorted[annotation.time] << annotation;
					}
					// Merge MPTR(t) into throughput(t)
					// Note: we don't merge the annotations because it's too much info
					foreach (FlowIntDataItem item, flowMptr) {
						flowReceivedThroughputRaw100Sorted[item.time] << item;
						flowReceivedThroughputRaw500Sorted[item.time] << item;
						flowReceivedThroughputRaw1000Sorted[item.time] << item;
					}
					// Compute the path bottleneck
					QList<FlowIntDataItem> flowBottleneck;
					flowBottleneck << FlowIntDataItem(namedFlow.first + " - Bottleneck",
													  0,
													  namedFlow.first == fwdFlowName ? pathConversations.maxPossibleBandwidthFwd * 8 :
																					   pathConversations.maxPossibleBandwidthRet * 8);
					flowBottleneck << FlowIntDataItem(namedFlow.first + " - Bottleneck",
													  ns2ms(tsEnd - tsStart),
													  namedFlow.first == fwdFlowName ? pathConversations.maxPossibleBandwidthFwd * 8 :
																					   pathConversations.maxPossibleBandwidthRet * 8);
					foreach (FlowIntDataItem item, flowBottleneck) {
						flowBottleneckSorted[item.time] << item;
					}
					// Merge flowBottleneck(t) into throughput(t)
					foreach (FlowIntDataItem item, flowBottleneck) {
						flowReceivedThroughputRaw100Sorted[item.time] << item;
						flowReceivedThroughputRaw500Sorted[item.time] << item;
						flowReceivedThroughputRaw1000Sorted[item.time] << item;
					}
					// compute #packets in flight(t)
					QList<FlowAnnotation> flowPacketsInFlightLossAnnotations;
					QList<FlowIntDataItem> flowPacketsInFlight = computePacketsInFlightForFlow(namedFlow.second,
																							   namedFlow.first,
																							   tsStart,
																							   flowLosses,
																							   flowPacketsInFlightLossAnnotations,
																							   false);
					foreach (FlowIntDataItem item, flowPacketsInFlight) {
						flowPacketsInFlightSorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowPacketsInFlightLossAnnotations) {
						flowPacketsInFlightAnnotationsSorted[annotation.time] << annotation;
					}
					// compute #bytes in flight(t)
					QList<FlowAnnotation> flowBytesInFlightLossAnnotations;
					QList<FlowIntDataItem> flowBytesInFlight = computePacketsInFlightForFlow(namedFlow.second,
																							 namedFlow.first,
																							 tsStart,
																							 flowLosses,
																							 flowBytesInFlightLossAnnotations,
																							 true);
					foreach (FlowIntDataItem item, flowBytesInFlight) {
						flowBytesInFlightSorted[item.time] << item;
					}
					foreach (FlowAnnotation annotation, flowBytesInFlightLossAnnotations) {
						flowBytesInFlightAnnotationsSorted[annotation.time] << annotation;
					}
				}
			}
		}
	}

	QProcess::execute("bash",
					  QStringList() <<
					  "-c" <<
					  QString("cp --no-dereference --recursive --remove-destination %1/www/* '%2'")
					  .arg(rootPath + "/")
					  .arg(expPath + "/"));

	saveFile(expPath + "/" + QString("flow-received-raw.csv"),
			 toCsv(flowReceivedRawSorted));
	saveFile(expPath + "/" + QString("flow-received-raw.js"),
			 toCsvJs(flowReceivedRawSorted,
					 "flowReceivedRawCsv"));
	saveFile(expPath + "/" + QString("flow-received-raw-annotations.json"),
			 toJson(flowReceivedRawAnnotationSorted,
					flowReceivedRawSorted));
	saveFile(expPath + "/" + QString("flow-received-raw-annotations.js"),
			 toJsonJs(flowReceivedRawAnnotationSorted,
					  flowReceivedRawSorted,
					  "flowReceivedRawAnnotations"));

	saveFile(expPath + "/" + QString("flow-received-throughput-raw-100.csv"),
			 toCsv(flowReceivedThroughputRaw100Sorted));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-100.js"),
			 toCsvJs(flowReceivedThroughputRaw100Sorted,
					 "flowReceivedThroughputRaw100Csv"));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-annotations-100.json"),
			 toJson(flowReceivedThroughputAnnotation100Sorted,
					flowReceivedThroughputRaw100Sorted));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-annotations-100.js"),
			 toJsonJs(flowReceivedThroughputAnnotation100Sorted,
					  flowReceivedThroughputRaw100Sorted,
					  "flowReceivedThroughputAnnotations100"));

	saveFile(expPath + "/" + QString("flow-received-throughput-raw-500.csv"),
			 toCsv(flowReceivedThroughputRaw500Sorted));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-500.js"),
			 toCsvJs(flowReceivedThroughputRaw500Sorted,
					 "flowReceivedThroughputRaw500Csv"));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-annotations-500.json"),
			 toJson(flowReceivedThroughputAnnotation500Sorted,
					flowReceivedThroughputRaw500Sorted));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-annotations-500.js"),
			 toJsonJs(flowReceivedThroughputAnnotation500Sorted,
					  flowReceivedThroughputRaw500Sorted,
					  "flowReceivedThroughputAnnotations500"));

	saveFile(expPath + "/" + QString("flow-received-throughput-raw-1000.csv"),
			 toCsv(flowReceivedThroughputRaw1000Sorted));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-1000.js"),
			 toCsvJs(flowReceivedThroughputRaw1000Sorted,
					 "flowReceivedThroughputRaw1000Csv"));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-annotations-1000.json"),
			 toJson(flowReceivedThroughputAnnotation1000Sorted,
					flowReceivedThroughputRaw1000Sorted));
	saveFile(expPath + "/" + QString("flow-received-throughput-raw-annotations-1000.js"),
			 toJsonJs(flowReceivedThroughputAnnotation1000Sorted,
					  flowReceivedThroughputRaw1000Sorted,
					  "flowReceivedThroughputAnnotations1000"));

	saveFile(expPath + "/" + QString("flow-rtt.csv"),
			 toCsv(flowRTTSorted));
	saveFile(expPath + "/" + QString("flow-rtt.js"),
			 toCsvJs(flowRTTSorted,
					 "flowRttCsv"));
	saveFile(expPath + "/" + QString("flow-rtt-annotations.json"),
			 toJson(flowRTTAnnotationsSorted,
					flowRTTSorted));
	saveFile(expPath + "/" + QString("flow-rtt-annotations.js"),
			 toJsonJs(flowRTTAnnotationsSorted,
					  flowRTTSorted,
					  "flowRttAnnotations"));

	saveFile(expPath + "/" + QString("flow-rwin.csv"),
			 toCsv(flowRwinSorted));
	saveFile(expPath + "/" + QString("flow-rwin.js"),
			 toCsvJs(flowRwinSorted,
					 "flowRwinCsv"));
	saveFile(expPath + "/" + QString("flow-rwin-annotations.json"),
			 toJson(flowRwinAnnotationsSorted,
					flowRwinSorted));
	saveFile(expPath + "/" + QString("flow-rwin-annotations.js"),
			 toJsonJs(flowRwinAnnotationsSorted,
					  flowRwinSorted,
					  "flowRwinAnnotations"));

	saveFile(expPath + "/" + QString("flow-packets-in-flight.csv"),
			 toCsv(flowPacketsInFlightSorted));
	saveFile(expPath + "/" + QString("flow-packets-in-flight.js"),
			 toCsvJs(flowPacketsInFlightSorted,
					 "flowPacketsInFlightCsv"));
	saveFile(expPath + "/" + QString("flow-packets-in-flight-annotations.json"),
			 toJson(flowPacketsInFlightAnnotationsSorted,
					flowPacketsInFlightSorted));
	saveFile(expPath + "/" + QString("flow-packets-in-flight-annotations.js"),
			 toJsonJs(flowPacketsInFlightAnnotationsSorted,
					  flowPacketsInFlightSorted,
					  "flowPacketsInFlightAnnotations"));

	saveFile(expPath + "/" + QString("flow-bytes-in-flight.csv"),
			 toCsv(flowBytesInFlightSorted));
	saveFile(expPath + "/" + QString("flow-bytes-in-flight.js"),
			 toCsvJs(flowBytesInFlightSorted,
					 "flowBytesInFlightCsv"));
	saveFile(expPath + "/" + QString("flow-bytes-in-flight-annotations.json"),
			 toJson(flowBytesInFlightAnnotationsSorted,
					flowBytesInFlightSorted));
	saveFile(expPath + "/" + QString("flow-bytes-in-flight-annotations.js"),
			 toJsonJs(flowBytesInFlightAnnotationsSorted,
					  flowBytesInFlightSorted,
					  "flowBytesInFlightAnnotations"));
}

bool processLineRecord(QString rootPath, QString expPath, int &packetCount, int &queueEventCount)
{
	RecordedData rec;
	if (!rec.load(expPath + "/" + "recorded.line-rec")) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not open/read file";
		return false;
	}

	packetCount = rec.recordedPacketData.count();
	queueEventCount = rec.recordedQueuedPacketData.count();
	qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Packet count:" << rec.recordedPacketData.count();
	qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Queue events:" << rec.recordedQueuedPacketData.count();

	NetGraph g;
	if (!loadGraph(expPath, g)) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Aborting";
		return false;
	}

	if (!createPcapFiles(expPath, rec, g)) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Aborting";
		return false;
	}

	QHash<NodePair, PathConversations> allPathConversations;
	quint64 tsStart, tsEnd;
	extractConversations(rec, g, allPathConversations, tsStart, tsEnd);

	foreach (NodePair nodePair, allPathConversations.uniqueKeys()) {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "PathConversations for nodes" << nodePair;
		PathConversations &pathConversations = allPathConversations[nodePair];
		foreach (ProtoPortPair portPair, pathConversations.conversations.uniqueKeys()) {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "  " << "Conversations for ports" << portPair;
			foreach (Conversation conversation, pathConversations.conversations[portPair]) {
				qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "  " << "  " << "FwdFlow" <<
							"#packets:" << conversation.fwdFlow.packets.count();
				qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "  " << "  " << "RetFlow" <<
							"#packets:" << conversation.retFlow.packets.count();
			}
		}
	}

	// SEQ ACK analysis
	doSeqAckAnalysis(rootPath, expPath, allPathConversations, tsStart, tsEnd);

	return true;
}

PathConversations::PathConversations(qint32 sourceNodeId, qint32 destNodeId)
	: sourceNodeId(sourceNodeId),
	  destNodeId(destNodeId),
	  maxPossibleBandwidthFwd(0),
	  maxPossibleBandwidthRet(0)
{
}

Conversation::Conversation(quint16 sourcePort, quint16 destPort, QString protocolString)
	: fwdFlow(sourcePort, destPort, protocolString),
	  retFlow(destPort, sourcePort, protocolString),
	  sourcePort(sourcePort),
	  destPort(destPort),
	  protocolString(protocolString)
{
}

Flow::Flow(quint16 sourcePort, quint16 destPort, QString protocolString)
	: sourcePort(sourcePort),
	  destPort(destPort),
	  protocolString(protocolString)
{
}
