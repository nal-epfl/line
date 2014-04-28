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

#include "netgraphedge.h"

#ifdef LINE_EMULATOR
bool comparePacketEvent(const packetEvent &a, const packetEvent &b)
{
    return a.timestamp < b.timestamp;
}

bool compareEdgeTimelineItem(const edgeTimelineItem &a, const edgeTimelineItem &b)
{
    return a.timestamp < b.timestamp;
}
#endif

NetGraphEdge::NetGraphEdge()
{
	used = true;

	color = 0xFF000000;
	width = 1.0;

	recordSampledTimeline = false;
	recordFullTimeline = false;

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
	result = QString("e%1").arg(index + 1);

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
	result << QString("Edge %1").arg(index + 1);
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
