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

#include "netgraph.h"

#include <QtXml>
#include "util.h"
#include "../util/chronometer.h"

#ifndef LINE_EMULATOR
#include "bgp.h"
#include "gephilayoutnetgraph.h"
#include "convexhull.h"
#endif

int unversionedStreams = 0;

NetGraph::NetGraph()
{
	hasGUI = true;
	viewportZoom = 1.0;
	viewportCenter = QPointF(0, 0);
	rootId = -1;
}

NetGraph::~NetGraph()
{
	hasGUI = true;
}

void NetGraph::clear()
{
	nodes.clear();
	edges.clear();
	connections.clear();
	paths.clear();
	domains.clear();
	fileName.clear();
}

int NetGraph::addNode(int type, QPointF pos, int ASNumber, QString customLabel)
{
	NetGraphNode node;
	node.index = nodes.count();
	node.x = pos.x();
	node.y = pos.y();
	node.nodeType = type;
	node.bgColor = NetGraphNode::colorFromType(node.nodeType);
	node.ASNumber = ASNumber;
	node.customLabel = customLabel;
	nodes.append(node);
	paths.clear();
	while (node.ASNumber >= domains.count()) {
		domains << NetGraphAS(domains.count(), domains.count());
	}
	return node.index;
}

int NetGraph::addEdge(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength, bool force)
{
	if (!canAddEdge(nodeStart, nodeEnd)) {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "illegal operation";
        Q_ASSERT_FORCE(!force);
	}
	NetGraphEdge edge;
	edge.index = edges.count();
	edge.source = nodeStart;
	edge.dest = nodeEnd;
	edge.bandwidth = bandwidth;
	edge.delay_ms = delay;
	edge.lossBernoulli = loss;
	edge.queueLength = queueLength;
	edges.append(edge);
	paths.clear();
	return edge.index;
}

int NetGraph::addEdgeSym(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength, bool force)
{
	addEdge(nodeStart, nodeEnd, bandwidth, delay, loss, queueLength, force);
	return addEdge(nodeEnd, nodeStart, bandwidth, delay, loss, queueLength, force);
}

bool NetGraph::canAddEdge(int nodeStart, int nodeEnd)
{
	// no edge to self
	if (nodeStart == nodeEnd)
		return false;
	// no duplicates
	foreach (NetGraphEdge edge, edges) {
		if (edge.source == nodeStart && edge.dest == nodeEnd)
			return false;
	}
	// no edges between hosts
	if (nodes[nodeStart].nodeType == NETGRAPH_NODE_HOST &&
			nodes[nodeEnd].nodeType == NETGRAPH_NODE_HOST)
		return false;
#if 0
	// no edges between hosts and routers
	if (nodes[nodeStart].nodeType == NETGRAPH_NODE_HOST &&
			nodes[nodeEnd].nodeType == NETGRAPH_NODE_ROUTER)
		return false;
	if (nodes[nodeEnd].nodeType == NETGRAPH_NODE_HOST &&
			nodes[nodeStart].nodeType == NETGRAPH_NODE_ROUTER)
		return false;
	// no edges between hosts and border routers
	if (nodes[nodeStart].nodeType == NETGRAPH_NODE_HOST &&
			nodes[nodeEnd].nodeType == NETGRAPH_NODE_BORDER)
		return false;
	if (nodes[nodeEnd].nodeType == NETGRAPH_NODE_HOST &&
			nodes[nodeStart].nodeType == NETGRAPH_NODE_BORDER)
		return false;
#endif
	// if edge is inter-AS, then the nodes must be border routers
	if (nodes[nodeStart].ASNumber != nodes[nodeEnd].ASNumber) {
		if (nodes[nodeStart].nodeType != NETGRAPH_NODE_BORDER ||
				nodes[nodeEnd].nodeType != NETGRAPH_NODE_BORDER)
			return false;
	}
	return true;
}

void NetGraph::deleteNode(int index)
{
	nodes.removeAt(index);
	for (int i = 0; i < nodes.length(); i++) {
		nodes[i].index = i;
	}

	QList<int> toRemove;
	for (int iEdge = 0; iEdge < edges.length(); iEdge++) {
		if (edges[iEdge].source == index || edges[iEdge].dest == index) {
			toRemove << iEdge;
			continue;
		}

		if (edges[iEdge].source > index)
			edges[iEdge].source--;
		if (edges[iEdge].dest > index)
			edges[iEdge].dest--;
	}
	qSort(toRemove);
	while (!toRemove.isEmpty()) {
		deleteEdge(toRemove.takeLast());
	}

	for (int iConnection = 0; iConnection < connections.length(); iConnection++) {
		if (connections[iConnection].source == index || connections[iConnection].dest == index) {
			deleteConnection(iConnection);
			iConnection--;
			continue;
		}

		if (connections[iConnection].source > index)
			connections[iConnection].source--;
		if (connections[iConnection].dest > index)
			connections[iConnection].dest--;
	}

	checkIndices();

	clearRoutes();

#ifndef LINE_EMULATOR
    computeASHulls();
#endif
}

