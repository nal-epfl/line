/*
 *	Copyright (C) 2012 Ovidiu Mara
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

#include "graphmlimporter.h"

#include <QtXml>

#include "util.h"
#include "netgraph.h"

GraphMLImporter::GraphMLImporter()
{
}

bool GraphMLImporter::import(QString fromFile, QString toFile)
{
	QDomDocument doc(fromFile);
	{
		QFile file(fromFile);
		if (!file.open(QIODevice::ReadOnly)) {
			fdbg << "Could not open file in read mode, file name:" << fromFile;
			return false;
		}
		QString errorMsg;
		int errorLine;
		int errorColumn;
		if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
			qDebug() << "Could not read XML, file name:"
					<< fromFile + ":" + QString::number(errorLine) + ":" + QString::number(errorColumn) <<
					errorMsg;
			file.close();
			return false;
		}
		file.close();
	}

	QDomElement graphml = doc.documentElement();

	QDomNodeList graphs = graphml.elementsByTagName("graph");

	if (graphs.count() != 1) {
		qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		return false;
	}
	QDomNode graph = graphs.at(0);

	bool directed = false;

	if (graph.attributes().contains("edgedefault")) {
		directed = graph.attributes().namedItem("edgedefault").toAttr().value() == "directed";
	}

	QHash<QString, QString> keyNames;
	QDomNodeList keys = graphml.elementsByTagName("key");
	for (int iKey = 0; iKey < keys.count(); iKey++) {
		QDomNode key = keys.at(iKey);
		// <key attr.name="Longitude" attr.type="double" for="node" id="d32" />
		if (!(key.attributes().contains("attr.name") && key.attributes().contains("attr.type") && key.attributes().contains("for") && key.attributes().contains("id")))
			continue;
		QString attrName = key.attributes().namedItem("attr.name").toAttr().value();
		QString attrType = key.attributes().namedItem("attr.type").toAttr().value();
		QString attrFor = key.attributes().namedItem("for").toAttr().value();
		QString attrId = key.attributes().namedItem("id").toAttr().value();

		if (attrName == "Longitude" && attrType == "double" && attrFor == "node") {
			keyNames[attrId] = attrName;
		}
		if (attrName == "Latitude" && attrType == "double" && attrFor == "node") {
			keyNames[attrId] = attrName;
		}
	}

	NetGraph g;

	QDomNodeList nodes = graph.toElement().elementsByTagName("node");
	QHash<QString, int> nodeIds;
	for (int iNode = 0; iNode < nodes.count(); iNode++) {
		QDomNode node = nodes.at(iNode);

		QString id = node.attributes().namedItem("id").toAttr().value();
		if (nodeIds.contains(id)) {
			emit logError(QString("Duplicate node detected: %1 (graphml id). Ignoring.").arg(id));
			continue;
		}

		QPointF pos;
		bool latOk, longOk;

		QDomNodeList dataNodes = node.toElement().elementsByTagName("data");
		// <data key="d29">38.89511</data>
		for (int iData = 0; iData < dataNodes.count(); iData++) {
			QDomNode data = dataNodes.at(iData);
			if (!data.attributes().contains("key"))
				continue;
			QString key = data.attributes().namedItem("key").toAttr().value();
			if (!keyNames.contains(key))
				continue;
			QString keyName = keyNames[key];
			if (keyName == "Latitude") {
				QString value = data.firstChild().toText().data();
				pos.setY(value.toDouble(&latOk));
			} else if (keyName == "Longitude") {
				QString value = data.firstChild().toText().data();
				pos.setX(value.toDouble(&longOk));
			}
		}

		if (!(latOk && longOk)) {
			pos = QPointF();
		}

		int index = g.addNode(NETGRAPH_NODE_GATEWAY, pos);
		nodeIds[id] = index;
	}

	QDomNodeList edges = graph.toElement().elementsByTagName("edge");
	QHash<QPair<int, int>, int> edgeCache;
	for (int iEdge = 0; iEdge < edges.count(); iEdge++) {
		QDomNode edge = edges.at(iEdge);

		QString src = edge.attributes().namedItem("source").toAttr().value();
		QString dst = edge.attributes().namedItem("target").toAttr().value();
		if (src == dst || !nodeIds.contains(src) || !nodeIds.contains(dst)) {
			emit logError(QString("Invalid edge detected: %1 -> %2 (graphml ids). Ignoring.").arg(src).arg(dst));
			continue;
		}
		int nsrc = nodeIds[src];
		int ndst = nodeIds[dst];

		bool edgedirected = directed;
		if (edge.attributes().contains("directed")) {
			edgedirected = edge.attributes().namedItem("directed").toAttr().value() == "true";
		}

		qreal bandwidth = 1000; // KB/s
		int delay = 1; // ms

		QPair<int, int> endpoints = QPair<int, int>(nsrc, ndst);
		if (edgeCache.contains(endpoints)) {
			emit logError(QString("Duplicate edge detected: %1 -> %2. Ignoring.").arg(endpoints.first).arg(endpoints.second));
		} else {
			edgeCache[endpoints] = g.addEdge(nsrc, ndst, bandwidth, delay, 0, NetGraph::optimalQueueLength(bandwidth, delay));
		}
		if (!edgedirected) {
			QPair<int, int> endpointsReverse = QPair<int, int>(endpoints.second, endpoints.first);
			if (edgeCache.contains(endpointsReverse)) {
				emit logError(QString("Duplicate edge detected: %1 -> %2. Ignoring.").arg(endpointsReverse.first).arg(endpointsReverse.second));
			} else {
				edgeCache[endpointsReverse] = g.addEdge(ndst, nsrc, bandwidth, delay, 0, NetGraph::optimalQueueLength(bandwidth, delay));
			}
		}
	}

	g.setFileName(toFile);
	g.saveToFile();

	emit logInfo("All fine.");
	return true;
}
