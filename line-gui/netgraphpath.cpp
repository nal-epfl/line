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

#include "netgraphpath.h"
#include "netgraph.h"
#include "json.h"
#include "compresseddevice.h"

NetGraphPath::NetGraphPath()
{
    recordSampledTimeline = false;
    retraceFailed = false;
}

NetGraphPath::NetGraphPath(NetGraph &g, int source, int dest) :
    source(source), dest(dest)
{
    recordSampledTimeline = false;
    retraceFailed = !retrace(g);
}

bool NetGraphPath::traceValid()
{
	if (edgeList.isEmpty())
		return false;
	if (edgeList.first().source != source)
		return false;
	if (edgeList.last().dest != dest)
		return false;
	for (int i = 0; i < edgeList.count() - 1; i++) {
		if (edgeList.at(i).dest != edgeList.at(i+1).source)
			return false;
	}
	return true;
}

/* This fails if there is no route from source to dest. Typical causes:
   - the routes have not been computed;
   - the graph was modified after the routes have been computed;
   - the routes have been computed but the topology is not strongly connected;
   - routing bug, corrupted data etc.
 */
bool NetGraphPath::retrace(NetGraph &g)
{
    // compute edge set
    QList<int> nodeQueue;
    QSet<int> nodesEnqueued;
    nodeQueue << source;
    nodesEnqueued << source;
    bool loadBalanced = false;
    bool routeFailed = false;
    while (!nodeQueue.isEmpty()) {
        int n = nodeQueue.takeFirst();
        QList<int> nextHops = g.getNextHop(n, dest);
        if (nextHops.isEmpty()) {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << QString("Failed: no route from %1 to %2, source = %3").arg(n).arg(dest).arg(source) << edgeList;
            routeFailed = true;
			//Q_ASSERT_FORCE(false);
			break;
        }
        loadBalanced = loadBalanced || (nextHops.count() > 1);
        foreach (int h, nextHops) {
            NetGraphEdge e = g.edgeByNodeIndex(n, h);
            edgeSet.insert(e);
            if (!loadBalanced) {
                edgeList << e;
            }
            if (h != dest && !nodesEnqueued.contains(h)) {
                nodeQueue << h;
                nodesEnqueued << h;
            }
        }
    }
    if (loadBalanced) {
        edgeList.clear();
    }
    if (routeFailed) {
        edgeSet.clear();
        edgeList.clear();
        retraceFailed = true;
        return !retraceFailed;
    }
    retraceFailed = false;
    return !retraceFailed;
}

QString NetGraphPath::toString()
{
    QString result;

    foreach (NetGraphEdge e, edgeList) {
        result += QString::number(e.source) + " -> " + QString::number(e.dest) + " (" + e.tooltip() + ") ";
    }
    result += "Bandwidth: " + QString::number(bandwidth()) + " KB/s ";
    result += "Delay for 500B frames: " + QString::number(computeFwdDelay(500)) + " ms ";
    result += "Loss (Bernoulli): " + QString::number(lossBernoulli() * 100.0) + "% ";
    return result;
}

double NetGraphPath::computeFwdDelay(int frameSize)
{
    double result = 0;
    foreach (NetGraphEdge e, edgeList) {
        result += e.delay_ms + frameSize/e.bandwidth;
    }
    return result;
}

double NetGraphPath::bandwidth()
{
    double result = 1.0e99;
    foreach (NetGraphEdge e, edgeList) {
        if (e.bandwidth < result)
            result = e.bandwidth;
    }
    return result;
}

double NetGraphPath::lossBernoulli()
{
    double success = 1.0;
    foreach (NetGraphEdge e, edgeList) {
        success *= 1.0 - e.lossBernoulli;
    }
    return 1.0 - success;
}

QString NetGraphPath::toText()
{
	QStringList result;
	result << QString("Path %1 %2").arg(source + 1).arg(dest + 1);
	QString tmp;
	tmp.clear();
	foreach (NetGraphEdge e, edgeList) {
		tmp += QString(" %1").arg(e.index + 1);
	}
    result << QString("Link-list %1").arg(tmp);
	return result.join("\n");
}