void NetGraph::deleteEdge(int index)
{
	edges.removeAt(index);
	for (int i = 0; i < edges.length(); i++) {
		edges[i].index = i;
	}
	clearRoutes();
}

void NetGraph::deleteConnection(int index)
{
	connections.removeAt(index);
	for (int i = 0; i < connections.length(); i++) {
		connections[i].index = i;
	}
}

void NetGraph::clearRoutes()
{
	for (int n = 0; n < nodes.count(); n++)
		nodes[n].routes = RoutingTable();
	paths.clear();
	for (int e = 0; e < edges.count(); e++) {
		edges[e].used = false;
	}
	for (int n = 0; n < nodes.count(); n++) {
		nodes[n].used = false;
	}
	for (int asi = 0; asi < domains.count(); asi++) {
		domains[asi].used = false;
	}
}

void NetGraph::checkIndices()
{
	for (int e = 0; e < edges.count(); e++) {
		Q_ASSERT_FORCE(edges[e].index == e);
		Q_ASSERT_FORCE(edges[e].source < nodes.count());
		Q_ASSERT_FORCE(edges[e].dest < nodes.count());
	}
}


int NetGraph::addConnection(NetGraphConnection c)
{
	c.index = connections.count();
	connections << c;
	//updateUsed();
	return c.index;
}

#ifndef LINE_EMULATOR
bool NetGraph::computeRoutes()
{
	paths.clear();
	Bgp::computeRoutes(*this);
	return updateUsed();
}

bool NetGraph::computeFullRoutes()
{
	paths.clear();

	int cc = connections.count();

	QList<NetGraphNode> hostNodes = getHostNodes();
	for (int i = 0; i < hostNodes.count(); i++) {
		for (int j = i + 1; j < hostNodes.count(); j++) {
			addConnection(NetGraphConnection(hostNodes[i].index, hostNodes[j].index, "dummy", ""));
		}
	}

	Bgp::computeRoutes(*this);

	bool ok = updateUsed();
	connections = connections.mid(0, cc);
	return ok;
}

bool NetGraph::computeRoutesFromPaths()
{
	foreach (NetGraphPath p, paths) {
		if (p.edgeList.isEmpty())
			return false;
	}
	foreach (NetGraphNode n, nodes) {
		nodes[n.index].routes = RoutingTable();
	}
	foreach (NetGraphPath p, paths) {
		foreach (NetGraphEdge e, p.edgeList) {
			// so we reach p.dest from e.source via e.dest
			nodes[e.source].routes.localRoutes.insert(p.dest, Route(p.dest, e.dest));
		}
	}
	return true;
}
#endif

void NetGraph::setFileName(QString newFileName)
{
	this->fileName = newFileName;
}

QDataStream& operator<<(QDataStream& s, const NetGraph& n)
{
	qint32 ver = 1;

	if (!unversionedStreams) {
		s << ver;
	} else {
		ver = 0;
	}

	s << n.nodes;
	s << n.edges;
	s << n.connections;
	s << n.domains;
	s << n.paths;

	if (ver >= 1) {
		s << n.rootId;
		s << n.viewportCenter;
		s << n.viewportZoom;
	}

	return s;
}

QDataStream& operator>>(QDataStream& s, NetGraph& n)
{
	qint32 ver = 0;

	if (!unversionedStreams) {
		s >> ver;
	}

	s >> n.nodes;
	s >> n.edges;
	s >> n.connections;
	s >> n.domains;
	s >> n.paths;

	if (ver >= 1) {
		s >> n.rootId;
		s >> n.viewportCenter;
		s >> n.viewportZoom;
	}
	return s;
}

bool NetGraph::saveToFile()
{
	QFile file(fileName);
	qDebug() << "Saving graph:" << fileName;
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << *this;

	return true;
}

bool NetGraph::loadFromFile()
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;
	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Failed to read graph from file:" << fileName;
		return false;
	}

	checkIndices();

	return true;
}

QByteArray NetGraph::saveToByteArray()
{
	QByteArray data;
	QDataStream out(&data, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_0);
	out << *this;
	return data;
}

bool NetGraph::loadFromByteArray(QByteArray b)
{
	if (b.isEmpty()) {
		return false;
	}

	QDataStream in(b);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;
	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Could not load graph from QByteArray of length" << b.count();
		return false;
	}

	return true;
}

bool NetGraph::pathsValid()
{
	foreach (NetGraphPath p, paths) {
		if (!p.traceValid()) {
			return false;
		}
	}
	return true;
}

#ifndef LINE_EMULATOR
qreal NetGraph::pathMaximumBandwidth(qint32 source, qint32 dest)
{
	bool ok;
	NetGraphPath p = pathByNodeIndexTry(source, dest, ok);
	if (!ok) {
		return 0;
	}
	qreal result = -1;
	foreach (NetGraphEdge edge, p.edgeSet) {
		if (result < 0 || result > edge.bandwidth) {
			result = edge.bandwidth;
		}
	}
	if (result < 0)
		result = 0;
	return result;
}

