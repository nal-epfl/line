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

#ifndef NETGRAPH_H
#define NETGRAPH_H

#include <QtCore>

#include "netgraphcommon.h"
#include "netgraphnode.h"
#include "netgraphedge.h"
#include "netgraphpath.h"
#include "netgraphas.h"
#include "netgraphconnection.h"
#include "util.h"

#ifndef LINE_EMULATOR
#include "gephilayoutnetgraph.h"
#else
#include "../util/ovector.h"
#endif

#ifdef LINE_EMULATOR
#define NO_ROUTE 0xFFffFFffU
#define LOAD_BALANCED_ROUTE_MASK 0x80000000U
#define LOAD_BALANCED_VALUE_MASK 0x7FFFFFFFU
#endif

class NetGraph
{
public:
	NetGraph();
	~NetGraph();

	QList<NetGraphNode> nodes;
	QList<NetGraphEdge> edges;
	QList<NetGraphConnection> connections;

	// updated by computeASHulls
	QList<NetGraphAS> domains;

	// not updated by anything
	QList<NetGraphPath> paths;
	QString fileName;

	qint32 rootId;
	QPointF viewportCenter;
	qreal viewportZoom;

	// Whether we are using this graph in the interface (default true)
	// Use false in multi threaded batch jobs
	bool hasGUI;

#ifdef LINE_EMULATOR
	// maps (node ID, node ID) -> edge ID
	QHash<QPair<qint32, qint32>, qint32> edgeCache;
	// maps (node ID, node ID) -> path ID
	QHash<QPair<qint32, qint32>, qint32 > pathCache;
	// vector index: node ID
	// value: if the node is a host, its index in a list of all host nodes ordered by node ID
	//        else undefined
	OVector<quint32> destID2Index;
	// first index: current node ID
	// second index: destID2Index[destination node ID]
	// item: if item == NO_ROUTE: no route
	// item: else if item & LOAD_BALANCED_ROUTE == 1: index = item & LOAD_BALANCED_MASK; lookup
	//       loadBalancedRouteCache[index]
	// item: else: item = next hop node ID
	OVector<OVector<quint32> > routeCache;
	// first index: see above
	// loadBalancedRouteCache[index] = vector of all the possible next hop node IDs
	OVector<OVector<qint32> > loadBalancedRouteCache;
#endif

	// Adds a node of type NETGRAPH_NODE_something, at a scene position pos
	// Returns the node index
	int addNode(int type, QPointF pos = QPointF(), int ASNumber = 0, QString customLabel = QString());

	// Adds an edge between two nodes
	// Returns the edge index
	int addEdge(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength, bool force = false);

	// Adds two edges with identical attrbiutes in both directions
	// Returns the index of the second one
	int addEdgeSym(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength, bool force = false);
	// Checks whether an edge can be added between two nodes
	bool canAddEdge(int nodeStart, int nodeEnd);

	// Adds a connection
	int addConnection(NetGraphConnection c);

	void deleteNode(int index);
	void deleteEdge(int index);
	void deleteConnection(int index);

	void clearRoutes();

	void checkIndices();

	// deletes everything
	void clear();

	// sets the file name
	void setFileName(QString fileName);

	bool saveToFile();
	bool loadFromFile();

	QByteArray saveToByteArray();
	bool loadFromByteArray(QByteArray b);

	bool pathsValid();

#ifndef LINE_EMULATOR
	bool readjustQueues();
	bool readjustQueuesSmall();
#endif

	void recordTimelinesEverywhere(quint64 interval = 1ULL * 1000ULL * 1000ULL * 1000ULL);

#ifndef LINE_EMULATOR
	// Computes the routing tables for the current connections and then the paths.
	// This function should be used to compute the routes in most cases.
	bool computeRoutes();

	// builds a full mesh (dummy connections), computes the routing tables and then the paths (and removes the dummy connections)
	bool computeFullRoutes();

