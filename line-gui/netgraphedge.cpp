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
#include "pconsumer.h"
#endif

#include "netgraphedge.h"

#include "netgraph.h"

#ifdef LINE_EMULATOR
bool comparePacketEvent(const packetEvent &a, const packetEvent &b)
{
    return a.timestamp < b.timestamp;
}
#endif

FlowIdentifier::FlowIdentifier(Packet *p)
{
    if (p) {
#ifdef LINE_EMULATOR
        ipSrc = p->src_ip;
        ipDst = p->dst_ip;
        protocol = p->l4_protocol;
        portSrc = p->l4_src_port;
        portDst = p->l4_dst_port;
#else
        ipSrc = 0;
        ipDst = 0;
        protocol = 0;
        portSrc = 0;
        portDst = 0;
#endif
    }
}

bool FlowIdentifier::operator==(const FlowIdentifier &other) const
{
    return this->ipSrc == other.ipSrc &&
            this->ipDst == other.ipDst &&
            this->protocol == other.protocol &&
            this->portSrc == other.portSrc &&
            this->portDst == other.portDst;
}

bool FlowIdentifier::operator!=(const FlowIdentifier &other) const
{
    return !(*this == other);
}

uint qHash(const FlowIdentifier& object)
{
    // Note: hash combining algorithm from boost::hash_combine
    // The magic number is supposed to be 32 random bits. A common way to find a
    // string of such bits is to use the binary expansion of an irrational number;
    // in this case, that number is the reciprocal of the golden ratio:
    // phi = (1 + sqrt(5)) / 2
    // 2^32 / phi = 0x9e3779b9
    // See also http://stackoverflow.com/questions/4948780/
    uint result = 0;
    result ^= qHash(object.ipSrc) + 0x9e3779b9 + (result << 6) + (result >> 2);;
    result ^= qHash(object.ipDst) + 0x9e3779b9 + (result << 6) + (result >> 2);;
    result ^= qHash(object.protocol) + 0x9e3779b9 + (result << 6) + (result >> 2);;
    result ^= qHash(object.portSrc) + 0x9e3779b9 + (result << 6) + (result >> 2);;
    result ^= qHash(object.portDst) + 0x9e3779b9 + (result << 6) + (result >> 2);;
    return result;
}

QDataStream& operator<<(QDataStream& s, const FlowIdentifier& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.ipSrc;
    s << d.ipDst;
    s << d.protocol;
    s << d.portSrc;
    s << d.portDst;

    return s;
}