bool NetGraph::readjustQueues()
{
	// The 1-RTT of traffic rule [1]:
	// Each queue should hold the minimum of:
	// * 10 full-sized frames
	// * 1 end-to-end RTT of traffic (without queuing delay)
	// * 1 end-to-end RTT of traffic + 2ms delay per hop
	// We compute these as network diameters, not per-link
	// Note: routes must be available
	// [1] C. Villamizar and C. Song, High performance TCP in ansnet, ACM Computer Communications Review, 1994.
	qreal rttDiameter = 0.0;
	qreal rttQueuingDiameter = 0.0;
	const qreal frameSize = 1500.0;
	const qreal perHopQueuingDelay = 2.0;
	foreach (NetGraphPath p, paths) {
		qreal rtt = 0.0;
		qreal rttQueuing = 0.0;
		foreach (NetGraphEdge e, p.edgeList) {
			qreal delay = e.delay_ms + frameSize / e.bandwidth;
			rtt += delay;
			rttQueuing += delay + perHopQueuingDelay;
		}
		rttDiameter = qMax(rttDiameter, rtt);
		rttQueuingDiameter = qMax(rttQueuingDiameter, rtt);
	}

	qreal end2endRtt = 2.0 * qMax(rttDiameter, rttQueuingDiameter);

	foreach (NetGraphEdge e, edges) {
		edges[e.index].queueLength = qMax((int)MIN_QUEUE_LEN_FRAMES,
										  (int)ceil(e.bandwidth * end2endRtt / frameSize));
	}

	return end2endRtt > 0;
}

bool NetGraph::readjustQueuesSmall()
{
	// The 1-RTT/sqrt(#flows) rule [2]:
	// Each queue should hold the minimum of:
	// * 10 full-sized frames
	// * 1 end-to-end RTT of traffic (without queuing delay) / sqrt(#flows per queue)
	// * 1 end-to-end RTT of traffic + 2ms delay per hop / sqrt(#flows per queue)
	// We compute these as network diameters, not per-link
	// Note: routes and connections must be available
	// [2] G. Appenzeller, I. Keslassy, N. McKeown, Sizing router buffers, ACM SIGCOMM 2004.

	if (!readjustQueues())
		return false;

	QVector<int> numFlowsPerLink;
	numFlowsPerLink.resize(edges.size());

	bool allFine = true;
	foreach (NetGraphConnection c, connections) {
		int multiplier = c.multiplier;
		bool ok;

		NetGraphPath pFwd = pathByNodeIndexTry(c.source, c.dest, ok);
		if (ok) {
			foreach (NetGraphEdge e, pFwd.edgeList) {
				numFlowsPerLink[e.index] += multiplier;
			}
		} else {
			allFine = false;
		}

		NetGraphPath pRev = pathByNodeIndexTry(c.dest, c.source, ok);
		if (ok) {
			foreach (NetGraphEdge e, pRev.edgeList) {
				numFlowsPerLink[e.index] += multiplier;
			}
		} else {
			allFine = false;
		}
	}

	foreach (NetGraphEdge e, edges) {
		edges[e.index].queueLength = qMax((int)MIN_QUEUE_LEN_FRAMES,
										  (int)ceil(e.queueLength / sqrt(qMax(1.0, (qreal)numFlowsPerLink[e.index]))));
	}

	return allFine;
}
#endif

void NetGraph::recordTimelinesEverywhere(quint64 interval)
{
	foreach (NetGraphEdge e, edges) {
		edges[e.index].recordSampledTimeline = true;
		edges[e.index].timelineSamplingPeriod = interval;
	}
}

#ifndef LINE_EMULATOR
NetGraphPath NetGraph::pathByNodeIndex(int src, int dst)
{
	foreach (NetGraphPath p, paths) {
		if (p.source == src && p.dest == dst)
			return p;
	}
	fprintf(stderr, "%s %d: %s(%d, %d)\n", __FILE__, __LINE__, __FUNCTION__, src, dst);
	printBacktrace();
	qDebug() << __FILE__ << __LINE__;
	exit(EXIT_FAILURE);
	return NetGraphPath(*this, -1, -1);
}
NetGraphPath NetGraph::pathByNodeIndexTry(int src, int dst, bool &ok)
{
	ok = true;
	foreach (NetGraphPath p, paths) {
		if (p.source == src && p.dest == dst)
			return p;
	}
	ok = false;
	return NetGraphPath();
}
#else
NetGraphPath& NetGraph::pathByNodeIndex(int src, int dst)
{
	return paths[pathCache[QPair<qint32,qint32>(src,dst)]];
}
#endif

#ifndef LINE_EMULATOR
NetGraphEdge NetGraph::edgeByNodeIndex(int src, int dst)
{
	foreach (NetGraphEdge e, edges) {
		if (e.source == src && e.dest == dst)
			return e;
	}
	fprintf(stderr, "%s %d: %s(%d, %d)\n", __FILE__, __LINE__, __FUNCTION__, src, dst);
	printBacktrace();
	qDebug() << __FILE__ << __LINE__;
	exit(EXIT_FAILURE);
	return NetGraphEdge();
}
#else
NetGraphEdge& NetGraph::edgeByNodeIndex(int src, int dst)
{
	return edges[edgeCache[QPair<qint32,qint32>(src,dst)]];
}
#endif