	// given the paths, computes the routing tables
	bool computeRoutesFromPaths();
#endif

#ifndef LINE_EMULATOR
#ifndef LINE_CLIENT
	// layouts the graph using Gephi
	bool layoutGephi(ProgressCallback *progressCallback = NULL);
	bool layoutGephiUsed();
#endif
#endif

#ifndef LINE_EMULATOR
	// computes the AS list and their GUI convex hulls
	void computeASHulls();
	void computeASHulls(int ASNumber);
#endif

	QList<int> getNextHop(int src, int dst);

	// searches the path with given src and dst node IDs
#ifndef LINE_EMULATOR
	NetGraphPath pathByNodeIndex(int src, int dst);
	NetGraphPath pathByNodeIndexTry(int src, int dst, bool &ok);
	// returns the bottleneck bandwidth in KB/s (same as B/ms); returns 0 on routing error
	qreal pathMaximumBandwidth(qint32 source, qint32 dest);
#else
	NetGraphPath& pathByNodeIndex(int src, int dst);
#endif

	// searches the edge with given src and dst node IDs
#ifndef LINE_EMULATOR
	NetGraphEdge edgeByNodeIndex(int src, int dst);
#else
	NetGraphEdge& edgeByNodeIndex(int src, int dst);
#endif

	// Marks nodes and edges as used/not used. The paths (actually the routes) need to be computed.
	bool updateUsed();

	// Enumerates the nodes that are hosts
	QList<NetGraphNode> getHostNodes();

	QList<int> getASList();
	QList<int> getASNeighbours(int AS);

	// Useful to determine how close a link is to the edge of the network
	QList<int> edgeDepths(bool oriented);
	QList<int> edgeDepthsFromPaths(bool oriented);
	QList<int> edgeDepthsFromRoutes(bool oriented);
	QList<int> treeEdgeDepthsFromPaths();

#ifdef LINE_EMULATOR
	void prepareEmulation();
#endif

	// Returns the optimal queue length (i.e. 1 RTT of traffic for 400B frames) in slots, given the bandwidth (KB/s) and delay (ms) over a link
	static int optimalQueueLength(double bandwidth, int delay);
	static void testComputePaths();

	QList<QList<NetGraphEdge> > toAdjacencyList();

	// compute the strongly connected components
	QList<QList<NetGraphNode> > getSCC();

#ifndef LINE_EMULATOR
	void computeDomainsFromSCC();
#endif

	QList<int> nodeDegrees();

	QString toText();

	QVector<bool> getTrueEdgeNeutrality();

	// >= 0 is a traffic class
	//    0 if there is no connection over the path
	// exit if there is more than 1 traffic class per path
	QVector<int> getPathTrafficClass();

    // >= 0 is a traffic class
    //  < 0 if there is no connection over the path
    // exit if there is more than 1 traffic class per path
    // If heavy is true, take into account only connections with heavy traffic
    QVector<int> getPathTrafficClassStrict(bool heavy = false);

    QVector<int> getConnectionTrafficClass();

	// -1 if there is no path for a connection
	QVector<qint32> getConnection2PathMapping();

	QList<qint32> getInternalLinks();

	QList<QPair<qint32, qint32> > getSparseRoutingMatrixTransposed();

    // The connections must have been flattened.
    QList<QPair<qint32, qint32> > getSparseConnectionRoutingMatrixTransposed();

    // Removes multipliers and instead creates etra connections. Does not work with TCPx (but TCP xn works).
    // Has no effect if the connections are already flattened.
    void flattenConnections();

    // Assigns port numbers to connections. Flattens connections if necessary.
    void assignPorts();

    // Returns the index of the connection using the specified port number, or -1.
    // Only works after calling assignPorts().
    qint32 getConnectionIndex(quint16 port) const;

    quint16 getBasePort() const;

	//Q_DISABLE_COPY(NetGraph)
};

QDataStream& operator>>(QDataStream& s, NetGraph& n);

QDataStream& operator<<(QDataStream& s, const NetGraph& n);

#endif // NETGRAPH_H
