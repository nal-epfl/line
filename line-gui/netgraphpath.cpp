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