int NetGraph::optimalQueueLength(double bandwidth, int delay)
{
	const double frameSize = 1500;
	const double networkDiameter = 6; // hops
	double propDelay = delay;
	double transDelay = frameSize / bandwidth;
	double rtt = 2 * propDelay + 2 * transDelay;

	rtt *= networkDiameter;

	double trafficRTT = bandwidth * rtt;
	double qSize = ceil(trafficRTT / frameSize);
    qSize = qMax(double(MIN_QUEUE_LEN_FRAMES), qSize);

    return (int)qSize;
}

#ifndef LINE_EMULATOR
#ifndef LINE_CLIENT
bool NetGraph::layoutGephi(ProgressCallback *progressCallback)
{
//	if (!GephiLayout::layout(*this, false))
//		return false;
//	if (!GephiLayout::oneLevelLayout(*this))
//		return false;
	// first try a tree layout
	if (GephiLayout::treeLayout(this))
		return true;
	if (!GephiLayout::twoLevelLayout(this, progressCallback))
		return false;

	return true;
}

bool NetGraph::layoutGephiUsed()
{
	if (!GephiLayout::layout(this, true))
		return false;

	return true;
}
#endif
#endif

bool NetGraph::updateUsed()
{
	QSet<QPair<int, int> > connectionsCache;
	QSet<QPair<int, int> > pathsCache;
	foreach (NetGraphConnection c, connections) {
		connectionsCache.insert(QPair<int, int>(c.source, c.dest));
	}
	bool retraceFailed = false;

	// Remove unused paths, retrace existing paths
	for (int iPath = 0; iPath < paths.count(); iPath++) {
		bool used = false;
		NetGraphPath &p = paths[iPath];
		used = (p.source != p.dest) &&
				(connectionsCache.contains(QPair<int, int>(p.source, p.dest)) ||
				 connectionsCache.contains(QPair<int, int>(p.dest, p.source)));
		if (!used) {
			paths.removeAt(iPath);
			iPath--;
		} else {
			retraceFailed = retraceFailed || !p.retrace(*this);
			pathsCache.insert(QPair<int, int>(p.source, p.dest));
		}
	}

	// Add new paths
	foreach (NetGraphConnection c, connections) {
		// forward
		bool found;
		found = pathsCache.contains(QPair<int, int>(c.source, c.dest));
		if (!found && (c.source != c.dest)) {
			paths << NetGraphPath(*this, c.source, c.dest);
			retraceFailed = retraceFailed || paths.last().retraceFailed;
			pathsCache.insert(QPair<int, int>(c.source, c.dest));
		}
		// reverse
		found = pathsCache.contains(QPair<int, int>(c.dest, c.source));
		if (!found && (c.source != c.dest)) {
			paths << NetGraphPath(*this, c.dest, c.source);
			retraceFailed = retraceFailed || paths.last().retraceFailed;
			pathsCache.insert(QPair<int, int>(c.dest, c.source));
		}
	}

	// Mark edges & nodes as used/not used
	foreach (NetGraphEdge e, edges) {
		edges[e.index].used = false;
	}
	foreach (NetGraphNode n, nodes) {
		nodes[n.index].used = false;
	}
	foreach (NetGraphPath p, paths) {
		foreach (NetGraphEdge pe, p.edgeSet) {
			edges[pe.index].used = true;
			nodes[pe.source].used = true;
			nodes[pe.dest].used = true;
		}
	}
	foreach (NetGraphAS as, domains) {
		domains[as.index].used = false;
	}
	foreach (NetGraphNode n, nodes) {
		if (n.used) {
			domains[n.ASNumber].used = true;
		}
	}

	return !retraceFailed;
}

#ifndef LINE_EMULATOR
void NetGraph::computeASHulls()
{
	// AS -> set of nodes
	QHash<int, QSet<int> > asNodes;

	foreach (NetGraphNode n, nodes) {
		asNodes[n.ASNumber] << n.index;
	}
	QList<int> whatever1;
	QSet<int> whatever2;
	QList<QSet<int> > whatever3;
	domains.clear();
	int maxAS = 0;
	foreach (NetGraphNode n, nodes) {
		maxAS = qMax(maxAS, n.ASNumber);
	}
	for (int AS = domains.count(); AS <= maxAS; AS++) {
		domains << NetGraphAS(AS, AS);
	}

	foreach (int AS, asNodes.uniqueKeys()) {
		QList<QPointF> points;
		foreach (int n, asNodes[AS]) {
			points << QPointF(nodes[n].x, nodes[n].y);
		}
		QList<QPointF> hull;
		if (points.count() >= 4) {
			hull = ConvexHull::giftWrap(points);
		} else if (!points.isEmpty()) {
			// bounding rect + border
			qreal x1, x2, y1, y2;
			x1 = x2 = points.first().x();
			y1 = y2 = points.first().y();
			foreach (QPointF p, points) {
				x1 = qMin(x1, p.x());
				x2 = qMax(x2, p.x());
				y1 = qMin(y1, p.y());
				y2 = qMax(y2, p.y());
			}
			x1 -= 5 * NETGRAPH_NODE_RADIUS;
			x2 += 5 * NETGRAPH_NODE_RADIUS;
			y1 -= 5 * NETGRAPH_NODE_RADIUS;
			y2 += 5 * NETGRAPH_NODE_RADIUS;
			hull << QPointF(x1, y1) << QPointF(x2, y1) << QPointF(x2, y2) << QPointF(x1, y2);
		}
		domains[AS] = NetGraphAS(AS, AS, hull);
	}
}

