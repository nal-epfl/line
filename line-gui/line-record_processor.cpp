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
#include "../util/bitarray.h"
#include "intervalmeasurements.h"
#include "result_processing.h"
#include "../util/json.h"

#define WRITE_PCAP 0

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

int getOriginalPacketLength(QByteArray buffer)
{
	IPHeader iph;
	TCPHeader tcph;
	UDPHeader udph;
	ICMPHeader icmph;
	if (decodePacket(buffer, iph, tcph, udph, icmph)) {
		return iph.totalLength;
	} else {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Warning: could not parse the IP header for a packet";
		return CAPTURE_LENGTH; // not correct
	}
}

// Returns a flow identifier for the packet. The identifier is guaranteed to be different only for concurrent flows
// belonging to the same path (flows on different paths can have the same identifier; also the indentifier might
// repeat if port numbers are reused in sequential flows).
// The value is not zero for TCP and UDP flows (except in case of error).
quint32 getPacketFlow(QByteArray buffer, bool &freshFlow)
{
	freshFlow = false;
	IPHeader iph;
	TCPHeader tcph;
	UDPHeader udph;
	ICMPHeader icmph;
	if (decodePacket(buffer, iph, tcph, udph, icmph)) {
		if (iph.protocol == IPPROTO_TCP) {
			quint32 flow = tcph.sourcePort;
			flow = (flow << 16) | tcph.destPort;
			freshFlow = tcph.flagSyn;
			return flow;
		} else if (iph.protocol == IPPROTO_UDP) {
			quint32 flow = udph.sourcePort;
			flow = (flow << 16) | udph.destPort;
			return flow;
		} else if (iph.protocol == IPPROTO_ICMP &&
				   (icmph.echoRequest || icmph.echoReply)) {
			quint32 flow = icmph.icmpId;
			flow = (flow << 16) | icmph.icmpSeqNo;
			return flow;
		} else {
			return 0;
		}
	} else {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Warning: could not parse the IP header for a packet";
		return 0; // not correct
	}
}

qint64 getSeqNo(QByteArray buffer, qint64 lastKnownSeqNo)
{
	IPHeader iph;
	TCPHeader tcph;
	UDPHeader udph;
	ICMPHeader icmph;
	if (decodePacket(buffer, iph, tcph, udph, icmph)) {
		if (iph.protocol == IPPROTO_TCP) {
			qint64 result = tcph.seq;
			// Wrap around protection
			if (lastKnownSeqNo >= 0) {
				while (result < lastKnownSeqNo && lastKnownSeqNo - result > (1LL << 31)) {
					result += 1LL << 32;
				}
			}
			return result;
		} else if (iph.protocol == IPPROTO_UDP) {
			return 0;
		} else if (iph.protocol == IPPROTO_ICMP) {
			return 0;
		} else {
			return 0;
		}
	} else {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Warning: could not parse the IP header for a packet";
		return 0; // not correct
	}
}

void generatePacketInfo(RecordedData &rec,
						OVector<quint64, qint64> &packetExitTimestamp,
						OVector<qint32, qint64> &packetExitLink,
						OVector<bool, qint64> &packetDropped)
{
	qDebug() << "generatePacketInfo() ...";
	packetExitTimestamp = OVector<quint64, qint64>(rec.recordedPacketData.count(), 0);
	packetExitLink = OVector<qint32, qint64>(rec.recordedPacketData.count(), -1);
	packetDropped = OVector<bool, qint64>(rec.recordedPacketData.count(), false);
	rec.recordedQueuedPacketData.setProgressLogOn();
	for (qint64 qei = 0; qei < rec.recordedQueuedPacketData.count(); qei++) {
		RecordedQueuedPacketData qe = rec.recordedQueuedPacketData[qei];
		qint64 pIndex = rec.packetIndexByID(qe.packet_id);
		if (qe.decision != RecordedQueuedPacketData::Queued) {
			packetDropped[pIndex] = true;
			packetExitTimestamp[pIndex] = qe.ts_enqueue;
			packetExitLink[pIndex] = qe.edge_index;
		} else {
			if (qe.ts_exit > packetExitTimestamp[pIndex]) {
				packetExitTimestamp[pIndex] = qe.ts_exit;
				packetExitLink[pIndex] = qe.edge_index;
			}
		}
	}
	rec.recordedQueuedPacketData.setProgressLogOff();
}

bool createPcapFiles(QString expPath, RecordedData &rec, NetGraph g)
{
#if WRITE_PCAP
	OVector<PcapWriter> input(g.edges.count());
	OVector<PcapWriter> dropped(g.edges.count());
	OVector<PcapWriter> output(g.edges.count());

	foreach (NetGraphEdge e, g.edges) {
		input[e.index].init(expPath + "/capture-data/" + QString("edge-%1-in.pcap").arg(e.index + 1), LINKTYPE_RAW);
		dropped[e.index].init(expPath + "/capture-data/" + QString("edge-%1-drop.pcap").arg(e.index + 1), LINKTYPE_RAW);
		output[e.index].init(expPath + "/capture-data/" + QString("edge-%1-out.pcap").arg(e.index + 1), LINKTYPE_RAW);
	}

	qDebug() << "Generating link pcap files...";
	rec.recordedQueuedPacketData.setProgressLogOn();
	for (qint64 qei = 0; qei < rec.recordedQueuedPacketData.count(); qei++) {
		RecordedQueuedPacketData qe = rec.recordedQueuedPacketData[qei];
		int e = qe.edge_index;

		RecordedPacketData p = rec.packetByID(qe.packet_id);
		if (p.isNull()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
			continue;
		}

		QByteArray buffer = QByteArray((const char*)p.buffer, CAPTURE_LENGTH);
		int originalLength = getOriginalPacketLength(buffer);

		// input
		input[e].writePacket(qe.ts_enqueue, originalLength, buffer);
		if (qe.decision != 0) {
			// drop
			dropped[e].writePacket(qe.ts_enqueue, originalLength, buffer);
		} else {
			// output
			output[e].writePacket(qe.ts_exit, originalLength, buffer);
		}
	}
	rec.recordedQueuedPacketData.setProgressLogOff();

	qDebug() << "Generating global pcap files...";
	// Create one global pcap files with everything that entered the emulator
	PcapWriter inputGlobal(expPath + "/capture-data/" + "emulator-in.pcap", LINKTYPE_RAW);
	PcapWriter droppedGlobal(expPath + "/capture-data/" + "emulator-drop.pcap", LINKTYPE_RAW);
	PcapWriter outputGlobal(expPath + "/capture-data/" + "emulator-out.pcap", LINKTYPE_RAW);

	OVector<quint64, qint64> packetExitTimestamp;
	OVector<qint32, qint64> packetExitLink;
	OVector<bool, qint64> packetDropped;
	generatePacketInfo(rec, packetExitTimestamp, packetExitLink, packetDropped);

	for (qint64 pIndex = 0; pIndex < rec.recordedPacketData.count(); pIndex++) {
		if (pIndex % 1000000 == 0) {
			qDebug() << "Progress:" << (pIndex * 100) / rec.recordedPacketData.count() << "%";
		}
		RecordedPacketData p = rec.recordedPacketData[pIndex];
		if (p.isNull()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
			continue;
		}
		bool foundPath;
		NetGraphPath path = g.pathByNodeIndexTry(p.src_id, p.dst_id, foundPath);
		if (!foundPath || path.edgeList.isEmpty()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No path for packet (bad route)";
			continue;
		}

		QByteArray buffer = QByteArray((const char*)p.buffer, CAPTURE_LENGTH);
		int originalLength = getOriginalPacketLength(buffer);
		inputGlobal.writePacket(p.ts_userspace_rx, originalLength, buffer);
		if (packetDropped[pIndex]) {
			droppedGlobal.writePacket(packetExitTimestamp[pIndex], originalLength, buffer);
		} else {
			outputGlobal.writePacket(packetExitTimestamp[pIndex], originalLength, buffer);
		}
	}
#else
	Q_UNUSED(expPath);
	Q_UNUSED(rec);
	Q_UNUSED(g);
#endif

	return true;
}

