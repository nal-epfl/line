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

#ifndef NETGRAPHPATH_H
#define NETGRAPHPATH_H

#include "netgraphnode.h"
#include "netgraphedge.h"

#include "netgraphcommon.h"

#ifdef LINE_EMULATOR
#include "../util/ovector.h"
#endif

class NetGraph;

#ifdef LINE_EMULATOR
class pathTimelineItem {
public:
	void clear();
    quint64      timestamp;
    quint64      arrivals_p;    /* number of ingress packets */
    quint64      arrivals_B;    /* ingress bytes */
    quint64      exits_p;       /* number of egress packets */
    quint64      exits_B;       /* egress bytes */
    quint64      drops_p;       /* number of packets dropped */
    quint64      drops_B;       /* bytes dropped */
    quint64      delay_total;   /* total packet delay, divide by exits_p to get average (if that is zero, this is zero) */
    quint64      delay_max;     /* max packet delay */
    quint64      delay_min;     /* min packet delay (default value: ULLONG_MAX) */
};
#endif

// A path between a source and a destination node in the graph.
// Due to load balancing, this may not be a list of edges, but a
// subgraph. EdgeList is populated if there are no load balancing
// effects.
// Theoretical path statistics (delay, loss, bandwidth)
class NetGraphPath
{
public:
    NetGraphPath();
    NetGraphPath(NetGraph &g, int source, int dest);

    QSet<NetGraphEdge> edgeSet;
    QList<NetGraphEdge> edgeList;
    qint32 source;
    qint32 dest;

	bool recordSampledTimeline;
    quint64 timelineSamplingPeriod; // nanoseconds
    bool retraceFailed;

	// don't expect this to be updated
	QSet<qint32> connections;

#ifdef LINE_EMULATOR
    // Statistics
    // Total number of packets that arrived on this path.
    quint64 packets_in;
    // Total number of packets that exit the path successfully.
    // Note: y in tomodata is computed as packets_out / (qreal)packets_in.
    quint64 packets_out;
    // Total number of bytes that arrived on this path
    quint64 bytes_in;
    // Total number of bytes that exit this path
    quint64 bytes_out;
    // Total packet delay - theoretical
    quint64 total_theor_delay;
    // Total packet delay - includes emulator overhead
    quint64 total_actual_delay;

    // Timeline
	OVector<pathTimelineItem> timelineSampled;
#endif

    bool retrace(NetGraph &g);
    QString toString();

	bool traceValid();

    // stats only work if there is no load balancing!
    // in milliseconds
    double computeFwdDelay(int frameSize);
    // returns the bandwidth of the bottleneck in kilobits/sec
    double bandwidth();
    double lossBernoulli();

#ifdef LINE_EMULATOR
    void prepareEmulation();
#endif

	QString toText();
};

QDataStream& operator>>(QDataStream& s, NetGraphPath& p);

QDataStream& operator<<(QDataStream& s, const NetGraphPath& p);

#endif // NETGRAPHPATH_H