void NetGraph::computeASHulls(int ASNumber)
{
	if (ASNumber < 0) {
		computeASHulls();
		return;
	}

	// AS -> set of nodes
	QHash<int, QSet<int> > asNodes;

	foreach (NetGraphNode n, nodes) {
		if (n.ASNumber == ASNumber)
			asNodes[n.ASNumber] << n.index;
	}

	int ASIndex = -1;
	foreach (NetGraphAS d, domains) {
		if (d.ASNumber == ASNumber) {
			ASIndex = d.index;
			break;
		}
	}
	if (ASIndex < 0) {
		// this domain has not been seen previously, see if it is a new one
		if (asNodes.contains(ASNumber) && !asNodes.value(ASNumber, QSet<int>()).isEmpty()) {
			// we have a new AS
			for (int AS = domains.count(); AS <= ASNumber; AS++) {
				domains << NetGraphAS(AS, AS);
			}
			ASIndex = ASNumber;
		} else {
			// there is no node within the specified AS
			return;
		}
	}

	domains[ASIndex].hull.clear();

	foreach (int AS, asNodes.uniqueKeys()) {
		QList<QPointF> points;
		foreach (int n, asNodes[AS]) {
			points << QPointF(nodes[n].x, nodes[n].y);
		}
		QList<QPointF> hull;
		if (points.count() >= 4) {
			hull = ConvexHull::giftWrap(points);
		} else if (!points.isEmpty()) {
			// bounding rect + border
			qreal x1, x2, y1, y2;
			x1 = x2 = points.first().x();
			y1 = y2 = points.first().y();
			foreach (QPointF p, points) {
				x1 = qMin(x1, p.x());
				x2 = qMax(x2, p.x());
				y1 = qMin(y1, p.y());
				y2 = qMax(y2, p.y());
			}
			x1 -= 5 * NETGRAPH_NODE_RADIUS;
			x2 += 5 * NETGRAPH_NODE_RADIUS;
			y1 -= 5 * NETGRAPH_NODE_RADIUS;
			y2 += 5 * NETGRAPH_NODE_RADIUS;
			hull << QPointF(x1, y1) << QPointF(x2, y1) << QPointF(x2, y2) << QPointF(x1, y2);
		}
		domains[ASIndex].hull = hull;
	}
}
#endif

QList<NetGraphNode> NetGraph::getHostNodes()
{
	QList<NetGraphNode> result;
	foreach (NetGraphNode n, nodes) {
		if (n.nodeType == NETGRAPH_NODE_HOST)
			result << n;
	}
	return result;
}

QList<int> NetGraph::getASList()
{
	QSet<int> asSet;
	foreach (NetGraphNode n, nodes) {
		asSet.insert(n.ASNumber);
	}
	return asSet.toList();
}

QList<int> NetGraph::getASNeighbours(int AS)
{
	QSet<int> neighbours;
	foreach (NetGraphEdge e, edges) {
		if (nodes[e.source].ASNumber == AS) {
			if (nodes[e.dest].ASNumber != AS) {
				neighbours.insert(nodes[e.dest].ASNumber);
			}
		}
		if (nodes[e.dest].ASNumber == AS) {
			if (nodes[e.source].ASNumber != AS) {
				neighbours.insert(nodes[e.source].ASNumber);
			}
		}
	}
	return neighbours.toList();
}

QList<int> NetGraph::edgeDepths(bool oriented)
{
	bool loadBalanced = false;
	foreach (NetGraphPath p, paths) {
		if (p.edgeList.isEmpty()) {
			loadBalanced = true;
			break;
		}
	}
	if (loadBalanced) {
		return edgeDepthsFromRoutes(oriented);
	} else {
		return edgeDepthsFromPaths(oriented);
	}
}

// Fast but does not work with load balancing
QList<int> NetGraph::edgeDepthsFromPaths(bool oriented)
{
	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);

	QList<int> depths;
	foreach (NetGraphEdge e, edges) {
		depths << INT_MAX;
	}

	foreach (NetGraphPath p, paths) {
		for (int i = 0; i < p.edgeList.length(); i++) {
			int dfwd = i;
			int drev = p.edgeList.length() - 1 - i;
			int d = oriented ? dfwd : qMin(dfwd, drev);
			depths[p.edgeList[i].index] = qMin(depths[p.edgeList[i].index], d);
		}
	}

	return depths;
}