bool indexCaptureFiles(QString expPath, RecordedData &rec, NetGraph g)
{
	OVector<QFile> fileLinkEvents(g.edges.count());
	OVector<QDataStream> sLinkEvents(g.edges.count());
	OVector<qint64> nLinkEvents(g.edges.count());

	for (int e = 0; e < g.edges.count(); e++) {
		fileLinkEvents[e].setFileName(expPath + QString("/capture-data/link-events-%1.dat").arg(e));
		if (!fileLinkEvents[e].open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		sLinkEvents[e].setDevice(&fileLinkEvents[e]);
		sLinkEvents[e].setVersion(QDataStream::Qt_4_0);
		nLinkEvents[e] = 0;
		// Compatible with OVector and OLazyVector, so start with the count. Write zero now and overwrite later.
		sLinkEvents[e] << nLinkEvents[e];
	}

	OVector<QFile> filePathEvents(g.paths.count());
	OVector<QDataStream> sPathEvents(g.paths.count());
	OVector<qint64> nPathEvents(g.paths.count());

	for (int p = 0; p < g.paths.count(); p++) {
		filePathEvents[p].setFileName(expPath + QString("/capture-data/path-events-%1.dat").arg(p));
		if (!filePathEvents[p].open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		sPathEvents[p].setDevice(&filePathEvents[p]);
		sPathEvents[p].setVersion(QDataStream::Qt_4_0);
		nPathEvents[p] = 0;
		// Compatible with OVector and OLazyVector, so start with the count. Write zero now and overwrite later.
		sPathEvents[p] << nPathEvents[p];
	}

	OVector<QFile> fileLinkPathEvents(g.edges.count() * g.paths.count());
	OVector<QDataStream> sLinkPathEvents(g.edges.count() * g.paths.count());
	OVector<qint64> nLinkPathEvents(g.edges.count() * g.paths.count());

	for (int e = 0; e < g.edges.count(); e++) {
		for (int p = 0; p < g.paths.count(); p++) {
			fileLinkPathEvents[e * g.paths.count() + p].setFileName(expPath + QString("/capture-data/link-path-events-%1-%2.dat").arg(e).arg(p));
			if (!fileLinkPathEvents[e * g.paths.count() + p].open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file" << fileLinkPathEvents[e * g.paths.count() + p].errorString();
				return false;
			}
			sLinkPathEvents[e * g.paths.count() + p].setDevice(&fileLinkPathEvents[e * g.paths.count() + p]);
			sLinkPathEvents[e * g.paths.count() + p].setVersion(QDataStream::Qt_4_0);
			nLinkPathEvents[e * g.paths.count() + p] = 0;
			// Compatible with OVector and OLazyVector, so start with the count. Write zero now and overwrite later.
			sLinkPathEvents[e * g.paths.count() + p] << nLinkPathEvents[e * g.paths.count() + p];
		}
	}

	QHash<NodePair, qint32> nodes2pathIndex;
	for (int i = 0; i < g.paths.count(); i++) {
		nodes2pathIndex[NodePair(g.paths[i].source, g.paths[i].dest)] = i;
	}

	qDebug() << "Grouping queuing events...";
	rec.recordedQueuedPacketData.setProgressLogOn();
	for (qint64 qei = 0; qei < rec.recordedQueuedPacketData.count(); qei++) {
		RecordedQueuedPacketData qe = rec.recordedQueuedPacketData[qei];

		sLinkEvents[qe.edge_index] << qe;
		nLinkEvents[qe.edge_index]++;

		RecordedPacketData p = rec.packetByID(qe.packet_id);
		if (p.isNull()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
			continue;
		}

		if (nodes2pathIndex.contains(NodePair(p.src_id, p.dst_id))) {
			qint32 path = nodes2pathIndex[NodePair(p.src_id, p.dst_id)];
			sPathEvents[path] << qe;
			nPathEvents[path]++;

			qint32 edge = qe.edge_index;
			sLinkPathEvents[edge * g.paths.count() + path] << qe;
			nLinkPathEvents[edge * g.paths.count() + path]++;
		} else {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No path for endpoints";
			continue;
		}
	}
	rec.recordedQueuedPacketData.setProgressLogOff();

	for (int e = 0; e < g.edges.count(); e++) {
		fileLinkEvents[e].seek(0);
		sLinkEvents[e] << nLinkEvents[e];
	}

	for (int p = 0; p < g.paths.count(); p++) {
		filePathEvents[p].seek(0);
		sPathEvents[p] << nPathEvents[p];
	}

	for (int e = 0; e < g.edges.count(); e++) {
		for (int p = 0; p < g.paths.count(); p++) {
			fileLinkPathEvents[e * g.paths.count() + p].seek(0);
			sLinkPathEvents[e * g.paths.count() + p] << nLinkPathEvents[e * g.paths.count() + p];
		}
	}

	OVector<QFile> filePathPackets(g.paths.count());
	OVector<QDataStream> sPathPackets(g.paths.count());
	OVector<qint64> nPathPackets(g.paths.count());

	for (int p = 0; p < g.paths.count(); p++) {
		filePathPackets[p].setFileName(expPath + QString("/capture-data/path-packets-%1.dat").arg(p));
		if (!filePathPackets[p].open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		sPathPackets[p].setDevice(&filePathPackets[p]);
		sPathPackets[p].setVersion(QDataStream::Qt_4_0);
		nPathPackets[p] = 0;
		// Compatible with OVector and OLazyVector, so start with the count. Write zero now and overwrite later.
		sPathPackets[p] << nPathPackets[p];
	}

	qDebug() << "Grouping packet events...";
	for (qint64 pi = 0; pi < rec.recordedPacketData.count(); pi++) {
		if (pi % 1000000 == 0) {
			qDebug() << "Progress:" << (pi * 100) / rec.recordedPacketData.count() << "%";
		}
		RecordedPacketData p = rec.recordedPacketData[pi];
		if (nodes2pathIndex.contains(NodePair(p.src_id, p.dst_id))) {
			qint32 path = nodes2pathIndex[NodePair(p.src_id, p.dst_id)];
			sPathPackets[path] << p;
			nPathPackets[path]++;
		} else {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No path for endpoints";
			continue;
		}
	}

	for (int p = 0; p < g.paths.count(); p++) {
		filePathPackets[p].seek(0);
		sPathPackets[p] << nPathPackets[p];
	}

	return true;
}

bool extractConversations(RecordedData &rec,
						  NetGraph g,
						  QHash<NodePair, PathConversations> &allPathConversations,
						  quint64 &tsStart,
						  quint64 &tsEnd)
{
	// Compute start,end timestamps
	tsStart = 0xFFffFFffFFffFFffULL;
	tsEnd = 0;
	qDebug() << "Computing start/end timestamps...";
	for (qint64 pIndex = 0; pIndex < rec.recordedPacketData.count(); pIndex++) {
		if (pIndex % 1000000 == 0) {
			qDebug() << "Progress:" << (pIndex * 100) / rec.recordedPacketData.count() << "%";
		}
		RecordedPacketData p = rec.recordedPacketData[pIndex];
		tsStart = qMin(tsStart, p.ts_userspace_rx);
		tsEnd = qMax(tsEnd, p.ts_userspace_rx);
	}
	qDebug() << "Computing start/end timestamps...";
	rec.recordedQueuedPacketData.setProgressLogOn();
	for (qint64 qei = 0; qei < rec.recordedQueuedPacketData.count(); qei++) {
		RecordedQueuedPacketData qe = rec.recordedQueuedPacketData[qei];
		if (qe.decision == RecordedQueuedPacketData::Queued) {
			tsEnd = qMax(tsEnd, qe.ts_exit);
		}
	}
	rec.recordedQueuedPacketData.setProgressLogOff();
	if (tsStart == 0xFFffFFffFFffFFffULL) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Possibly an empty experiment";
		tsStart = 0;
	}

	// Compute packet drop info + timings
	OVector<quint64, qint64> packetExitTimestamp;
	OVector<qint32, qint64> packetExitLink;
	OVector<bool, qint64> packetDropped;
	generatePacketInfo(rec, packetExitTimestamp, packetExitLink, packetDropped);

	// Write
	QFile fileFlowPackets("./capture-data/flow-packets.data");
	if (!fileFlowPackets.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
		return false;
	}
	QDataStream sFlowPackets(&fileFlowPackets);
	sFlowPackets.setVersion(QDataStream::Qt_4_0);
	qint64 nextFlowPacketIndex = 0;
	// Compatible with OVector and OLazyVector, so start with the count. Write zero now and overwrite later.
	sFlowPackets << nextFlowPacketIndex;

	allPathConversations.clear();

	qDebug() << "Extracting flows...";
	for (qint64 pIndex = 0; pIndex < rec.recordedPacketData.count(); pIndex++) {
		if (pIndex % 1000000 == 0) {
			qDebug() << "Progress:" << (pIndex * 100) / rec.recordedPacketData.count() << "%";
		}
		RecordedPacketData p = rec.recordedPacketData[pIndex];
		IPHeader iph;
		TCPHeader tcph;
		UDPHeader udph;
		ICMPHeader icmph;

		if (!decodePacket(QByteArray((const char*)p.buffer, CAPTURE_LENGTH), iph, tcph, udph, icmph)) {
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
		bool newFlow = false;
		if (iph.protocolString == "TCP") {
			if (tcph.flagSyn && !tcph.flagAck) {
				nodePair = nodePairFwd;
				portPair = portPairFwd;
				if (!allPathConversations.contains(nodePair)) {
					allPathConversations[nodePair] = PathConversations(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthFwd = 1000.0 * g.pathMaximumBandwidth(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthRet = 1000.0 * g.pathMaximumBandwidth(nodePair.second, nodePair.first);
				}
				PathConversations &pathConversations = allPathConversations[nodePair];
				pathConversations.conversations[portPair] << Conversation(portPair.second.first, portPair.second.second, portPair.first);
				newFlow = true;
				forward = true;
				found = true;
			} else if (allPathConversations.contains(nodePairFwd) && allPathConversations[nodePairFwd].conversations.contains(portPairFwd)) {
				nodePair = nodePairFwd;
				portPair = portPairFwd;
				forward = true;
				found = true;
			} else if (allPathConversations.contains(nodePairRet) && allPathConversations[nodePairRet].conversations.contains(portPairRet)) {
				nodePair = nodePairRet;
				portPair = portPairRet;
				forward = false;
				found = true;
			}
		} else if (iph.protocolString == "UDP") {
			if (allPathConversations.contains(nodePairFwd) && allPathConversations[nodePairFwd].conversations.contains(portPairFwd)) {
				nodePair = nodePairFwd;
				portPair = portPairFwd;
				forward = true;
				found = true;
			} else if (allPathConversations.contains(nodePairRet) && allPathConversations[nodePairRet].conversations.contains(portPairRet)) {
				nodePair = nodePairRet;
				portPair = portPairRet;
				forward = false;
				found = true;
			}
			if (!found) {
				nodePair = nodePairFwd;
				portPair = portPairFwd;
				if (!allPathConversations.contains(nodePair)) {
					allPathConversations[nodePair] = PathConversations(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthFwd = 1000.0 * g.pathMaximumBandwidth(nodePair.first, nodePair.second);
					allPathConversations[nodePair].maxPossibleBandwidthRet = 1000.0 * g.pathMaximumBandwidth(nodePair.second, nodePair.first);
				}
				PathConversations &pathConversations = allPathConversations[nodePair];
				pathConversations.conversations[portPair] << Conversation(portPair.second.first, portPair.second.second, portPair.first);
				newFlow = true;
				forward = true;
				found = true;
			}
		}
		if (!found) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Incomplete stream (no SYN)";
			exit(0);
			continue;
		}

		if (newFlow) {
			if (iph.protocolString == "TCP") {
				qDebug() << "New flow:" << iph.sourceString << iph.destString << iph.protocolString
						 << tcph.sourcePort << tcph.destPort
						 << (tcph.flagSyn ? "SYN" : "")
						 << (tcph.flagAck ? "ACK" : "")
						 << (tcph.flagRst ? "RST" : "")
						 << (tcph.flagFin ? "FIN" : "");
			} else if (iph.protocolString == "UDP") {
				qDebug() << "New flow:" << iph.sourceString << iph.destString << iph.protocolString
						 << udph.sourcePort << udph.destPort;
			}
		}

		PathConversations &pathConversations = allPathConversations[nodePair];
		Conversation &conversation = pathConversations.conversations[portPair].last();
		Flow &flow = forward ? conversation.fwdFlow : conversation.retFlow;

		bool foundPath;
		NetGraphPath path = g.pathByNodeIndexTry(p.src_id, p.dst_id, foundPath);
		if (!foundPath || path.edgeList.isEmpty()) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "No path for packet (bad route)";
			exit(0);
			continue;
		}

		FlowPacket flowPacket;
		flowPacket.packetId = p.packet_id;
		flowPacket.packetIndex = pIndex;
		flowPacket.tsSent = p.ts_userspace_rx;
		flowPacket.received = !packetDropped[pIndex];
		if (flowPacket.received) {
			flowPacket.tsReceived = packetExitTimestamp[pIndex];
		} else {
			flowPacket.tsReceived = 0;
		}
		flowPacket.dropped = packetDropped[pIndex];
		if (flowPacket.dropped) {
			flowPacket.dropEdgeId = packetExitLink[pIndex];
			flowPacket.tsDrop = packetExitTimestamp[pIndex];
		}
		flowPacket.ipHeader = iph;
		flowPacket.tcpHeader = tcph;
		flowPacket.udpHeader = udph;

		sFlowPackets << flowPacket;
		flow.packets << nextFlowPacketIndex;
		nextFlowPacketIndex++;
	}

	fileFlowPackets.seek(0);
	sFlowPackets << nextFlowPacketIndex;

	QFile fileAllPathConversations("./capture-data/all-path-conversations.data");
	if (!fileAllPathConversations.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
		return false;
	}
	QDataStream sAllPathConversations(&fileAllPathConversations);
	sAllPathConversations.setVersion(QDataStream::Qt_4_0);
	sAllPathConversations << allPathConversations;

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
												 OLazyVector<FlowPacket> flowPackets,
												 QString flowName,
												 quint64 tsStart,
												 QList<FlowIntDataItem> flowLosses,
												 QList<FlowAnnotation> &annotations)
{
	annotations.clear();
	QList<FlowIntDataItem> result;

	// time -> size of packet received
	QMap<quint64, QList<FlowIntDataItem> > receivedRaw;
	foreach (qint64 pi, flow.packets) {
		FlowPacket p = flowPackets[pi];
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
														   OLazyVector<FlowPacket> flowPackets,
														   QString flowName,
														   quint64 tsStart,
														   QList<FlowIntDataItem> flowLosses,
														   QList<FlowAnnotation> &annotations,
														   qreal window = 0)
{
	QList<FlowIntDataItem> receivedRaw = computeReceivedRawForFlow(flow,
																   flowPackets,
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
										  OLazyVector<FlowPacket> flowPackets,
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
	foreach (qint64 pi, flow.packets) {
		FlowPacket p = flowPackets[pi];
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
										 OLazyVector<FlowPacket> flowPackets,
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
	foreach (qint64 pi, fwdFlow.packets) {
		FlowPacket p = flowPackets[pi];
		if (p.received) {
			oneWayDelays[ns2ms(p.tsReceived - tsStart)].first = ns2ms(p.tsReceived - p.tsSent);
		}
	}
	foreach (qint64 pi, retFlow.packets) {
		FlowPacket p = flowPackets[pi];
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
													 OLazyVector<FlowPacket> flowPackets,
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
	foreach (qint64 pi, flow.packets) {
		FlowPacket p = flowPackets[pi];
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
QList<FlowIntDataItem> computeLossesForFlow(Flow fwdFlow,
											Flow retFlow,
											OLazyVector<FlowPacket> flowPackets,
											QString flowName,
											quint64 tsStart)
{
	QList<FlowIntDataItem> result;

	// time -> code (0/1 as specified above)
	QMap<quint64, QList<qint64> > losses;
	foreach (qint64 pi, fwdFlow.packets) {
		FlowPacket p = flowPackets[pi];
		if (p.dropped) {
			losses[ns2ms(p.tsDrop - tsStart)] << LossFwd;
		}
	}
	foreach (qint64 pi, retFlow.packets) {
		FlowPacket p = flowPackets[pi];
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
	QFile fileFlowPackets(expPath + "/capture-data/flow-packets.data");
	if (!fileFlowPackets.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileFlowPackets.fileName();
		return;
	}

	OLazyVector<FlowPacket> flowPackets(&fileFlowPackets, FlowPacket::getSerializedSize());

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
																			 flowPackets,
																			 fwdFlowName,
																			 tsStart);
				// compute RTT(t)
				// Note: there is only one RTT for fwd/ret path since RTT is symmetrical
				QList<FlowAnnotation> flowRTTLossAnnotations;
				QList<FlowIntDataItem> flowRTT = computeRttForFlow(conversation.fwdFlow,
																   conversation.retFlow,
																   flowPackets,
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
																			 flowPackets,
																			 namedFlow.first,
																			 tsStart);
					// compute raw received data(t)
					QList<FlowAnnotation> flowReceivedRawLossAnnotations;
					QList<FlowIntDataItem> flowReceivedRaw = computeReceivedRawForFlow(namedFlow.second,
																					   flowPackets,
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
																										   flowPackets,
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
																					flowPackets,
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
																					flowPackets,
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
																		 flowPackets,
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
																							   flowPackets,
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
																							 flowPackets,
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

	{
		QDir dir;
		dir.cd(expPath);
		dir.mkpath(QString("capture-data"));
	}

	packetCount = rec.recordedPacketData.count();
	queueEventCount = rec.recordedQueuedPacketData.count();
	qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Packet count:" << rec.recordedPacketData.count();
	qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Queue events:" << rec.recordedQueuedPacketData.count();
	if (rec.saturated) {
		qint64 durationPackets = rec.recordedPacketData.last().ts_userspace_rx - rec.recordedPacketData.first().ts_userspace_rx;
		qint64 durationEvents = rec.recordedQueuedPacketData.last().ts_enqueue - rec.recordedQueuedPacketData.first().ts_enqueue;
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "WARNING: recording truncated due to memory limit!!!! at duration"
				 << withCommasStr(qMin(durationPackets, durationEvents)) << "ns <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

	}

	NetGraph g;
	if (!loadGraph(expPath, g)) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Aborting";
		return false;
	}

	if (!createPcapFiles(expPath, rec, g)) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Aborting";
		return false;
	}

	if (!indexCaptureFiles(expPath, rec, g)) {
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
	if (rec.recordedPacketData.count() < 10 * 1000)
		doSeqAckAnalysis(rootPath, expPath, allPathConversations, tsStart, tsEnd);

	return true;
}

bool loadFlowPackets(OLazyVector<FlowPacket> &flowPackets, QFile &fileFlowPackets)
{
	if (!fileFlowPackets.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileFlowPackets.fileName();
		return false;
	}

	QDataStream in(&fileFlowPackets);
	in.setVersion(QDataStream::Qt_4_0);
	flowPackets.init(&fileFlowPackets, RecordedPacketData::getSerializedSize());

	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error reading file:" << fileFlowPackets.fileName();
		return false;
	}

	return true;
}

bool loadAllPathConversations(QHash<NodePair, PathConversations> &allPathConversations, QString expPath)
{
	QFile fileAllPathConversations(expPath + "/capture-data/all-path-conversations.data");
	if (!fileAllPathConversations.open(QIODevice::ReadOnly)) {
		qError() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
		return false;
	}
	QDataStream sAllPathConversations(&fileAllPathConversations);
	sAllPathConversations.setVersion(QDataStream::Qt_4_0);
	sAllPathConversations >> allPathConversations;
	if (sAllPathConversations.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error reading file:" << fileAllPathConversations.fileName();
		return false;
	}
	return true;
}

bool postProcessLineRecordShowFlows(QString expPath, NetGraph &g)
{
	QFile fileFlowPackets(expPath + "/capture-data/flow-packets.data");
	OLazyVector<FlowPacket> flowPackets;
	if (!loadFlowPackets(flowPackets, fileFlowPackets))
		return false;

	QHash<NodePair, PathConversations> allPathConversations;
	if (!loadAllPathConversations(allPathConversations, expPath))
		return false;

	QHash<NodePair, qint32> nodes2pathIndex;
	for (int i = 0; i < g.paths.count(); i++) {
		nodes2pathIndex[NodePair(g.paths[i].source, g.paths[i].dest)] = i;
	}

	int numFlows = 0;
	foreach (NodePair nodePair, allPathConversations.uniqueKeys()) {
		int numFlowsPath = 0;
		PathConversations &pathConversations = allPathConversations[nodePair];
		foreach (ProtoPortPair portPair, pathConversations.conversations.uniqueKeys()) {
			foreach (Conversation conversation, pathConversations.conversations[portPair]) {
				foreach (Flow flow, conversation.flows()) {
					numFlowsPath++;
				}
			}
		}
		QString pString = QString("p%1").arg(nodes2pathIndex[nodePair]+1);
		QString pMaxString = QString("p%1").arg(g.paths.count());
		while (pString.length() < pMaxString.length()) {
			pString = QString(" ") + pString;
		}
		qDebug() << "path =" << pString << "nodePair0 =" << nodePair << "numFlows =" << numFlowsPath;
		numFlows += numFlowsPath;
	}
	qDebug() << "numFlows =" << numFlows;

	return true;
}

qreal interpolateQueueLoad(const NetGraphEdge &e, quint64 t1, qreal y1, quint64 t2)
{
	qreal qcapacity = e.queueLength * ETH_FRAME_LEN;
	qreal load1 = y1 * qcapacity;
	qreal load2 = qMax(0.0, load1 - (t2 - t1) * e.bandwidth / 1.0e6);
	qreal y2 = load2 / qcapacity;
	return y2;
}

typedef qreal (*RecordFunction)(qreal, qreal);

qreal recordFunctionMax(qreal a, qreal b)
{
	return qMax(a, b);
}

qreal recordFunctionSum(qreal a, qreal b)
{
	return a + b;
}

void recordSample(qreal &sink, qreal value, RecordFunction recordFunction)
{
	sink = recordFunction(sink, value);
}

class IntervalLossiness {
public:
	QSet<Link> lossyLinks;
	QSet<Path> lossyPaths;
	QSet<Link> trafficLinks;
	QSet<Path> trafficPaths;
};

QString toJson(const IntervalLossiness &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.lossyLinks);
	jsonObjectPrinterAddMember(p, d.lossyPaths);
	jsonObjectPrinterAddMember(p, d.trafficLinks);
	jsonObjectPrinterAddMember(p, d.trafficPaths);
	return p.json();
}

QColor getLoadColor(qreal load, qreal loss)
{
	QColor color;
	if (load == 0 && loss == 0) {
		color = Qt::white;
	} else {
		const qreal hueGreen = 120/360.0;
		const qreal hueYellow = 60/360.0;
		const qreal hueRed = 0;

		if (loss == 0) {
			qreal hue = hueGreen;
			qreal sat = 1.0;
			qreal val = 1.0 - qMax(0.0, qMin(1.0, load)) * 0.5;
			color.setHsvF(hue, sat, val, 1.0);
		} else {
			qreal hue = hueYellow + qMax(0.0, qMin(1.0, loss)) * (hueRed - hueYellow);
			qreal sat = 1.0;
			qreal val = 1.0 - qMax(0.0, qMin(1.0, load)) * 0.5;
			color.setHsvF(hue, sat, val, 1.0);
		}
	}
	return color;
}

QImage makeQueueLoadLegend()
{
	int padding = 40;
	int h = 14;
	int blockSize = 10;
	int numBlocks = 51;
	int yOffset = 0;
	int w = 2 * padding + numBlocks * blockSize;

	QImage image(w, 11 * h, QImage::Format_RGB32);
	image.fill(Qt::white);

	QPainter painter;
	painter.begin(&image);

	yOffset += h;

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter,
					 QString("The link load is computed as the queue occupancy, normalized to [0, 1]."));
	yOffset += h;
	yOffset += h;

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter, QString("Legend: link load, without drops"));
	yOffset += h;
	for (int s = 0; s < numBlocks; s++) {
		painter.setPen(Qt::black);
		QColor color = getLoadColor(s / (qreal)numBlocks, 0);
		painter.fillRect(padding + s * blockSize,
						 yOffset + (h - blockSize) / 2,
						 blockSize,
						 blockSize,
						 color);
		if (s == 0 ||
			s == numBlocks - 1 ||
			s == numBlocks / 2) {
			painter.drawText(padding + (s - 2) * blockSize,
							 yOffset + h,
							 5 * blockSize,
							 h,
							 Qt::AlignCenter,
							 QString("%1").arg(s/(qreal)(numBlocks-1)));
		}
	}
	painter.setPen(Qt::black);
	painter.drawRect(padding,
					 yOffset + (h - blockSize) / 2,
					 numBlocks * blockSize,
					 blockSize);
	yOffset += h; // plot
	yOffset += h; // text labels
	yOffset += h; // space

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter, QString("Legend: link load, with drops"));
	yOffset += h;
	for (int s = 0; s < numBlocks; s++) {
		painter.setPen(Qt::black);
		QColor color = getLoadColor(0, 1);
		painter.fillRect(padding + s * blockSize,
						 yOffset + (h - blockSize) / 2,
						 blockSize,
						 blockSize,
						 color);
		if (s == 0 ||
			s == numBlocks - 1 ||
			s == numBlocks / 2) {
			painter.drawText(padding + (s - 2) * blockSize,
							 yOffset + h,
							 5 * blockSize,
							 h,
							 Qt::AlignCenter,
							 QString("%1").arg(s/(qreal)(numBlocks-1)));
		}
	}
	painter.setPen(Qt::black);
	painter.drawRect(padding,
					 yOffset + (h - blockSize) / 2,
					 numBlocks * blockSize,
					 blockSize);
	yOffset += h; // plot
	yOffset += h; // text labels
	yOffset += h; // space

	painter.end();
	return image;
}

QImage makePathLoadLegend()
{
	int padding = 40;
	int h = 14;
	int blockSize = 10;
	int numBlocks = 51;
	int numDropPlots = 4;
	int yOffset = 0;
	int w = 2 * padding + numBlocks * blockSize;

	QImage image(w, ((numDropPlots + 1) * 4 + 3) * h, QImage::Format_RGB32);
	image.fill(Qt::white);

	QPainter painter;
	painter.begin(&image);

	yOffset += h;

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter,
					 QString("The path load is computed as the number of packets / 10, normalized to [0,1]."));
	yOffset += h;
	yOffset += h;

	for (int d = 0; d <= numDropPlots; d++) {
		painter.setPen(Qt::black);
		painter.setBrush(Qt::NoBrush);
		painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter,
						 QString("Legend: path load, loss rate %1%").arg(d * 100.0 / (qreal)numDropPlots));
		yOffset += h;
		for (int s = 0; s < numBlocks; s++) {
			painter.setPen(Qt::black);
			QColor color = getLoadColor(s / (qreal)numBlocks, d / (qreal)numDropPlots);
			painter.fillRect(padding + s * blockSize,
							 yOffset + (h - blockSize) / 2,
							 blockSize,
							 blockSize,
							 color);
			if (s == 0 ||
				s == numBlocks - 1 ||
				s == numBlocks / 2) {
				painter.drawText(padding + (s - 2) * blockSize,
								 yOffset + h,
								 5 * blockSize,
								 h,
								 Qt::AlignCenter,
								 QString("%1").arg(s/(qreal)(numBlocks-1)));
			}
		}
		painter.setPen(Qt::black);
		painter.drawRect(padding,
						 yOffset + (h - blockSize) / 2,
						 numBlocks * blockSize,
						 blockSize);
		yOffset += h; // plot
		yOffset += h; // text labels
		yOffset += h; // space
	}

	painter.end();
	return image;
}

QColor getDelayColor(qreal delay, qreal load, qreal loss)
{
	QColor color;
	if (load == 0) {
		color = Qt::white;
	} else if (loss == 1.0) {
		color = Qt::black;
	} else {
		const qreal hueBlue = 240/360.0;
		const qreal hueGreen = 120/360.0;
		const qreal hueOrange = 42/360.0;
		const qreal hueRed = 0;

		if (delay <= 50.0) {
			qreal hue = hueBlue;
			qreal sat = 1.0;
			qreal val = 1.0;
			color.setHsvF(hue, sat, val, 1.0);
		} else if (delay <= 120.0) {
			qreal hue = hueBlue + (delay - 50.0)/(120.0 - 50.0) * (hueGreen - hueBlue);
			qreal sat = 1.0;
			qreal val = 1.0;
			color.setHsvF(hue, sat, val, 1.0);
		} else if (delay <= 250.0) {
			qreal hue = hueGreen + (delay - 120.0)/(250.0 - 120.0) * (hueOrange - hueGreen);
			qreal sat = 1.0;
			qreal val = 1.0;
			color.setHsvF(hue, sat, val, 1.0);
		} else if (delay <= 500.0) {
			qreal hue = hueOrange + (delay - 250.0)/(500.0 - 250.0) * (hueRed - hueOrange);
			qreal sat = 1.0;
			qreal val = 1.0;
			color.setHsvF(hue, sat, val, 1.0);
		} else {
			qreal hue = hueRed;
			qreal sat = 1.0;
			qreal val = 1.0;
			color.setHsvF(hue, sat, val, 1.0);
		}
	}
	return color;
}

QImage makePathDelayLegend()
{
	int padding = 40;
	int h = 14;
	int blockSize = 10;
	int numBlocks = 51;
	int yOffset = 0;
	int w = 2 * padding + numBlocks * blockSize;

	QImage image(w, 14 * h, QImage::Format_RGB32);
	image.fill(Qt::white);

	QPainter painter;
	painter.begin(&image);

	yOffset += h;

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter,
					 QString("Legend: path delay, loss rate < 100%"));
	yOffset += h;
	for (int s = 0; s < numBlocks; s++) {
		qreal d = s * 500.0 / (qreal)(numBlocks - 1);
		painter.setPen(Qt::black);
		QColor color = getDelayColor(d, 1, 0);
		painter.fillRect(padding + s * blockSize,
						 yOffset + (h - blockSize) / 2,
						 blockSize,
						 blockSize,
						 color);
		if (s == 0 ||
			s == numBlocks - 1 ||
			s == numBlocks / 2 ||
			s == numBlocks / 4 ||
			s == 3 * numBlocks / 4) {
			painter.drawText(padding + (s - 2) * blockSize,
							 yOffset + h,
							 5 * blockSize,
							 h,
							 Qt::AlignCenter,
							 QString("%1 ms").arg(d));
		}
	}
	painter.setPen(Qt::black);
	painter.drawRect(padding,
					 yOffset + (h - blockSize) / 2,
					 numBlocks * blockSize,
					 blockSize);
	yOffset += h; // plot
	yOffset += h; // text labels
	yOffset += h; // space

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter,
					 QString("Legend: path delay, loss rate 100%"));
	yOffset += h;
	for (int s = 0; s < numBlocks; s++) {
		qreal d = s * 500.0 / (qreal)(numBlocks - 1);
		painter.setPen(Qt::black);
		QColor color = getDelayColor(d, 1, 1);
		painter.fillRect(padding + s * blockSize,
						 yOffset + (h - blockSize) / 2,
						 blockSize,
						 blockSize,
						 color);
		if (s == 0 ||
			s == numBlocks - 1 ||
			s == numBlocks / 2 ||
			s == numBlocks / 4 ||
			s == 3 * numBlocks / 4) {
			painter.drawText(padding + (s - 2) * blockSize,
							 yOffset + h,
							 5 * blockSize,
							 h,
							 Qt::AlignCenter,
							 QString("%1 ms").arg(d));
		}
	}
	painter.setPen(Qt::black);
	painter.drawRect(padding,
					 yOffset + (h - blockSize) / 2,
					 numBlocks * blockSize,
					 blockSize);
	yOffset += h; // plot
	yOffset += h; // text labels
	yOffset += h; // space

	painter.setPen(Qt::black);
	painter.setBrush(Qt::NoBrush);
	painter.drawText(padding, yOffset, w - padding, h, Qt::AlignLeft | Qt::AlignVCenter,
					 QString("Legend: path delay, no traffic"));
	yOffset += h;
	for (int s = 0; s < numBlocks; s++) {
		qreal d = s * 500.0 / (qreal)(numBlocks - 1);
		painter.setPen(Qt::black);
		QColor color = getDelayColor(d, 0, 0);
		painter.fillRect(padding + s * blockSize,
						 yOffset + (h - blockSize) / 2,
						 blockSize,
						 blockSize,
						 color);
		if (s == 0 ||
			s == numBlocks - 1 ||
			s == numBlocks / 2 ||
			s == numBlocks / 4 ||
			s == 3 * numBlocks / 4) {
			painter.drawText(padding + (s - 2) * blockSize,
							 yOffset + h,
							 5 * blockSize,
							 h,
							 Qt::AlignCenter,
							 QString("%1 ms").arg(d));
		}
	}
	painter.setPen(Qt::black);
	painter.drawRect(padding,
					 yOffset + (h - blockSize) / 2,
					 numBlocks * blockSize,
					 blockSize);
	yOffset += h; // plot
	yOffset += h; // text labels
	yOffset += h; // space

	painter.end();
	return image;
}

bool postProcessLineRecord(QString expPath, QString srcDir, QString tag)
{
	{
		QDir dir;
		dir.cd(expPath);
		dir.mkpath(QString("capture-plots"));
	}
	makeQueueLoadLegend().save(QString("%1/capture-plots/legend-link-load.png").arg(expPath));
	makePathLoadLegend().save(QString("%1/capture-plots/legend-path-load.png").arg(expPath));
	makePathDelayLegend().save(QString("%1/capture-plots/legend-path-delay.png").arg(expPath));

	NetGraph g;
	if (!loadGraph(expPath, g)) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Aborting";
		return false;
	}

	ErrorAnalysisData errorAnalysisData;
	if (!errorAnalysisData.loadFromFile(tag + "/error-analysis-point-measurements.data")) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Aborting";
		return false;
	}

	postProcessLineRecordShowFlows(expPath, g);

	RecordedData rec;
	if (!rec.load(expPath + "/" + "recorded.line-rec")) {
		qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not open/read file";
		return false;
	}

	OVector<quint64, qint64> packetExitTimestamp;
	OVector<qint32, qint64> packetExitLink;
	OVector<bool, qint64> packetDropped;
	generatePacketInfo(rec, packetExitTimestamp, packetExitLink, packetDropped);

	OVector<quint16, qint64> packetSize(rec.recordedPacketData.count());
	for (qint64 pIndex = 0; pIndex < rec.recordedPacketData.count(); pIndex++) {
		RecordedPacketData p = rec.recordedPacketData[pIndex];
		QByteArray buffer = QByteArray((const char*)p.buffer, CAPTURE_LENGTH);
		packetSize[pIndex] = getOriginalPacketLength(buffer) + 14;
	}

	QVector<QVector<QVector<qint64> > > link2interval2eventIndices(g.edges.count());
	for (int e = 0; e < g.edges.count(); e++) {
		link2interval2eventIndices[e].resize(errorAnalysisData.numIntervals);

		QFile fileLinkEvents(expPath + QString("/capture-data/link-events-%1.dat").arg(e));
		if (!fileLinkEvents.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<RecordedQueuedPacketData> linkEvents(&fileLinkEvents, RecordedQueuedPacketData::getSerializedSize());

		for (qint64 qei = 0; qei < linkEvents.count(); qei++) {
			RecordedQueuedPacketData event = linkEvents[qei];
			int i = errorAnalysisData.time2interval(event.ts_enqueue);
			if (i < 0)
				continue;
			link2interval2eventIndices[e][i].append(qei);
		}
	}

	qDebug() << "Generating per interval, link, path pcap files...";
	for (int i = 0; i < errorAnalysisData.numIntervals; i++) {
		QDir dir;
		dir.cd(expPath);
		dir.mkpath(QString("capture-data/i%1").arg(i+1));
	}
	QHash<NodePair, qint32> nodes2pathIndex;
	for (int i = 0; i < g.paths.count(); i++) {
		nodes2pathIndex[NodePair(g.paths[i].source, g.paths[i].dest)] = i;
	}

#if WRITE_PCAP
	for (Link e = 0; e < g.edges.count(); e++) {
		qDebug() << "Link" << (e+1);

		QFile fileLinkEvents(expPath + QString("/capture-data/link-events-%1.dat").arg(e));
		if (!fileLinkEvents.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<RecordedQueuedPacketData> linkEvents(&fileLinkEvents, RecordedQueuedPacketData::getSerializedSize());

		for (int i = 0; i < errorAnalysisData.numIntervals; i++) {
			PcapWriter input;
			OVector<PcapWriter> inputPerPath(g.paths.count());

			input.init(expPath + "/capture-data/" + QString("i%1/edge-%2-in-i%1.pcap").arg(i+1).arg(e+1), LINKTYPE_RAW);
			foreach (Path p, errorAnalysisData.link2paths[e]) {
				inputPerPath[p].init(expPath + "/capture-data/" + QString("i%1/edge-%2-path-%3-in-i%1.pcap").arg(i+1).arg(e+1).arg(p+1), LINKTYPE_RAW);
			}

			foreach (qint64 qei, link2interval2eventIndices[e][i]) {
				RecordedQueuedPacketData qe = linkEvents[qei];
				Q_ASSERT_FORCE(e == qe.edge_index);

				RecordedPacketData p = rec.packetByID(qe.packet_id);
				if (p.isNull()) {
					qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Null packet";
					continue;
				}

				QByteArray buffer = QByteArray((const char*)p.buffer, CAPTURE_LENGTH);
				int originalLength = getOriginalPacketLength(buffer);

				input.writePacket(qe.ts_enqueue, originalLength, buffer);
				if (nodes2pathIndex.contains(NodePair(p.src_id, p.dst_id))) {
					qint32 path = nodes2pathIndex[NodePair(p.src_id, p.dst_id)];
					inputPerPath[path].writePacket(qe.ts_enqueue, originalLength, buffer);
				} else {
					qDebug() << "bad path";
				}
			}
		}
	}
	qDebug() << "Generating per interval, path pcap files...";
	{
		OVector<PcapWriter> inputPath(g.paths.count());
		OVector<qint32> inputPathInterval(g.paths.count(), -1);
		for (qint64 ip = 0; ip < rec.recordedPacketData.count(); ip++) {
			RecordedPacketData p = rec.recordedPacketData[ip];
			if (nodes2pathIndex.contains(NodePair(p.src_id, p.dst_id))) {
				Path path = nodes2pathIndex[NodePair(p.src_id, p.dst_id)];
				qint32 i = errorAnalysisData.time2interval(p.ts_userspace_rx);
				if (i < 0)
					continue;

				while (inputPathInterval[path] < i) {
					inputPath[path].cleanup();
					inputPathInterval[path]++;
					inputPath[path].init(expPath + "/capture-data/" + QString("i%1/path-%2-in-i%1.pcap").arg(inputPathInterval[path]+1).arg(path+1), LINKTYPE_RAW);
				}
				Q_ASSERT_FORCE(inputPathInterval[path] == i);

				QByteArray buffer = QByteArray((const char*)p.buffer, CAPTURE_LENGTH);
				int originalLength = getOriginalPacketLength(buffer);
				inputPath[path].writePacket(p.ts_userspace_rx, originalLength, buffer);
			} else {
				qDebug() << "bad path";
			}
		}
		for (Path path = 0; path < g.paths.count(); path++) {
			while (inputPathInterval[path] < errorAnalysisData.numIntervals - 1) {
				inputPath[path].cleanup();
				inputPathInterval[path]++;
				inputPath[path].init(expPath + "/capture-data/" + QString("i%1/path-%2-in-i%1.pcap").arg(inputPathInterval[path]+1).arg(path+1), LINKTYPE_RAW);
			}
			inputPath[path].cleanup();
		}
	}
#endif

	QVector<QVector<QVector<qint64> > > path2interval2eventIndices(g.paths.count());
	for (int p = 0; p < g.paths.count(); p++) {
		path2interval2eventIndices[p].resize(errorAnalysisData.numIntervals);

		QFile filePathEvents(expPath + QString("/capture-data/path-events-%1.dat").arg(p));
		if (!filePathEvents.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<RecordedQueuedPacketData> pathEvents(&filePathEvents, RecordedQueuedPacketData::getSerializedSize());

		for (qint64 pei = 0; pei < pathEvents.count(); pei++) {
			RecordedQueuedPacketData event = pathEvents[pei];
			int i = errorAnalysisData.time2interval(event.ts_enqueue);
			if (i < 0)
				continue;
			path2interval2eventIndices[p][i].append(pei);
		}
	}

	for (int i = 0; i < errorAnalysisData.numIntervals; i++) {
		QDir dir;
		dir.cd(expPath);
		dir.mkpath(QString("capture-plots/i%1").arg(i + 1));
	}

	// Plotting parameters

	// Number of samples per interval
	// 1 ms = 1M ns
	errorAnalysisData.samplingPeriod = 1ULL * 1000 * 1000;

	const qint64 nSamples = errorAnalysisData.intervalSize / errorAnalysisData.samplingPeriod;
	Q_ASSERT_FORCE(errorAnalysisData.intervalSize % errorAnalysisData.samplingPeriod == 0);

	// Index intervals with losses
	QVector<IntervalLossiness> intervalLossiness(errorAnalysisData.numIntervals);

	const int textWidth = 70;
	const int blockSize = 10;
	const int textPadding = 4;
	const int w = textWidth + nSamples * blockSize + textWidth;
	const int h = 14;
	const qreal maxPPS = 10.0;
	// The same value is used in the emulator when taking interval measurements
	const int minPacketSize = 1400;

	// for each link
	//   for each interval
	//     plot queue state

	qDebug() << "Per link interval plots (data)";
	for (Link e = 0; e < g.edges.count(); e++) {
		qDebug() << "Link" << (e+1);
		QFile fileLinkEvents(expPath + QString("/capture-data/link-events-%1.dat").arg(e));
		if (!fileLinkEvents.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<RecordedQueuedPacketData> linkEvents(&fileLinkEvents, RecordedQueuedPacketData::getSerializedSize());

		QFile fileLinkState(expPath + QString("/capture-data/link-state-sampled-%1.dat").arg(e));
		if (!fileLinkState.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		QDataStream sLinkState(&fileLinkState);
		sLinkState.setVersion(QDataStream::Qt_4_0);
		// Compatibility with OLazyVector
		sLinkState << qint64(0);

		QFile fileLinkDrops(expPath + QString("/capture-data/link-drops-sampled-%1.dat").arg(e));
		if (!fileLinkDrops.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		QDataStream sLinkDrops(&fileLinkDrops);
		sLinkDrops.setVersion(QDataStream::Qt_4_0);
		// Compatibility with OLazyVector
		sLinkDrops << qint64(0);

		QVector<qreal> queueLoad(nSamples);
		queueLoad.fill(0.0, nSamples);

		QVector<qreal> queueDrops(nSamples);
		queueDrops.fill(0.0, nSamples);

		quint64 t1 = errorAnalysisData.tsStart;
		qint64 i1 = errorAnalysisData.time2interval(t1);
		Q_ASSERT_FORCE(i1 >= 0);
		qint64 s1 = errorAnalysisData.time2sample(t1);
		qreal y1 = 0.0;

		for (qint64 qei = 0; qei <= linkEvents.count(); qei++) {
			bool dummy = false;
			RecordedQueuedPacketData event;
			if (qei < linkEvents.count()) {
				event = linkEvents[qei];
				if (packetSize[rec.packetIndexByID(event.packet_id)] < minPacketSize)
					continue;
			} else {
				// Dummy event for the last timestamp
				event.ts_enqueue = errorAnalysisData.tsEnd;
				event.qload = 0;
				event.qcapacity = 1;
				event.decision = RecordedQueuedPacketData::Queued;
				dummy = true;
			}

			quint64 t2 = event.ts_enqueue;
			qint64 i2 = errorAnalysisData.time2interval(t2);
			if (i2 < 0)
				continue;
			Q_ASSERT_FORCE(i2 >= i1);
			qint64 s2 = errorAnalysisData.time2sample(t2);
			qreal y2 = event.qload / (qreal)event.qcapacity;
			qreal d2 = (event.decision != RecordedQueuedPacketData::Queued) ? 1 : 0;
			qint64 s1o = s1;
			if (!dummy)
				intervalLossiness[i2].trafficLinks.insert(e);
			if (d2 > 0) {
				intervalLossiness[i2].lossyLinks.insert(e);
			}

			while (i1 <= i2) {
				for (qint64 s = s1; s <= s2; s++) {
					if (errorAnalysisData.sample2interval(s) != i1) {
						s1 = s;
						break;
					}
					QPair<quint64, quint64> sampleTimestamps = errorAnalysisData.sample2time(s);
					if (s > s1o) {
						qreal ys = interpolateQueueLoad(g.edges[e], t1, y1, sampleTimestamps.first);
						recordSample(queueLoad[s % nSamples], ys, recordFunctionMax);
					}
					if (s < s2) {
						qreal ye = interpolateQueueLoad(g.edges[e], t1, y1, sampleTimestamps.second);
						recordSample(queueLoad[s % nSamples], ye, recordFunctionMax);
					}
				}
				if (i1 < i2) {
					// dump samples for interval i1
					foreach (qreal sample, queueLoad) {
						sLinkState << sample;
					}
					queueLoad.fill(0.0, nSamples);

					foreach (qreal sample, queueDrops) {
						sLinkDrops << sample;
					}
					queueDrops.fill(0.0, nSamples);
				}
				i1++;
			}
			if (!dummy) {
				recordSample(queueLoad[s2 % nSamples], y2, recordFunctionMax);
				recordSample(queueDrops[s2 % nSamples], d2, recordFunctionSum);
			}
			i1 = i2;
			s1 = s2;
			y1 = y2;
			t1 = t2;
		}

		fileLinkState.seek(0);
		sLinkState << qint64(errorAnalysisData.numIntervals * nSamples);

		fileLinkDrops.seek(0);
		sLinkDrops << qint64(errorAnalysisData.numIntervals * nSamples);
	}

	// Plot
	qDebug() << "Per link interval plots (plots)";
	for (Link e = 0; e < g.edges.count(); e++) {
		qDebug() << "Link" << (e+1);
		QFile fileLinkState(expPath + QString("/capture-data/link-state-sampled-%1.dat").arg(e));
		if (!fileLinkState.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<qreal> linkState(&fileLinkState);

		QFile fileLinkDrops(expPath + QString("/capture-data/link-drops-sampled-%1.dat").arg(e));
		if (!fileLinkDrops.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<qreal> linkDrops(&fileLinkDrops);

		for (int i = 0; i < errorAnalysisData.numIntervals; i++) {
			QVector<qreal> queueLoad(nSamples);
			queueLoad.fill(0.0, nSamples);

			QVector<qreal> queueDrops(nSamples);
			queueDrops.fill(0.0, nSamples);

			for (int s = 0; s < nSamples; s++) {
				queueLoad[s] = linkState[i * nSamples + s];
				queueDrops[s] = linkDrops[i * nSamples + s];
			}

			// Plot
			QImage image(w, h, QImage::Format_RGB32);
			image.fill(Qt::white);

			QPainter painter;
			painter.begin(&image);
			painter.setPen(Qt::black);
			painter.setBrush(Qt::NoBrush);
			painter.drawText(textPadding, 0, textWidth - 2 * textPadding, h, Qt::AlignLeft | Qt::AlignVCenter, QString("e%1").arg(e + 1));
			for (int s = 0; s < nSamples; s++) {
				painter.setPen(Qt::black);
				QColor color = getLoadColor(queueDrops[s] == 0 ? queueLoad[s] : 0, qMin(queueDrops[s], 1.0));
				painter.fillRect(textWidth + s * blockSize,
								 (h - blockSize) / 2,
								 blockSize,
								 blockSize, color);
				painter.drawRect(textWidth + s * blockSize,
								 (h - blockSize) / 2,
								 blockSize,
								 blockSize);
			}
			painter.end();
			image.save(QString("%1/capture-plots/i%2/e%3.png")
					   .arg(expPath)
					   .arg(i + 1)
					   .arg(e + 1));
		}
	}

	// for each link
	//   for each interval
	//     plot queue state

	qDebug() << "Per path interval plots (data)";
	for (Path p = 0; p < g.paths.count(); p++) {
		qDebug() << "Path" << (p+1);
		QFile filePathPackets(expPath + QString("/capture-data/path-packets-%1.dat").arg(p));
		if (!filePathPackets.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<RecordedPacketData> pathPackets(&filePathPackets, RecordedPacketData::getSerializedSize());

		QFile filePathPacketCounters(expPath + QString("/capture-data/path-packets-sampled-%1.dat").arg(p));
		if (!filePathPacketCounters.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		QDataStream sPathPackets(&filePathPacketCounters);
		sPathPackets.setVersion(QDataStream::Qt_4_0);
		// Compatibility with OLazyVector
		sPathPackets << qint64(0);

		QFile filePathDrops(expPath + QString("/capture-data/path-drops-sampled-%1.dat").arg(p));
		if (!filePathDrops.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		QDataStream sPathDrops(&filePathDrops);
		sPathDrops.setVersion(QDataStream::Qt_4_0);
		// Compatibility with OLazyVector
		sPathDrops << qint64(0);

		QFile filePathDelay(expPath + QString("/capture-data/path-delay-sampled-%1.dat").arg(p));
		if (!filePathDelay.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		QDataStream sPathDelay(&filePathDelay);
		sPathDelay.setVersion(QDataStream::Qt_4_0);
		// Compatibility with OLazyVector
		sPathDelay << qint64(0);

		QVector<qreal> packets(nSamples);
		packets.fill(0.0, nSamples);

		QVector<qreal> drops(nSamples);
		drops.fill(0.0, nSamples);

		QVector<qreal> delay(nSamples);
		delay.fill(0.0, nSamples);

		quint64 t1 = errorAnalysisData.tsStart;
		qint64 i1 = errorAnalysisData.time2interval(t1);
		Q_ASSERT_FORCE(i1 >= 0);

		for (qint64 pi = 0; pi <= pathPackets.count(); pi++) {
			bool dummy = false;
			RecordedPacketData packet;
			if (pi < pathPackets.count()) {
				packet = pathPackets[pi];
				QByteArray buffer = QByteArray((const char*)packet.buffer, CAPTURE_LENGTH);
				int size = getOriginalPacketLength(buffer) + 14;
				if (size < minPacketSize)
					continue;
			} else {
				// Dummy event for the last timestamp
				packet.packet_id = ULONG_LONG_MAX;
				packet.ts_userspace_rx = errorAnalysisData.tsEnd;
				dummy = true;
			}

			quint64 t2 = packet.ts_userspace_rx;
			qint64 i2 = errorAnalysisData.time2interval(t2);
			if (i2 < 0)
				continue;
			Q_ASSERT_FORCE(i2 >= i1);
			qint64 s2 = errorAnalysisData.time2sample(t2);
			qreal p2 = !dummy ? 1.0 : 0.0;
			qreal drop2 = !dummy ? packetDropped[rec.packetIndexByID(packet.packet_id)] : 0.0;
			qreal delay2 = !dummy
						   ? (packetDropped[rec.packetIndexByID(packet.packet_id)]
							 ? 0.0
							 : packetExitTimestamp[rec.packetIndexByID(packet.packet_id)] - packet.ts_userspace_rx)
					: 0.0;
			if (!dummy)
				intervalLossiness[i2].trafficPaths.insert(p);
			if (drop2 > 0) {
				intervalLossiness[i2].lossyPaths.insert(p);
			}

			while (i1 < i2) {
				// dump samples for interval i1
				foreach (qreal sample, packets) {
					sPathPackets << sample;
				}
				packets.fill(0.0, nSamples);

				foreach (qreal sample, drops) {
					sPathDrops << sample;
				}
				drops.fill(0.0, nSamples);

				foreach (qreal sample, delay) {
					sPathDelay << sample;
				}
				delay.fill(0.0, nSamples);

				i1++;
			}
			if (!dummy) {
				recordSample(packets[s2 % nSamples], p2, recordFunctionSum);
				recordSample(drops[s2 % nSamples], drop2, recordFunctionSum);
				recordSample(delay[s2 % nSamples], delay2, recordFunctionMax);
			}
			i1 = i2;
			t1 = t2;
		}

		filePathPacketCounters.seek(0);
		sPathPackets << qint64(errorAnalysisData.numIntervals * nSamples);

		filePathDrops.seek(0);
		sPathDrops << qint64(errorAnalysisData.numIntervals * nSamples);

		filePathDelay.seek(0);
		sPathDelay << qint64(errorAnalysisData.numIntervals * nSamples);
	}

	// Plot
	qDebug() << "Per path interval plots (plots)";
	for (Path p = 0; p < g.paths.count(); p++) {
		qDebug() << "Path" << (p+1);
		QFile filePathPackets(expPath + QString("/capture-data/path-packets-sampled-%1.dat").arg(p));
		if (!filePathPackets.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<qreal> pathPackets(&filePathPackets);

		QFile filePathDrops(expPath + QString("/capture-data/path-drops-sampled-%1.dat").arg(p));
		if (!filePathDrops.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<qreal> pathDrops(&filePathDrops);

		QFile filePathDelay(expPath + QString("/capture-data/path-delay-sampled-%1.dat").arg(p));
		if (!filePathDelay.open(QIODevice::ReadOnly)) {
			qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
			return false;
		}
		OLazyVector<qreal> pathDelay(&filePathDelay);

		for (int i = 0; i < errorAnalysisData.numIntervals; i++) {
			QVector<qreal> packets(nSamples);
			packets.fill(0.0, nSamples);

			QVector<qreal> drops(nSamples);
			drops.fill(0.0, nSamples);

			QVector<qreal> delay(nSamples);
			delay.fill(0.0, nSamples);

			for (int s = 0; s < nSamples; s++) {
				packets[s] = pathPackets[i * nSamples + s];
				drops[s] = pathDrops[i * nSamples + s];
				delay[s] = pathDelay[i * nSamples + s] * 1.0e-6;
			}

			// Plot
			// Throughput & loss
			QImage image(w, h, QImage::Format_RGB32);
			image.fill(Qt::white);

			QPainter painter;
			painter.begin(&image);
			painter.setPen(errorAnalysisData.pathClass[p] == 0 ? Qt::blue : Qt::red);
			painter.setBrush(Qt::NoBrush);
			painter.drawText(textPadding, 0, textWidth - 2 * textPadding, h, Qt::AlignLeft | Qt::AlignVCenter, QString("p%1").arg(p + 1));
			for (int s = 0; s < nSamples; s++) {
				painter.setPen(Qt::black);
				QColor color = getLoadColor(packets[s] / maxPPS, drops[s] / qMax(packets[s], 1.0));
				painter.fillRect(textWidth + s * blockSize,
								 (h - blockSize) / 2,
								 blockSize,
								 blockSize, color);
				painter.drawRect(textWidth + s * blockSize,
								 (h - blockSize) / 2,
								 blockSize,
								 blockSize);
			}
			painter.end();
			image.save(QString("%1/capture-plots/i%2/p%3.png")
					   .arg(expPath)
					   .arg(i + 1)
					   .arg(p + 1));

			// Delay
			image.fill(Qt::white);

			painter.begin(&image);
			painter.setPen(errorAnalysisData.pathClass[p] == 0 ? Qt::blue : Qt::red);
			painter.setBrush(Qt::NoBrush);
			painter.drawText(textPadding, 0, textWidth - 2 * textPadding, h, Qt::AlignLeft | Qt::AlignVCenter, QString("p%1").arg(p + 1));
			for (int s = 0; s < nSamples; s++) {
				painter.setPen(Qt::black);
				QColor color = getDelayColor(delay[s], packets[s], drops[s] / qMax(packets[s], 1.0));
				painter.fillRect(textWidth + s * blockSize,
								 (h - blockSize) / 2,
								 blockSize,
								 blockSize, color);
				painter.drawRect(textWidth + s * blockSize,
								 (h - blockSize) / 2,
								 blockSize,
								 blockSize);
			}

			qreal delayMax = 0;
			for (int s = 0; s < nSamples; s++) {
				delayMax = qMax(delayMax, delay[s]);
			}
			painter.setPen(Qt::black);
			painter.setBrush(Qt::NoBrush);
			painter.drawText(textWidth + nSamples * blockSize + textPadding,
							 0,
							 textWidth - 2 * textPadding,
							 h,
							 Qt::AlignCenter | Qt::AlignVCenter,
							 QString("%1 ms").arg((int)delayMax));

			painter.end();
			image.save(QString("%1/capture-plots/i%2/pdelay%3.png")
					   .arg(expPath)
					   .arg(i + 1)
					   .arg(p + 1));
		}
	}

	// for each link
	//   for each path
	//     for each interval
	//       plot p

	qDebug() << "Per link, path interval plots (data)";
	for (Link e = 0; e < g.edges.count(); e++) {
		qDebug() << "Link" << (e+1);
		foreach (Path p, errorAnalysisData.link2paths[e]) {
			QFile fileLinkPathEvents(expPath + QString("/capture-data/link-path-events-%1-%2.dat").arg(e).arg(p));
			if (!fileLinkPathEvents.open(QIODevice::ReadOnly)) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
				return false;
			}
			OLazyVector<RecordedQueuedPacketData> linkPathEvents(&fileLinkPathEvents, RecordedQueuedPacketData::getSerializedSize());

			QFile fileLinkPathState(expPath + QString("/capture-data/link-path-state-sampled-%1-%2.dat").arg(e).arg(p));
			if (!fileLinkPathState.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
				return false;
			}
			QDataStream sLinkPathState(&fileLinkPathState);
			sLinkPathState.setVersion(QDataStream::Qt_4_0);
			// Compatibility with OLazyVector
			sLinkPathState << qint64(0);

			QFile fileLinkPathPackets(expPath + QString("/capture-data/link-path-packets-sampled-%1-%2.dat").arg(e).arg(p));
			if (!fileLinkPathPackets.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
				return false;
			}
			QDataStream sLinkPathPackets(&fileLinkPathPackets);
			sLinkPathPackets.setVersion(QDataStream::Qt_4_0);
			// Compatibility with OLazyVector
			sLinkPathPackets << qint64(0);

			QVector<qreal> pathPackets(nSamples);
			pathPackets.fill(0, nSamples);
			QVector<qreal> pathDrops(nSamples);
			pathDrops.fill(0, nSamples);

			quint64 t1 = errorAnalysisData.tsStart;
			qint64 i1 = errorAnalysisData.time2interval(t1);
			Q_ASSERT_FORCE(i1 >= 0);

			for (qint64 qei = 0; qei <= linkPathEvents.count(); qei++) {
				bool dummy = false;
				RecordedQueuedPacketData event;
				if (qei < linkPathEvents.count()) {
					event = linkPathEvents[qei];
					Q_ASSERT_FORCE(event.edge_index == e);
					if (packetSize[rec.packetIndexByID(event.packet_id)] < minPacketSize)
						continue;
				} else {
					// Dummy event for the last timestamp
					event.ts_enqueue = errorAnalysisData.tsEnd;
					event.qload = 0;
					event.qcapacity = 1;
					dummy = true;
				}

				quint64 t2 = event.ts_enqueue;
				qint64 i2 = errorAnalysisData.time2interval(t2);
				if (i2 < 0)
					continue;
				Q_ASSERT_FORCE(i2 >= i1);
				qint64 s2 = errorAnalysisData.time2sample(t2);
				qreal packets2 = !dummy ? 1 : 0;
				qreal drops2 = dummy ? 0 : (event.decision == RecordedQueuedPacketData::Queued ? 0 : 1);

				while (i1 < i2) {
					// dump samples for interval i1
					for (int rs = 0; rs < pathPackets.count(); rs++) {
						sLinkPathState << (pathPackets[rs] > 0 ? (pathPackets[rs] - pathDrops[rs]) / pathPackets[rs] : 1.0);
					}
					foreach (qreal sample, pathPackets) {
						sLinkPathPackets << sample;
					}
					pathPackets.fill(0, nSamples);
					pathDrops.fill(0, nSamples);
					i1++;
				}
				if (!dummy) {
					recordSample(pathPackets[s2 % nSamples], packets2, recordFunctionSum);
					recordSample(pathDrops[s2 % nSamples], drops2, recordFunctionSum);
				}
				i1 = i2;
				t1 = t2;
			}

			fileLinkPathState.seek(0);
			sLinkPathState << qint64(errorAnalysisData.numIntervals * nSamples);

			fileLinkPathPackets.seek(0);
			sLinkPathPackets << qint64(errorAnalysisData.numIntervals * nSamples);
		}
	}

	// Plot
	qDebug() << "Per link, path interval plots (plots)";
	for (Link e = 0; e < g.edges.count(); e++) {
		qDebug() << "Link" << (e+1);
		foreach (Path p, errorAnalysisData.link2paths[e]) {
			QFile fileLinkPathState(expPath + QString("/capture-data/link-path-state-sampled-%1-%2.dat").arg(e).arg(p));
			if (!fileLinkPathState.open(QIODevice::ReadOnly)) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
				return false;
			}
			OLazyVector<qreal> linkPathState(&fileLinkPathState);

			QFile fileLinkPathPackets(expPath + QString("/capture-data/link-path-packets-sampled-%1-%2.dat").arg(e).arg(p));
			if (!fileLinkPathPackets.open(QIODevice::ReadOnly)) {
				qWarning() << __FILE__ << __LINE__ << __FUNCTION__ << "Cannot open file";
				return false;
			}
			OLazyVector<qreal> linkPathPackets(&fileLinkPathPackets);

			for (int i = 0; i < errorAnalysisData.numIntervals; i++) {
				QVector<qreal> pathState(nSamples);
				pathState.fill(0.0, nSamples);

				QVector<qreal> pathPackets(nSamples);
				pathPackets.fill(0.0, nSamples);

				for (int s = 0; s < nSamples; s++) {
					pathState[s] = linkPathState[i * nSamples + s];
					pathPackets[s] = linkPathPackets[i * nSamples + s];
				}

				// Plot
				QImage image(w, h, QImage::Format_RGB32);
				image.fill(Qt::white);

				QPainter painter;
				painter.begin(&image);
				painter.setPen(errorAnalysisData.pathClass[p] == 0 ? Qt::blue : Qt::red);
				painter.setBrush(Qt::NoBrush);
				painter.drawText(textPadding, 0,
								 textWidth - 2 * textPadding, h,
								 Qt::AlignLeft | Qt::AlignVCenter,
								 QString("e%1 p%2").arg(e + 1).arg(p + 1));
				for (int s = 0; s < nSamples; s++) {
					painter.setPen(Qt::black);
					QColor color = getLoadColor(pathPackets[s] / maxPPS, 1.0 - pathState[s]);
					painter.fillRect(textWidth + s * blockSize,
									 (h - blockSize) / 2,
									 blockSize,
									 blockSize, color);
					painter.drawRect(textWidth + s * blockSize,
									 (h - blockSize) / 2,
									 blockSize,
									 blockSize);
				}
				painter.end();
				image.save(QString("%1/capture-plots/i%2/e%3-p%4.png")
						   .arg(expPath)
						   .arg(i + 1)
						   .arg(e + 1)
						   .arg(p + 1));
			}
		}
	}

	{
		QFile file(expPath + "/capture-data/interval-lossiness.json");
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qDebug() << __FILE__ << __LINE__ << "Failed to open file";
			return false;
		}
		QTextStream out(&file);

		out << toJson(intervalLossiness) << "\n";

		if (out.status() != QTextStream::Ok)
			return false;
	}

	QFile::copy(srcDir + "/www/error-analysis/error-analysis-captures.html",
				expPath + "/error-analysis-captures.html");
	copyDir(srcDir + "/www/error-analysis/error-analysis-www", expPath);

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

QList<Flow> Conversation::flows()
{
	return QList<Flow>() << fwdFlow << retFlow;
}

Flow::Flow(quint16 sourcePort, quint16 destPort, QString protocolString)
	: sourcePort(sourcePort),
	  destPort(destPort),
	  protocolString(protocolString)
{
}

QDataStream& operator<<(QDataStream& s, const FlowPacket& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.packetId;
	s << d.packetIndex;
	s << d.tsSent;
	s << d.received;
	s << d.dropped;
	s << d.dropEdgeId;
	s << d.tsDrop;
	s << d.tsReceived;

	return s;
}

QDataStream& operator>>(QDataStream& s, FlowPacket& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.packetId;
	s >> d.packetIndex;
	s >> d.tsSent;
	s >> d.received;
	s >> d.dropped;
	s >> d.dropEdgeId;
	s >> d.tsDrop;
	s >> d.tsReceived;

	return s;
}

qint64 FlowPacket::getSerializedSize()
{
	QByteArray buffer;
	QDataStream stream(&buffer, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_4_0);
	FlowPacket dummy;
	stream << dummy;
	return buffer.length();
}

QDataStream& operator<<(QDataStream& s, const Flow& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.sourcePort;
	s << d.destPort;
	s << d.protocolString;
	s << d.packets;

	return s;
}

QDataStream& operator>>(QDataStream& s, Flow& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.sourcePort;
	s >> d.destPort;
	s >> d.protocolString;
	s >> d.packets;

	return s;
}

QDataStream& operator<<(QDataStream& s, const Conversation& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.sourcePort;
	s << d.destPort;
	s << d.protocolString;
	s << d.fwdFlow;
	s << d.retFlow;

	return s;
}

QDataStream& operator>>(QDataStream& s, Conversation& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.sourcePort;
	s >> d.destPort;
	s >> d.protocolString;
	s >> d.fwdFlow;
	s >> d.retFlow;

	return s;
}

QDataStream& operator<<(QDataStream& s, const PathConversations& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.sourceNodeId;
	s << d.destNodeId;
	s << d.maxPossibleBandwidthFwd;
	s << d.maxPossibleBandwidthRet;
	s << d.conversations;

	return s;
}

QDataStream& operator>>(QDataStream& s, PathConversations& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.sourceNodeId;
	s >> d.destNodeId;
	s >> d.maxPossibleBandwidthFwd;
	s >> d.maxPossibleBandwidthRet;
	s >> d.conversations;

	return s;
}