QDataStream& operator>>(QDataStream& s, FlowIdentifier& d)
{
    qint32 ver = 0;

    s >> ver;

    s >> d.ipSrc;
    s >> d.ipDst;
    s >> d.protocol;
    s >> d.portSrc;
    s >> d.portDst;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

void EdgeTimelineItem::clear()
{
    timestamp = 0;
    arrivals_p = 0;
    arrivals_B = 0;
    qdrops_p = 0;
    qdrops_B = 0;
    rdrops_p = 0;
    rdrops_B = 0;
    queue_sampled = 0;
    queue_avg = 0;
    queue_max = 0;
    numFlows = 0;
#ifdef LINE_EMULATOR
    if (flowTracking) {
#endif
        flows.clear();
#ifdef LINE_EMULATOR
    }
#endif
}

bool compareEdgeTimelineItem(const EdgeTimelineItem &a, const EdgeTimelineItem &b)
{
    return a.timestamp < b.timestamp;
}

QDataStream& operator<<(QDataStream& s, const EdgeTimelineItem& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.timestamp;
    s << d.arrivals_p;
    s << d.arrivals_B;
    s << d.qdrops_p;
    s << d.qdrops_B;
    s << d.rdrops_p;
    s << d.rdrops_B;
    s << d.queue_sampled;
    s << d.queue_avg;
    s << d.queue_max;
    s << d.numFlows;
    s << d.flows;

    return s;
}

QDataStream& operator>>(QDataStream& s, EdgeTimelineItem& d)
{
    qint32 ver = 0;

    s >> ver;

    s >> d.timestamp;
    s >> d.arrivals_p;
    s >> d.arrivals_B;
    s >> d.qdrops_p;
    s >> d.qdrops_B;
    s >> d.rdrops_p;
    s >> d.rdrops_B;
    s >> d.queue_sampled;
    s >> d.queue_avg;
    s >> d.queue_max;
    s >> d.numFlows;
    s >> d.flows;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

EdgeTimeline::EdgeTimeline()
{
    edgeIndex = -1;
    queueIndex = -1;
    tsMin = 1;
    tsMax = 0;
    timelineSamplingPeriod = 0;
    rate_Bps = 0;
    delay_ms = 0;
    qcapacity = 0;
}

QDataStream& operator<<(QDataStream& s, const EdgeTimeline& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.edgeIndex;
    s << d.queueIndex;
    s << d.tsMin;
    s << d.tsMax;
    s << d.timelineSamplingPeriod;
    s << d.rate_Bps;
    s << d.delay_ms;
    s << d.qcapacity;
    s << d.items;

    return s;
}

QDataStream& operator>>(QDataStream& s, EdgeTimeline& d)
{
    qint32 ver = 0;

    s >> ver;

    s >> d.edgeIndex;
    s >> d.queueIndex;
    s >> d.tsMin;
    s >> d.tsMax;
    s >> d.timelineSamplingPeriod;
    s >> d.rate_Bps;
    s >> d.delay_ms;
    s >> d.qcapacity;
    s >> d.items;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

QDataStream& operator<<(QDataStream& s, const EdgeTimelines& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.timelines;

    return s;
}

QDataStream& operator>>(QDataStream& s, EdgeTimelines& d)
{
    qint32 ver = 0;

    s >> ver;

    s >> d.timelines;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

bool readEdgeTimelines(EdgeTimelines &d, NetGraph *g, QString workingDir)
{
    QFile file(QString("%1/edge-timelines.dat").arg(workingDir));
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream s(&file);
        s.setVersion(QDataStream::Qt_4_0);
        s >> d;
        return s.status() == QDataStream::Ok;
    } else {
        d = EdgeTimelines();
        // attempt to load legacy data
        for (qint32 iEdge = 0; iEdge < g->edges.count(); iEdge++) {
            d.timelines.append(QVector<EdgeTimeline>());
            QVector<EdgeTimeline> &edgeQueueTimelines = d.timelines.last();
            for (int queue = -1; queue < g->edges[iEdge].queueCount; queue++) {
                edgeQueueTimelines.append(EdgeTimeline());
                EdgeTimeline &queueTimeline = edgeQueueTimelines.last();
                queueTimeline.edgeIndex = iEdge;
                queueTimeline.queueIndex = queue;

                QString fileName = workingDir + "/" + QString("timelines-edge-%1%2.dat").
                                   arg(iEdge).
                                   arg(queue >= 0 ? QString("-queue-%1").arg(queue) : QString(""));
                QFile file(fileName);
                if (!file.open(QIODevice::ReadOnly)) {
                    continue;
                }
                QDataStream in(&file);
                in.setVersion(QDataStream::Qt_4_0);

                // edge timeline range
                in >> queueTimeline.tsMin;
                in >> queueTimeline.tsMax;

                // edge properties
                in >> queueTimeline.timelineSamplingPeriod;
                in >> queueTimeline.rate_Bps;
                in >> queueTimeline.delay_ms;
                in >> queueTimeline.qcapacity;

                QVector<quint64> vector_timestamp;
                QVector<quint64> vector_arrivals_p;
                QVector<quint64> vector_arrivals_B;
                QVector<quint64> vector_qdrops_p;
                QVector<quint64> vector_qdrops_B;
                QVector<quint64> vector_rdrops_p;
                QVector<quint64> vector_rdrops_B;
                QVector<quint64> vector_queue_sampled;
                QVector<quint64> vector_queue_avg;
                QVector<quint64> vector_queue_max;
                QVector<quint64> vector_queue_numflows;

                in >> vector_timestamp;
                in >> vector_arrivals_p;
                in >> vector_arrivals_B;
                in >> vector_qdrops_p;
                in >> vector_qdrops_B;
                in >> vector_rdrops_p;
                in >> vector_rdrops_B;
                in >> vector_queue_sampled;
                in >> vector_queue_avg;
                in >> vector_queue_max;
                in >> vector_queue_numflows;

                for (int i = 0; i < vector_timestamp.count(); i++) {
                    queueTimeline.items.append(EdgeTimelineItem());
                    EdgeTimelineItem &item = queueTimeline.items.last();
                    item.clear();
                    item.timestamp = vector_timestamp[i];
                    item.arrivals_p = vector_arrivals_p[i];
                    item.arrivals_B = vector_arrivals_B[i];
                    item.qdrops_p = vector_qdrops_p[i];
                    item.qdrops_B = vector_qdrops_B[i];
                    item.rdrops_p = vector_rdrops_p[i];
                    item.rdrops_B = vector_rdrops_B[i];
                    item.queue_sampled = vector_queue_sampled[i];
                    item.queue_avg = vector_queue_avg[i];
                    item.queue_max = vector_queue_max[i];
                    item.numFlows = vector_queue_numflows[i];
                }
            }
        }
    }

    return true;
}

NetGraphEdge::NetGraphEdge()
{
	used = true;

	color = 0xFF000000;
	width = 1.0;

	recordSampledTimeline = false;
	recordFullTimeline = false;
	timelineSamplingPeriod = 1 * 1000ULL * 1000ULL * 1000ULL;

    queueCount = 1;
	queueWeights.resize(1);
	queueWeights[0] = 1.0;

	policerCount = 1;
	policerWeights.resize(1);
	policerWeights[0] = 1.0;
}

QRgb NetGraphEdge::getColor()
{
	if (color) {
		return color;
	} else {
		if (!isNeutral()) {
			return qRgb(0xff, 0, 0);
		} else {
			return qRgb(0, 0, 0);
		}
	}
}

QString NetGraphEdge::tooltip()
{
	QString result;

	// result += QString("(%1) %2 KB/s %3 ms q=%4 B %5").arg(index).arg(bandwidth).arg(delay_ms).arg(queueLength * 1500).arg(extraTooltip);
    result = QString("%1").arg(index + 1);

	if (lossBernoulli > 1.0e-12) {
		result += " L=" + QString::number(lossBernoulli);
	}

	if (!extraTooltip.isEmpty())
		result += " " + extraTooltip;

	return result;
}

double NetGraphEdge::metric()
{
	// Link metric = cost = ref-bandwidth/bandwidth; by default ref is 10Mbps = 1250 KBps
	// See http://www.juniper.com.lv/techpubs/software/junos/junos94/swconfig-routing/modifying-the-interface-metric.html#id-11111080
	// See http://www.juniper.com.lv/techpubs/software/junos/junos94/swconfig-routing/reference-bandwidth.html#id-11227690
	const double referenceBw_KBps = 1250.0;
	return referenceBw_KBps / bandwidth;
}

bool NetGraphEdge::isNeutral()
{
	return queueCount == 1 && policerCount == 1;
}

bool NetGraphEdge::operator==(const NetGraphEdge &other) const {
	return this->index == other.index;
}

QString NetGraphEdge::toText()
{
	QStringList result;
    result << QString("Link %1").arg(index + 1);
	result << QString("Source %1").arg(source + 1);
	result << QString("Dest %1").arg(dest + 1);
	result << QString("Bandwidth-Mbps %1").arg(bandwidth * 8.0 / 1000.0);
	result << QString("Propagation-delay-ms %1").arg(delay_ms);
	result << QString("Queue-length-frames %1").arg(queueLength);
	result << QString("Bernoulli-loss %1").arg(lossBernoulli);
	result << QString("Queues %1").arg(queueCount);
	QString tmp;
	tmp.clear();
	foreach (qreal w, queueWeights) {
		tmp += QString(" %1").arg(w);
	}
	result << QString("Queue-weigths %1").arg(tmp);
	tmp.clear();
	foreach (qreal w, policerWeights) {
		tmp += QString(" %1").arg(w);
	}
	result << QString("Policers %1").arg(policerCount);
	result << QString("Policer-weigths %1").arg(tmp);
	return result.join("\n");
}

QDataStream& operator<<(QDataStream& s, const NetGraphEdge& e)
{
	qint32 ver = 4;

	if (!unversionedStreams) {
		s << ver;
	} else {
		ver = 0;
	}

	s << e.index;
	s << e.source;
	s << e.dest;

	s << e.delay_ms;
	s << e.lossBernoulli;
	s << e.queueLength;
	s << e.bandwidth;

	s << e.used;
	s << e.recordSampledTimeline;
	s << e.timelineSamplingPeriod;
	s << e.recordFullTimeline;

	if (ver >= 1) {
		s << e.color;
		s << e.width;
		s << e.extraTooltip;
	}

    if (ver >= 2) {
        s << e.queueCount;
    }

	if (ver >= 3) {
		s << e.policerCount;
	}

	if (ver >= 4) {
		s << e.policerWeights;
		s << e.queueWeights;
	}

	return s;
}

QDataStream& operator>>(QDataStream& s, NetGraphEdge& e)
{
	qint32 ver = 0;

	if (!unversionedStreams) {
		s >> ver;
	}

	s >> e.index;
	s >> e.source;
	s >> e.dest;

	s >> e.delay_ms;
	s >> e.lossBernoulli;
	s >> e.queueLength;
	s >> e.bandwidth;

	s >> e.used;
	s >> e.recordSampledTimeline;
	s >> e.timelineSamplingPeriod;
	s >> e.recordFullTimeline;

	if (ver >= 1) {
		s >> e.color;
		e.color = 0;
		s >> e.width;
		s >> e.extraTooltip;
	}

    if (ver >= 2) {
        s >> e.queueCount;
    } else {
        e.queueCount = 1;
    }

	if (ver >= 3) {
		s >> e.policerCount;
	} else {
		e.policerCount = e.queueCount;
	}

	if (ver >= 4) {
		s >> e.policerWeights;
		s >> e.queueWeights;
	} else {
		e.policerWeights.clear();
		e.queueWeights.clear();
	}

	if (e.policerWeights.isEmpty()) {
		e.policerWeights.resize(e.policerCount);
		for (int i = 0; i < e.policerCount; i++) {
			e.policerWeights[i] = 1.0 / e.policerCount;
		}
	}
	if (e.queueWeights.isEmpty()) {
		e.queueWeights.resize(e.queueCount);
		for (int i = 0; i < e.queueCount; i++) {
			e.queueWeights[i] = 1.0 / e.queueCount;
		}
	}

	e.delay_ms = qMax(1, e.delay_ms);
	e.lossBernoulli = qMin(1.0, qMax(0.0, e.lossBernoulli));
	e.queueLength = qMax(1, e.queueLength);
	e.bandwidth = qMax(0.0, e.bandwidth);

	return s;
}

QDebug &operator<<(QDebug &stream, const NetGraphEdge &e)
{
    stream << QString("%1 -> %2").arg(e.source).arg(e.dest); return stream.maybeSpace();
}