// Fast but does not work with load balancing
QList<int> NetGraph::treeEdgeDepthsFromPaths()
{
	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);

	// find root
	// the root is the node that is an endpoint for every path
	int root = -1;
	{
		QVector<int> nodeFreq;
		nodeFreq.resize(nodes.count());
		for (int i = 0; i < paths.count(); i++) {
			nodeFreq[paths[i].source]++;
			nodeFreq[paths[i].dest]++;
		}
		int maxFreq = -1;
		for (int i = 0; i < nodeFreq.count(); i++) {
			if (nodeFreq[i] > maxFreq) {
				maxFreq = nodeFreq[i];
				root = i;
			}
		}
		if (maxFreq != paths.count() || root == -1) {
			Q_ASSERT_FORCE(false);
		}
	}

	QList<int> depths;
	foreach (NetGraphEdge e, edges) {
		depths << INT_MAX;
	}

	foreach (NetGraphPath p, paths) {
		for (int i = 0; i < p.edgeList.length(); i++) {
			int dfwd = i;
			int drev = p.edgeList.length() - 1 - i;
			int d = p.dest == root ? dfwd : drev;
			depths[p.edgeList[i].index] = qMin(depths[p.edgeList[i].index], d);
		}
	}

	return depths;
}

// Slow but works for load balancing
QList<int> NetGraph::edgeDepthsFromRoutes(bool oriented)
{
	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);

	QList<int> depths;

	QHash<QPair<qint32, qint32>, qint32> edgeCache;
	foreach (NetGraphEdge e, edges) {
		depths << INT_MAX;
		edgeCache[QPair<qint32, qint32>(e.source, e.dest)] = e.index;
	}

	QSet<int> hostNodes;
	foreach (NetGraphConnection c, connections) {
		hostNodes.insert(c.source);
		hostNodes.insert(c.dest);
	}

	foreach (int iSrc, hostNodes) {
		NetGraphNode &src= nodes[iSrc];
		// do a BFS from src
		QList<int> queue = QList<int>() << src.index;
		QList<int> nodeDepth;
		for (int i = 0; i < nodes.count(); i++)
			nodeDepth << -1;
		nodeDepth[src.index] = 0;
		QSet<int> marked;

		while (!queue.isEmpty()) {
			int n = queue.takeFirst();
			QList<Route> allRoutes;
			allRoutes << nodes[n].routes.localRoutes.values();
			// we can mix intra domain and cross domain routes because we only care about next hops
#if AS_AGGREGATION
			allRoutes << nodes[n].routes.interASroutes.values();
#else
			allRoutes << nodes[n].routes.aggregateInterASroutes.values();
#endif
			foreach (Route r, allRoutes) {
				if (!marked.contains(r.nextHop)) {
					queue << r.nextHop;
					marked.insert(r.nextHop);
					nodeDepth[r.nextHop] = nodeDepth[n] + 1;
				}
				int e = edgeCache[QPair<qint32, qint32>(n, r.nextHop)];
				if (depths[e] > nodeDepth[n]) {
					depths[e] = nodeDepth[n];
				}
			}
		}
	}

	// TODO this is not correct for asymmetric edges
	if (!oriented) {
		foreach (NetGraphEdge e, edges) {
			QPair<qint32, qint32> reversed(e.dest, e.source);
			if (edgeCache.contains(reversed)) {
				depths[e.index] = qMin(depths[e.index], depths[edgeCache[reversed]]);
			}
		}
	}

	return depths;
}

QList<int> NetGraph::getNextHop(int src, int dst)
{
	QList<int> result;
	int srcAS = nodes[src].ASNumber;
	int dstAS = nodes[dst].ASNumber;
#if AS_AGGREGATION
	if (srcAS == dstAS) {
		foreach (Route r, nodes[src].routes.localRoutes.values(dst)) {
			result << r.nextHop;
		}
	} else {
		foreach (Route r, nodes[src].routes.interASroutes.values(dstAS)) {
			result << r.nextHop;
		}
	}
#else
	if (srcAS != dstAS) {
		foreach (Route r, nodes[src].routes.aggregateInterASroutes.values(dstAS)) {
			result << r.nextHop;
		}
	}
	foreach (Route r, nodes[src].routes.localRoutes.values(dst)) {
		result << r.nextHop;
	}
#endif
	if (!nodes[src].loadBalancing && result.count() > 1) {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << QString("Fail: too many routes from %1 to %2").arg(src).arg(dst) << result;
		result = result.mid(0, 1);
	}
	return result;
}

QList<QList<NetGraphEdge> > NetGraph::toAdjacencyList()
{
	QList<QList<NetGraphEdge> > adj;

	for (int i = 0; i < nodes.count(); i++) {
		adj << QList<NetGraphEdge>();
	}

	for (int iEdge = 0; iEdge < edges.count(); iEdge++) {
		NetGraphEdge e = edges.at(iEdge);
		adj[e.source] << e;
	}

	return adj;
}

