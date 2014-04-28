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

#define _USE_MATH_DEFINES
#include <math.h>

#include "gephilayout.h"

#include <QtXml>
#include "util.h"

bool exportGexf(QList<QPointF> &nodes, QList<qreal> nodeSizes, QList<QPair<int, int> > edges, QString fileName)
{
	QDomDocument doc("");
	doc.appendChild(doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"ISO-8859-1\""));

	QDomElement gexf = doc.createElement("gexf");
	gexf.setAttribute("xmlns", "http://www.gexf.net/1.2draft");
	gexf.setAttribute("version", "1.2");
	gexf.setAttribute("xmlns:viz", "http://www.gexf.net/1.2draft/viz");
	gexf.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
	gexf.setAttribute("xsi:schemaLocation", "http://www.gexf.net/1.2draft http://www.gexf.net/1.2draft/gexf.xsd");
	doc.appendChild(gexf);

	QDomElement graph = doc.createElement("graph");
	gexf.appendChild(graph);
	graph.setAttribute("mode", "static");
	graph.setAttribute("defaultedgetype", "directed");

	QDomElement nodesElem = doc.createElement("nodes");
	graph.appendChild(nodesElem);

	for (int i = 0; i < nodes.count(); i++) {
		QDomElement nodeElem = doc.createElement("node");
		nodeElem.setAttribute("id", i);
		QDomElement pos = doc.createElement("viz:position");
		pos.setAttribute("x", nodes[i].x());
		pos.setAttribute("y", nodes[i].y());
		nodeElem.appendChild(pos);
		if (!nodeSizes.isEmpty()) {
			QDomElement size = doc.createElement("viz:size");
			size.setAttribute("value", nodeSizes[i]);
			nodeElem.appendChild(size);
			QDomElement shape = doc.createElement("viz:shape");
			shape.setAttribute("value", "disc");
			nodeElem.appendChild(shape);
		}
		nodesElem.appendChild(nodeElem);
	}

	QDomElement edgesElem = doc.createElement("edges");
	graph.appendChild(edgesElem);

	for (int i = 0; i < edges.count(); i++) {
		QDomElement edge = doc.createElement("edge");
		edge.setAttribute("id", i);
		edge.setAttribute("source", edges[i].first);
		edge.setAttribute("target", edges[i].second);
		edge.setAttribute("weight", 1.0);
		edgesElem.appendChild(edge);
	}

	return saveFile(fileName, doc.toString());
}

bool importGexfPositions(QList<QPointF> &nodes, QString fileName)
{
	QDomDocument doc(fileName);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		fdbg << "Could not open file in read mode, file name:" << fileName;
		return false;
	}
	QString errorMsg;
	int errorLine;
	int errorColumn;
	if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
		fdbg << "Could not read XML, file name:"
				<< fileName + ":" + QString::number(errorLine) + ":" + QString::number(errorColumn) <<
				errorMsg;
		file.close();
		return false;
	}
	file.close();

	QDomElement gexf = doc.documentElement();

	QDomNodeList nodesElem = gexf.elementsByTagName("node");
	for (int iNode = 0; iNode < nodesElem.count(); iNode++) {
		QDomNode node = nodesElem.at(iNode);

		bool ok;
		int id = node.attributes().namedItem("id").toAttr().value().toInt(&ok);
		if (!ok || id < 0 || id >= nodes.count()) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
			return false;
		}

		QDomNodeList position = node.toElement().elementsByTagName("viz:position");
		if (position.count() != 1) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
			return false;
		}
		QDomNode pos = position.at(0);
		double x = pos.attributes().namedItem("x").toAttr().value().toDouble(&ok);
		if (!ok) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
			return false;
		}
		double y = pos.attributes().namedItem("y").toAttr().value().toDouble(&ok);
		if (!ok) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
			return false;
		}

		nodes[id].setX(x);
		nodes[id].setY(y);
	}

	return true;
}

void rescale(QList<QPointF> &nodes, QList<QPair<int, int> > edges, const qreal lowerDistLimit)
{
	// rescale if necessary
	qreal minNodeDist = 1.0e100;
	for (int iEdge = 0; iEdge < edges.count(); iEdge++) {
		QPair<int, int> e = edges[iEdge];
		int i = e.first;
		int j = e.second;
		qreal d = sqrt((nodes.at(i).x() - nodes.at(j).x()) * (nodes.at(i).x() - nodes.at(j).x()) + (nodes.at(i).y() - nodes.at(j).y()) * (nodes.at(i).y() - nodes.at(j).y()));
		if (d > 0) {
			minNodeDist = qMin(minNodeDist, d);
		}
	}
    //if (minNodeDist < lowerDistLimit) {
        qreal rescaleFactor = lowerDistLimit/minNodeDist;
        // qDebug() << "rescaleFactor" << rescaleFactor;
		for (int i = 0; i < nodes.count(); i++) {
            nodes[i].setX(nodes[i].x() * rescaleFactor);
            nodes[i].setY(nodes[i].y() * rescaleFactor);
		}
    //}
}

#define DEBUG_GEXF 0
bool runLayout(QList<QPointF> &nodes, QList<qreal> nodeSizes, QList<QPair<int, int> > edges, QString config)
{
	//
	QTemporaryFile fileIn("qt_tempXXXXXX_in.gexf");
	fileIn.setAutoRemove(!DEBUG_GEXF);
	if (!fileIn.open()) {
		qDebug() << __FUNCTION__ << "Could not open temp file";
		return false;
	}
	fileIn.close();

	QTemporaryFile fileOut("qt_tempXXXXXX_out.gexf");
	fileOut.setAutoRemove(!DEBUG_GEXF);
	if (!fileOut.open()) {
		qDebug() << __FUNCTION__ << "Could not open temp file";
		return false;
	}
	fileOut.close();

	QString inputFileName = fileIn.fileName();
	QString outputFileName = fileOut.fileName();

	if (!exportGexf(nodes, nodeSizes, edges, inputFileName)) {
		qDebug() << "Could not export gexf";
        qDebug() << inputFileName << outputFileName;
		return false;
	}

	QString output;
	int exitCode;
	{
		QProcess p;
		p.setProcessChannelMode(QProcess::MergedChannels);
		QStringList args = QStringList() << "-jar" << "../line-layout/gephi-toolkit-demos/dist/ToolkitDemos.jar" <<
										inputFileName << outputFileName << config;
        // qDebug() << args;
		p.start("java", args);
		if (!p.waitForStarted(10 * 1000)) {
			qDebug() << "ERROR: Could not start gephi";
			return false;
		}

		if (!p.waitForFinished(-1)) {
			qDebug() << "ERROR: gephi failed (error or timeout).";
			p.kill();
			p.waitForFinished(5 * 1000);
			output = p.readAllStandardOutput();
			return false;
		}
		output = p.readAllStandardOutput();
		exitCode = p.exitCode();
	}
	if (exitCode != 0) {
		qDebug() << "Could not layout gexf" << output;
		return false;
	} else {
		// OK
        // qDebug() << "Gephi says:" << output;
	}

	if (!importGexfPositions(nodes, outputFileName))
		return false;
	//rescale(nodes, edges);
	return true;
}