QDataStream& operator<<(QDataStream& s, const NetGraphPath& p)
{
	qint32 ver = 1;

	if (!unversionedStreams) {
		s << ver;
	} else {
		ver = 0;
	}

	s << p.edgeSet;
	s << p.edgeList;
	s << p.source;
	s << p.dest;
	s << p.recordSampledTimeline;
	s << p.timelineSamplingPeriod;
	return s;
}

QDataStream& operator>>(QDataStream& s, NetGraphPath& p)
{
	qint32 ver = 0;

	if (!unversionedStreams) {
		s >> ver;
	}

    s >> p.edgeSet;
    s >> p.edgeList;
    s >> p.source;
    s >> p.dest;
    s >> p.recordSampledTimeline;
    s >> p.timelineSamplingPeriod;
    return s;
}

QString toJson(const NetGraphPath &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.edgeSet);
	jsonObjectPrinterAddMember(p, d.edgeList);
	jsonObjectPrinterAddMember(p, d.source);
	jsonObjectPrinterAddMember(p, d.dest);
	jsonObjectPrinterAddMember(p, d.recordSampledTimeline);
	jsonObjectPrinterAddMember(p, d.timelineSamplingPeriod);
	jsonObjectPrinterAddMember(p, d.retraceFailed);
	jsonObjectPrinterAddMember(p, d.connections);
	return p.json();
}

void PathTimelineItem::clear()
{
	timestamp = 0;
	arrivals_p = 0;
	arrivals_B = 0;
	exits_p = 0;
	exits_B = 0;
	drops_p = 0;
	drops_B = 0;
	delay_total = 0;
	delay_max = 0;
	delay_min = 0;
}

bool comparePathTimelineItem(const PathTimelineItem &a, const PathTimelineItem &b)
{
	return a.timestamp < b.timestamp;
}

QDataStream& operator<<(QDataStream& s, const PathTimelineItem& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.timestamp;
    s << d.arrivals_p;
    s << d.arrivals_B;
    s << d.exits_p;
    s << d.exits_B;
    s << d.drops_p;
    s << d.drops_B;
    s << d.delay_total;
    s << d.delay_max;
    s << d.delay_min;

    return s;
}

QDataStream& operator>>(QDataStream& s, PathTimelineItem& d)
{
    qint32 ver = 0;

    s >> ver;

	s >> d.timestamp;
    s >> d.arrivals_p;
    s >> d.arrivals_B;
    s >> d.exits_p;
    s >> d.exits_B;
    s >> d.drops_p;
    s >> d.drops_B;
    s >> d.delay_total;
    s >> d.delay_max;
    s >> d.delay_min;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

PathTimeline::PathTimeline()
{
    pathIndex = -1;
    tsMin = 1;
    tsMax = 0;
    timelineSamplingPeriod = 0;
}

QDataStream& operator<<(QDataStream& s, const PathTimeline& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.pathIndex;
    s << d.tsMin;
    s << d.tsMax;
    s << d.timelineSamplingPeriod;
    s << d.items;

    return s;
}

QDataStream& operator>>(QDataStream& s, PathTimeline& d)
{
    qint32 ver = 0;

    s >> ver;

    s >> d.pathIndex;
    s >> d.tsMin;
    s >> d.tsMax;
    s >> d.timelineSamplingPeriod;
    s >> d.items;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

QDataStream& operator<<(QDataStream& s, const PathTimelines& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.timelines;

    return s;
}

QDataStream& operator>>(QDataStream& s, PathTimelines& d)
{
    qint32 ver = 0;

    s >> ver;

    s >> d.timelines;

    Q_ASSERT_FORCE(ver <= 1);

    return s;
}

bool readPathTimelines(PathTimelines &d, NetGraph *, QString workingDir)
{
    QFile file(QString("%1/path-timelines.dat").arg(workingDir));
	if (file.open(QIODevice::ReadOnly)) {
		CompressedDevice device(&file);
		device.open(QIODevice::ReadOnly);
		QDataStream s(&device);
		s.setVersion(QDataStream::Qt_4_0);
		s >> d;
		return s.status() == QDataStream::Ok;
	}

    return false;
}