void SCC_DFS_Forward(int root, QList<QList<NetGraphEdge> > adj, QList<int> &S, QSet<int> &visited)
{
	visited.insert(root);
	foreach (NetGraphEdge e, adj[root]) {
		if (!visited.contains(e.dest)) {
			SCC_DFS_Forward(e.dest, adj, S, visited);
		}
	}
	S << root;
}

void SCC_DFS_Back(int root, QList<QList<NetGraphEdge> > adj, QSet<int> &visited, QSet<int> &dontTouch)
{
	visited.insert(root);
	foreach (NetGraphEdge e, adj[root]) {
		if (!visited.contains(e.dest) && !dontTouch.contains(e.dest)) {
			SCC_DFS_Back(e.dest, adj, visited, dontTouch);
		}
	}
}

QList<QList<NetGraphEdge> > SCC_Traspose(QList<QList<NetGraphEdge> > adj)
{
	QList<QList<NetGraphEdge> > adj2;
	for (int i = 0; i < adj.count(); i++) {
		adj2 << QList<NetGraphEdge>();
	}
	for (int i = 0; i < adj.count(); i++) {
		for (int j = 0; j < adj[i].count(); j++) {
			NetGraphEdge e = adj[i][j];
			qSwap(e.source, e.dest);
			adj2[e.source] << e;
		}
	}
	return adj2;
}

// Kosaraju's algorithm
/*
	Let G be a directed graph and S be an empty stack.
	While S does not contain all vertices:
	Choose an arbitrary vertex v not in S. Perform a depth-first search starting at v. Each time that
	depth-first search finishes expanding a vertex u, push u onto S.
	Reverse the directions of all arcs to obtain the transpose graph.
	While S is nonempty:
	Pop the top vertex v from S. Perform a depth-first search starting at v. The set of visited vertices
	will give the strongly connected component containing v; record this and remove all these vertices
	from the graph G and the stack S. Equivalently, breadth-first search (BFS) can be used instead of
	depth-first search.
	ref : http://en.wikipedia.org/wiki/Kosaraju%27s_algorithm
*/
QList<QList<NetGraphNode> > NetGraph::getSCC()
{
	if (nodes.isEmpty())
		return QList<QList<NetGraphNode> >();

	QList<QList<NetGraphEdge> > adj = toAdjacencyList();

	QList<int> S;
	QSet<int> visited;
	while (S.count() != nodes.count()) {
		int root = -1;
		for (int i = 0; i < nodes.count(); i++) {
			if (!visited.contains(i)) {
				root = i;
				break;
			}
		}
		Q_ASSERT_FORCE(root >= 0);
		SCC_DFS_Forward(root, adj, S, visited);
	}

	QList<QList<NetGraphEdge> > adj2 = SCC_Traspose(adj);

	QList<QList<NetGraphNode> > components;
	visited.clear();
	QSet<int> dontTouch;
	while (!S.isEmpty()) {
		int root = S.takeLast();
		QSet<int> component;
		SCC_DFS_Back(root, adj2, component, dontTouch);
		for (int i = 0; i < S.count(); i++) {
			if (component.contains(S[i])) {
				S.removeAt(i);
				i--;
			}
		}
		dontTouch.unite(component);
		QList<NetGraphNode> c;
		foreach (int i, component) {
			c << nodes[i];
		}
		components << c;
	}

	return components;
}

#ifndef LINE_EMULATOR
void NetGraph::computeDomainsFromSCC()
{
	QList<QList<NetGraphNode> > scc = getSCC();

	domains.clear();
	for (int iAS = 0; iAS < scc.count(); iAS++) {
		foreach (NetGraphNode n, scc[iAS]) {
			nodes[n.index].ASNumber = iAS;
		}
	}
	computeASHulls();
}
#endif

QList<int> NetGraph::nodeDegrees()
{
	QList<int> degrees;
	for (int i = 0; i < nodes.count(); i++) {
		degrees << 0;
	}

	foreach (NetGraphEdge e, edges) {
		degrees[e.source]++;
		degrees[e.dest]++;
	}

	return degrees;
}

QString NetGraph::toText()
{
	QStringList result;
	result << QString("Graph %1").arg(fileName);
	result << QString("Nodes %1").arg(nodes.count());
    result << QString("Links %1").arg(edges.count());
	result << QString("Paths %1").arg(paths.count());
	result << QString("Connections %1").arg(connections.count());
	result << "";
	foreach (NetGraphNode n, nodes) {
		result << n.toText() << "";
	}
	foreach (NetGraphEdge e, edges) {
		result << e.toText() << "";
	}
	foreach (NetGraphPath p, paths) {
		result << p.toText() << "";
	}
	foreach (NetGraphConnection c, connections) {
		result << c.toText() << "";
	}
	return result.join("\n");
}

QVector<bool> NetGraph::getTrueEdgeNeutrality()
{
	QVector<bool> result(edges.count());
	foreach (NetGraphEdge e, edges) {
		result[e.index] = e.isNeutral();
	}
	return result;
}

