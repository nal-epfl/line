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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtGui>

#include "../util/util.h"

void MainWindow::on_btnImportTraces_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this, QString("Open trace dump file..."), QString("."), QString("*.goodtraces"));
	if (fileName.isEmpty())
		return;

	QStringList fileNames;
	QStringList files;
	if (!readTextArchive(fileName, fileNames, files))
		return;

	qDebug() << fileNames;

	QHash<QString, QHash<QString, QString> > nodes;
	QList<QList<QString> > paths;
	QList<QPair<QString, QString> > pathSrcDest;

	for (int iFile = 0; iFile < fileNames.count(); iFile++) {
		QString name = fileNames[iFile];
		QString srcIp = name.section('(', 1, 1).section("->", 0, 0).trimmed();
		QString dstIp = name.section('(', 1, 1).section("->", 1, 1).section(')', 0, 0).trimmed();

		qDebug() << name << srcIp << dstIp;

		QString file = files[iFile];
		QStringList lines = file.split('\n', QString::SkipEmptyParts);
		lines = lines.mid(1);

		QList<QString> hops;

		int hopIndex = 0;
		foreach (QString line, lines) {
			if (!line.startsWith(QString("%1 ").arg(hopIndex)))
				break;
			line = line.mid(QString("%1 ").arg(hopIndex).length());

			if (!line.contains(" "))
				break;
			QString ip = line.section(' ', 0, 0, QString::SectionSkipEmpty);
			if (!ipnumOk(ip) && ip != "*")
				break;
			qDebug() << hopIndex << ip;
			hops << ip;
			if (ipnumOk(ip)) {
				line = line.section(' ', 1, -1, QString::SectionSkipEmpty);
				QStringList tagTokens = line.split('(', QString::SkipEmptyParts);
				QHash<QString, QString> tags;
				foreach (QString tag, tagTokens) {
					if (!tag.contains(')'))
						continue;
					tag = tag.replace(")", "");
					QString key = tag.section(' ', 0, 0).trimmed();
					QString value = tag.section(' ', 1).trimmed();
					tags[key] = value;
					qDebug() << key << value;
				}
				nodes[ip] = tags;
			}
			hopIndex++;
		}
		while (hops.endsWith("*")) {
			hops.removeLast();
		}
		paths << hops;
		pathSrcDest << QPair<QString, QString>(srcIp, dstIp);
	}

	NetGraph g;

	QHash<QString, qint32> ip2NodeIndex;
	QList<QList<qint32> > pathHop2nodeIndex;

	for (int iPath = 0; iPath < paths.count(); iPath++) {
		QList<QString> path = paths[iPath];
		pathHop2nodeIndex << QList<qint32>();
		for (int iHop = 0; iHop < path.count(); iHop++) {
			QString hop = path[iHop];
			int n;
			if (ipnumOk(hop)) {
				if (!ip2NodeIndex.contains(hop)) {
					int asNumber = 0;
					if (nodes[hop].contains("as")) {
						QString s = nodes[hop]["as"];
						s = s.section(" ", 0, 0);
						if (s.startsWith("AS")) {
							s = s.mid(2);
							asNumber = s.toInt();
						}
					}
					n = g.addNode((hop == pathSrcDest[iPath].first || hop == pathSrcDest[iPath].second) ? NETGRAPH_NODE_HOST : NETGRAPH_NODE_GATEWAY, QPointF(), asNumber);
					ip2NodeIndex[hop] = n;
					g.nodes[n].tags = nodes[hop];
					g.nodes[n].tags["ip"] = hop;
				} else {
					n = ip2NodeIndex[hop];
				}
			} else {
				// assume same AS
				int asNumber = 0;
				if (iHop > 0) {
					int pn = pathHop2nodeIndex[iPath][iHop-1];
					asNumber = g.nodes[pn].ASNumber;
				}
				n = g.addNode(NETGRAPH_NODE_GATEWAY, QPointF(), asNumber);
				ip2NodeIndex[hop] = n;
				g.nodes[n].tags["ip"] = "*";
			}
			pathHop2nodeIndex[iPath] << n;
		}
	}

	// make border routers
	for (int iPath = 0; iPath < paths.count(); iPath++) {
		for (int iHop = 0; iHop < pathHop2nodeIndex[iPath].count(); iHop++) {
			int n = pathHop2nodeIndex[iPath][iHop];
			if (iHop > 0) {
				int pn = pathHop2nodeIndex[iPath][iHop-1];
				if (g.nodes[pn].ASNumber != g.nodes[n].ASNumber) {
					g.nodes[pn].nodeType = NETGRAPH_NODE_BORDER;
					g.nodes[n].nodeType = NETGRAPH_NODE_BORDER;
				}
			}
		}
	}

	// add edges and paths
	QList< QList<qint32> > path2edges;
	QHash<QPair<qint32, qint32>, qint32 > nodePair2edge;
	for (int iPath = 0; iPath < paths.count(); iPath++) {
		QList<qint32> pathEdges;
		for (int iHop = 0; iHop < pathHop2nodeIndex[iPath].count(); iHop++) {
			int n = pathHop2nodeIndex[iPath][iHop];
			if (iHop > 0) {
				int pn = pathHop2nodeIndex[iPath][iHop-1];
				if (!nodePair2edge.contains(QPair<qint32, qint32>(pn, n))) {
					nodePair2edge[QPair<qint32, qint32>(pn, n)] = g.addEdge(pn, n, 100, 10, 0, 10);
				}
				pathEdges << nodePair2edge[QPair<qint32, qint32>(pn, n)];
			}
		}
		path2edges << pathEdges;
		if (!pathHop2nodeIndex[iPath].isEmpty()) {
			if (g.nodes[pathHop2nodeIndex[iPath].first()].nodeType == NETGRAPH_NODE_HOST && g.nodes[pathHop2nodeIndex[iPath].last()].nodeType == NETGRAPH_NODE_HOST) {
				NetGraphPath p;
				p.source = pathHop2nodeIndex[iPath].first();
				p.dest = pathHop2nodeIndex[iPath].last();
				foreach (qint32 e, path2edges[iPath]) {
					p.edgeList << g.edges[e];
				}
				p.edgeSet = p.edgeList.toSet();
				g.paths << p;
			}
		}
	}

	fileName = QFileDialog::getSaveFileName(this, QString("Save graph as..."), QString("."), QString("*.graph"));
	if (fileName.isEmpty())
		return;
	if (!fileName.endsWith(".graph"))
		fileName += ".graph";
	g.setFileName(fileName);
	g.saveToFile();
}

