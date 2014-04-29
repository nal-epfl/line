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

#include "briteimporter.h"

#include "util.h"
#include "netgraph.h"

BriteImporter::BriteImporter() :
	QObject(0)
{
}

/** Notes:
  * - border routers are kept border routers
  * - all other RT_NODEs (regular routers) are transformed into gateways
*/
bool BriteImporter::import(QString fromFile, QString toFile)
{
	QString text;
	if (!readFile(fromFile, text)) {
		emit logError(QString("Could not open file %1 in read mode").arg(fromFile));
		return false;
	}

	text = text.replace('\t', ' ');

	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	text.clear();

	emit logInfo(QString("Opened file %1: %2 lines").arg(fromFile).arg(lines.count()));

	QStringList tokens;
	bool ok;
	int lineNumber = 0;
	QString line = lines.takeFirst();
	lineNumber++;
	if (!line.startsWith("Topology:")) {
		emit logError(QString("File %1:%2: expected a topology declaration").arg(fromFile).arg(lineNumber));
		return false;
	}
	line = line.replace("Topology:", "").replace('(', ' ').replace(')', ' ').replace("Nodes,", " ").replace("Edges", " ");
	tokens = line.split(' ', QString::SkipEmptyParts);
	if (tokens.count() != 2) {
		emit logError(QString("File %1:%2: expected 2 numbers").arg(fromFile).arg(lineNumber));
		return false;
	}

	int nodeCount;
	int edgeCount;

	nodeCount = tokens.at(0).toInt(&ok);
	if (!ok || nodeCount <= 0) {
		emit logError(QString("File %1:%2: expected 2 positive integers").arg(fromFile).arg(lineNumber));
		return false;
	}
	edgeCount = tokens.at(1).toInt(&ok);
	if (!ok || edgeCount <= 0) {
		emit logError(QString("File %1:%2: expected 2 positive integers").arg(fromFile).arg(lineNumber));
		return false;
	}
    emit logInfo(QString("Found a topology with %1 nodes and %2 links").arg(nodeCount).arg(edgeCount));

	NetGraph g;

	QString state = "model";
	int nodesLeft, edgesLeft;
	int firstNodeIndex = -1;
	nodesLeft = edgesLeft = 0;
	foreach (line, lines) {
		lineNumber++;
		if (state == "model") {
			if (line.startsWith("Model")) {
				emit logInfo(QString("Found model: %1").arg(line));
			} else if (line.startsWith("Nodes:")) {
				line = line.replace("Nodes:", "").replace('(', ' ').replace(')', ' ');
				tokens = line.split(' ', QString::SkipEmptyParts);
				if (tokens.count() != 1) {
					emit logError(QString("File %1:%2: expected 1 positive integer").arg(fromFile).arg(lineNumber));
					return false;
				}
				int nodeCount2 = tokens.first().toInt(&ok);
				if (!ok || nodeCount2 != nodeCount) {
					emit logError(QString("File %1:%2: expected 1 positive integer that would match the previous node count").arg(fromFile).arg(lineNumber));
					return false;
				}
				state = "nodes";
				nodesLeft = nodeCount;
			}
		} else if (state == "nodes") {
			if (nodesLeft > 0) {
				// [NodeID]  [x-coord]  [y-coord]  [inDegree] [outDegree] [ASid]  [type]
				tokens = line.split(' ', QString::SkipEmptyParts);
				if (tokens.count() != 7) {
					emit logError(QString("File %1:%2: expected 7 tokens").arg(fromFile).arg(lineNumber));
					return false;
				}

				int nodeId;
				double x, y;
				int inDegree, outDegree;
				int asNumber;
				QString nodeType;

				nodeId = tokens.takeFirst().toInt(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for nodeID").arg(fromFile).arg(lineNumber));
					return false;
				}

				x = tokens.takeFirst().toDouble(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected a double for node x").arg(fromFile).arg(lineNumber));
					return false;
				}

				y = tokens.takeFirst().toDouble(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected a double for node y").arg(fromFile).arg(lineNumber));
					return false;
				}

				inDegree = tokens.takeFirst().toInt(&ok);
				Q_UNUSED(inDegree);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for node in-degree").arg(fromFile).arg(lineNumber));
					return false;
				}

				outDegree = tokens.takeFirst().toInt(&ok);
				Q_UNUSED(outDegree);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for node out-degree").arg(fromFile).arg(lineNumber));
					return false;
				}

				asNumber = tokens.takeFirst().toInt(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for node ASN").arg(fromFile).arg(lineNumber));
					return false;
				}

				nodeType = tokens.takeFirst();
				if (nodeType != "RT_BORDER" && nodeType != "RT_NODE") {
					emit logError(QString("File %1:%2: unexpected node type (unsupported?)").arg(fromFile).arg(lineNumber));
					return false;
				}

				if (firstNodeIndex < 0)
					firstNodeIndex = nodeId;

				// add to graph
				int index = g.addNode(nodeType == "RT_BORDER" ? NETGRAPH_NODE_BORDER : NETGRAPH_NODE_GATEWAY, QPointF(x, y), asNumber);
				if (nodeId - firstNodeIndex != index) {
					emit logError(QString("File %1:%2: could not keep track of the node indices").arg(fromFile).arg(lineNumber));
					return false;
				}
			}
			nodesLeft--;
			if (nodesLeft == 0) {
				// fix AS numbers: make them start from zero
				QHash<int, int> asMappings;
				foreach (NetGraphNode n, g.nodes) {
					asMappings[n.ASNumber] = -1;
				}
				QList<int> oldNumbers = asMappings.keys();
				qSort(oldNumbers);
				for (int i = 0; i < oldNumbers.count(); i++) {
					asMappings[oldNumbers[i]] = i;
				}
				foreach (NetGraphNode n, g.nodes) {
					g.nodes[n.index].ASNumber = asMappings[g.nodes[n.index].ASNumber];
				}

				state = "pre-edges";
			}
		} else if (state == "pre-edges" && line.startsWith("Edges:")) {
			line = line.replace("Edges:", "").replace('(', ' ').replace(')', ' ');
			tokens = line.split(' ', QString::SkipEmptyParts);
			if (tokens.count() != 1) {
				emit logError(QString("File %1:%2: expected 1 positive integer").arg(fromFile).arg(lineNumber));
				return false;
			}
			int edgeCount2 = tokens.first().toInt(&ok);
			if (!ok || edgeCount2 != edgeCount) {
                emit logError(QString("File %1:%2: expected 1 positive integer that would match the previous edge count").arg(fromFile).arg(lineNumber));
				return false;
			}
			state = "edges";
			edgesLeft = edgeCount;
		} else if (state == "edges") {
			if (edgesLeft > 0) {
				// [EdgeID]  [fromNodeID]  [toNodeID]  [Length]  [Delay]  [Bandwidth]  [ASFromNodeID]  [ASToNodeID]  [EdgeType]  [Direction]
				tokens = line.split(' ', QString::SkipEmptyParts);
				if (tokens.count() != 10) {
					emit logError(QString("File %1:%2: expected 10 tokens").arg(fromFile).arg(lineNumber));
					return false;
				}

				int edgeId;
				int fromNode, toNode;
				double length;
				double delay, bandwidth;
				int fromASNumber, toASNumber;
				QString edgeType;
				QString directivity;

				edgeId = tokens.takeFirst().toInt(&ok);
				Q_UNUSED(edgeId);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for edgeId").arg(fromFile).arg(lineNumber));
					return false;
				}

				fromNode = tokens.takeFirst().toInt(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for fromNode").arg(fromFile).arg(lineNumber));
					return false;
				}

				toNode = tokens.takeFirst().toInt(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for toNode").arg(fromFile).arg(lineNumber));
					return false;
				}

				length = tokens.takeFirst().toDouble(&ok);
				if (!ok || length < 0) {
					emit logError(QString("File %1:%2: expected a nonnegative double for length").arg(fromFile).arg(lineNumber));
					return false;
				}

				delay = tokens.takeFirst().toDouble(&ok);
				if (!ok || delay <= 0) {
					emit logError(QString("File %1:%2: expected a positive delay for length").arg(fromFile).arg(lineNumber));
					return false;
				}

				bandwidth = tokens.takeFirst().toDouble(&ok);
				if (!ok || bandwidth <= 0) {
					emit logError(QString("File %1:%2: expected a positive bandwidth for length").arg(fromFile).arg(lineNumber));
					return false;
				}
				// XXX for testing
				// bandwidth = 1000; // KB/s

				fromASNumber = tokens.takeFirst().toInt(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for fromASNumber").arg(fromFile).arg(lineNumber));
					return false;
				}

				toASNumber = tokens.takeFirst().toInt(&ok);
				if (!ok) {
					emit logError(QString("File %1:%2: expected an integer for toASNumber").arg(fromFile).arg(lineNumber));
					return false;
				}

				edgeType = tokens.takeFirst();
				// we don't care what type it is

				if (fromASNumber != toASNumber) {
					bandwidth = 1000; // KB/s
				} else {
					bandwidth = 100; // KB/s
				}

				directivity = tokens.takeFirst();
				if (directivity != "U" && directivity != "D") {
					emit logError(QString("File %1:%2: expected U or D for the edge directivity").arg(fromFile).arg(lineNumber));
					return false;
				}

				//add to graph
				fromNode -= firstNodeIndex;
				if (fromNode < 0 || fromNode >= g.nodes.count()) {
					emit logError(QString("File %1:%2: wrong source node index").arg(fromFile).arg(lineNumber));
					return false;
				}
				toNode -= firstNodeIndex;
				if (toNode < 0 || toNode >= g.nodes.count()) {
					emit logError(QString("File %1:%2: wrong destination node index").arg(fromFile).arg(lineNumber));
					return false;
				}
				if (!g.canAddEdge(fromNode, toNode)) {
					emit logError(QString("File %1:%2: cannot add an edge between the specified nodes").arg(fromFile).arg(lineNumber));
					return false;
				}
				g.addEdge(fromNode, toNode, bandwidth, delay, 0, NetGraph::optimalQueueLength(bandwidth, delay));
				if (directivity == "U") {
					g.addEdge(toNode, fromNode, bandwidth, delay, 0, NetGraph::optimalQueueLength(bandwidth, delay));
				}
			}
			edgesLeft--;
			if (edgesLeft == 0) {
				state = "done";
			}
		} else {
		}
	}

	if (state != "done") {
		emit logError(QString("File %1:%2: parser in unexpected state %3").arg(fromFile).arg(lineNumber).arg(state));
		return false;
	}

	emit logInfo("Saving...");

	g.setFileName(toFile);
	g.saveToFile();

	emit logInfo("All fine.");
	return true;
}