QVector<int> NetGraph::getPathTrafficClass()
{
    QVector<int> result = getPathTrafficClassStrict();
    for (int p = 0; p < paths.count(); p++) {
        if (result[p] < 0) {
			result[p] = 0;
		}
	}
    return result;
}

QVector<int> NetGraph::getPathTrafficClassStrict(bool heavy)
{
    QVector<qint32> connection2path = getConnection2PathMapping();

    QVector<int> result(paths.count());
    const int uninitialized = -1;
    result.fill(uninitialized);
    for (int c = 0; c < connections.count(); c++) {
        if (heavy && connections[c].isLight()) {
            continue;
        }
        qint32 p = connection2path[c];
        if (p < 0) {
            continue;
        }
        int trafficClass = connections[c].trafficClass;
        if (result[p] == uninitialized) {
            result[p] = trafficClass;
        }
        Q_ASSERT_FORCE(trafficClass == result[p]);
    }
    return result;
}

QVector<int> NetGraph::getConnectionTrafficClass()
{
    QVector<int> trafficClass(connections.count());
    for (int c = 0; c < connections.count(); c++) {
        trafficClass[c] = connections[c].trafficClass;
    }
    return trafficClass;
}

QVector<qint32> NetGraph::getConnection2PathMapping()
{
	QHash<QPair<qint32, qint32>, qint32 > pathEndpoints2index;
	for (int p = 0; p < paths.count(); p++) {
		pathEndpoints2index[QPair<qint32, qint32>(paths[p].source, paths[p].dest)] = p;
	}
	QVector<qint32> connection2path(connections.count());
	for (int c = 0; c < connections.count(); c++) {
		connection2path[c] = pathEndpoints2index.value(QPair<qint32, qint32>(connections[c].source, connections[c].dest), -1);
	}
	return connection2path;
}

QList<qint32> NetGraph::getInternalLinks()
{
	QSet<qint32> hostLinks;
	for (int p = 0; p < paths.count(); p++) {
		if (!paths[p].edgeList.isEmpty()) {
			hostLinks.insert(paths[p].edgeList.first().index);
			hostLinks.insert(paths[p].edgeList.last().index);
		}
	}
	QList<qint32> result;
	foreach (NetGraphEdge e, edges) {
		if (!hostLinks.contains(e.index)) {
			result << e.index;
		}
	}
	return result;
}

QList<QPair<qint32, qint32> > NetGraph::getSparseRoutingMatrixTransposed()
{
	QList<QPair<qint32, qint32> > result;
	for (int p = 0; p < paths.count(); p++) {
		foreach (NetGraphEdge e, paths[p].edgeList) {
			result << QPair<qint32, qint32>(e.index, p);
		}
	}
    return result;
}

QList<QPair<qint32, qint32> > NetGraph::getSparseConnectionRoutingMatrixTransposed()
{
    QList<QPair<qint32, qint32> > result;
    QVector<qint32> c2p = getConnection2PathMapping();
    for (int c = 0; c < connections.count(); c++) {
        int p = c2p[c];
        if (p < 0)
            continue;
        foreach (NetGraphEdge e, paths[p].edgeList) {
            result << QPair<qint32, qint32>(e.index, c);
        }
    }
    return result;
}

void NetGraph::flattenConnections()
{
    QList<NetGraphConnection> newConnections;
    for (int c = 0; c < connections.count(); c++) {
        for (int i = 1; i < connections[c].multiplier; i++) {
            newConnections << connections[c];
            newConnections.last().index = connections.count() + newConnections.count() - 1;
            newConnections.last().multiplier = 1;
            connections[c].multiplier = 1;
        }
    }
    connections.append(newConnections);
}

void NetGraph::assignPorts()
{
    flattenConnections();

    int port = getBasePort();
    foreach (NetGraphConnection c, connections) {
        if (c.basicType == "TCP") {
            connections[c.index].port = port;
            port++;
        } else if (c.basicType == "TCP-Poisson-Pareto") {
            connections[c.index].port = port;
            port++;
        } else if (c.basicType == "TCP-DASH") {
            connections[c.index].port = port;
            port++;
        } else if (c.basicType == "UDP-CBR") {
            connections[c.index].ports.clear();
            connections[c.index].port = port;
            port++;
        } else if (c.basicType == "UDP-VBR") {
            connections[c.index].ports.clear();
            connections[c.index].port = port;
            port++;
        } else if (c.basicType == "UDP-VCBR") {
            connections[c.index].ports.clear();
            connections[c.index].port = port;
            port++;
        } else {
            qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters:" << c.encodedType;
            Q_ASSERT_FORCE(false);
        }
    }
}

qint32 NetGraph::getConnectionIndex(quint16 port) const
{
    qint32 result = port;
    result -= getBasePort();
    if (result >= 0 && result < connections.count())
        return result;
    return -1;
}

quint16 NetGraph::getBasePort() const
{
    // We should never assign ports that might be used as ephemeral ports.
    // See https://en.wikipedia.org/wiki/Ephemeral_port
    // This means no ports >= 32768, and no ports in [1024, 5000].
    return 8000;
}
