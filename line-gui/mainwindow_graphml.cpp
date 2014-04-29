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

#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::on_btnGraphMLImportMerge_clicked()
{
	ui->txtBrite->clear();

	QStringList fileNames;
	{
		QFileDialog dialog(this);
		dialog.setFileMode(QFileDialog::ExistingFiles);
		dialog.setAcceptMode(QFileDialog::AcceptOpen);
		dialog.setDirectory("../zoo/archive/");
		dialog.setNameFilter("GraphML topology file (*.graphml)");
		if (!dialog.exec())
			return;
		fileNames = dialog.selectedFiles();
	}

	QTemporaryFile file;
	if (file.open()) {
		file.close();
	}

	QList<NetGraph*> graphs;

	foreach (QString fromFile, fileNames) {
		if (!graphMLImporter.import(fromFile, file.fileName()))
			continue;
		NetGraph *g = new NetGraph();
		g->setFileName(file.fileName());
		if (!g->loadFromFile()) {
			delete g;
			continue;
		}
		QString s = fromFile;
		s = s.split('/', QString::SkipEmptyParts).last().replace(".graphml", ".graph", Qt::CaseInsensitive);
		g->setFileName(s);
		graphs << g;
	}

	// for each graph, make sure we have coords for every node
	for (int iGraph = 0; iGraph < graphs.count(); iGraph++) {
		bool ok = true;
		for (int iNode = 0; iNode < graphs[iGraph]->nodes.count(); iNode++) {
			if (graphs[iGraph]->nodes[iNode].x == 0 && graphs[iGraph]->nodes[iNode].y == 0) {
				ok = false;
				break;
			}
		}
		if (!ok) {
			emit logError(ui->txtBrite, QString("Removing graph %1 (missing node coordinates)").arg(graphs[iGraph]->fileName));
			delete graphs[iGraph];
			graphs.removeAt(iGraph);
			iGraph--;
		}
	}

	// remove hosts
	foreach (NetGraph *g, graphs) {
		for (int i = 0; i < g->nodes.count(); i++) {
			if (g->nodes[i].nodeType == NETGRAPH_NODE_HOST) {
				g->deleteNode(i);
				i--;
			}
		}
	}

	// compute domain list
	QList<QPair<int, int> > domains;
	for (int ig = 0; ig < graphs.count(); ig++) {
		graphs[ig]->computeDomainsFromSCC();
		for (int id = 0; id < graphs[ig]->domains.count(); id++) {
			domains << QPair<int, int>(ig, id);
		}
	}

	QList<qreal> distances;
	QList<qreal> degrees;
	foreach (NetGraph *g, graphs) {
		foreach (NetGraphEdge e, g->edges) {
			qreal lat1, long1, lat2, long2;
			lat1 = g->nodes[e.source].y;
			long1 = g->nodes[e.source].x;
			lat2 = g->nodes[e.dest].y;
			long2 = g->nodes[e.dest].x;
			qreal d = sphereDistance(lat1, long1, lat2, long2);
			if (qIsNaN(d))
				continue;
			distances << d;
		}
		QList<int> nodeDegrees = g->nodeDegrees();
		foreach (int d, nodeDegrees) {
			degrees << d;
		}
	}

	// degree frequencies
	QList<qreal> degreeFreq;
	{
		int maxdeg = 1;
		foreach (qreal d, degrees) {
			maxdeg = qMax(maxdeg, qRound(d/2));
		}
		for (int d = 0; d <= maxdeg; d++) {
			degreeFreq << 0;
		}
		foreach (qreal d, degrees) {
			degreeFreq[qRound(d/2)]++;
		}
	}

	// compute domain degrees
	QList<qreal> domainDegreeFreq = degreeFreq;
	domainDegreeFreq[0] = 0; // make sure we have connectivity for everyone
	QList<int> domainDegrees;
	for (int i = 0; i < domains.count(); i++) {
		domainDegrees << randDist(domainDegreeFreq);
	}

	// domain degree frequencies
	domainDegreeFreq.clear();
	{
		int maxdeg = 1;
		foreach (int d, domainDegrees) {
			maxdeg = qMax(maxdeg, d);
		}
		for (int d = 0; d <= maxdeg; d++) {
			domainDegreeFreq << 0;
		}
		foreach (int d, domainDegrees) {
			domainDegreeFreq[d]++;
		}
	}

	// connect domains
	QList<QPair<int, int> > interDomainLinks;
	for (;;) {
		// initialize
		interDomainLinks.clear();
		QList<int> residualDomainDegrees = domainDegrees;
		QSet<int> remainingDomains;
		for (int i = 0; i < domains.count(); i++) {
			if (residualDomainDegrees[i] > 0)
				remainingDomains.insert(i);
		}
		// add links
		while (remainingDomains.count() >= 2) {
			int d1 = pickRandomElement(remainingDomains.toList());
			int d2 = d1;
			while (d1 == d2) {
				d2 = pickRandomElement(remainingDomains.toList());
			}
			interDomainLinks << QPair<int, int>(d1, d2);
			residualDomainDegrees[d1]--;
			if (residualDomainDegrees[d1] == 0) {
				remainingDomains.remove(d1);
			}
			residualDomainDegrees[d2]--;
			if (residualDomainDegrees[d2] == 0) {
				remainingDomains.remove(d2);
			}
		}
		// check for connectivity
		QSet<int> visited;
		QList<int> q;
		q << 0;
		visited.insert(0);
		while (!q.isEmpty()) {
			int d1 = q.takeFirst();
			for (int ie = 0; ie < interDomainLinks.count(); ie++) {
				int d2;
				if (interDomainLinks[ie].first == d1) {
					d2 = interDomainLinks[ie].second;
				} else if (interDomainLinks[ie].second == d1) {
					d2 = interDomainLinks[ie].first;
				} else {
					continue;
				}
				if (visited.contains(d2))
					continue;
				q << d2;
				visited.insert(d2);
			}
		}
		if (visited.count() < domains.count()) {
			qDebug() << "not fully connected!";
		} else {
			break;
		}
	}

	// merge graphs
	NetGraph gBig;
	QList<QList<int> > nodeMaps;
	for (int i = 0; i < graphs.count(); i++) {
		QList<int> gNodeMaps;
		for (int j = 0; j < graphs[i]->nodes.count(); j++) {
			gNodeMaps << -1;
		}
		nodeMaps << gNodeMaps;
	}
	for (int id = 0; id < domains.count(); id++) {
		int ig = domains[id].first;
		int gd = domains[id].second;
		NetGraph *g = graphs[ig];
		foreach (NetGraphNode n, g->nodes) {
			if (n.ASNumber == gd) {
				nodeMaps[ig][n.index] = gBig.addNode(NETGRAPH_NODE_GATEWAY, QPointF(n.x, n.y), id);
			}
		}
	}
	for (int ig = 0; ig < graphs.count(); ig++) {
		foreach (NetGraphEdge e, graphs[ig]->edges) {
			int n1 = nodeMaps[ig][e.source];
			int n2 = nodeMaps[ig][e.dest];
			Q_ASSERT_FORCE(n1 >= 0 && n2 >= 0);
			gBig.addEdge(n1, n2, e.bandwidth, e.delay_ms, e.lossBernoulli, e.queueLength);
		}
	}

	// add interdomain links
	QList< QList<int> > domainNodes;
	{
		for (int id = 0; id < domains.count(); id++) {
			domainNodes << QList<int>();
		}
		for (int i = 0; i < gBig.nodes.count(); i++) {
			domainNodes[gBig.nodes[i].ASNumber] << i;
		}
	}
	QSet<QPair<int, int> > interDomainEdgeCache;
	for (int ie = 0; ie < interDomainLinks.count(); ie++) {
		int d1 = interDomainLinks[ie].first;
		int d2 = interDomainLinks[ie].second;
		// pick the closest nodes (geographically)
		int n1 = -1;
		int n2 = -1;
		qreal dMin = 1.0e99;
		foreach (int i, domainNodes[d1]) {
			foreach (int j, domainNodes[d2]) {
				qreal d = sphereDistance(gBig.nodes[i].y, gBig.nodes[i].x, gBig.nodes[j].y, gBig.nodes[j].x);
				if (d < dMin && !interDomainEdgeCache.contains(QPair<int, int>(i, j))) {
					dMin = d;
					n1 = i;
					n2 = j;
				}
			}
		}
		if (n1 < 0 || n2 < 0) {
            emit logError(ui->txtBrite, QString("Could not add an interdomain link (not enough routers)"));
			continue;
		}
		gBig.nodes[n1].nodeType = NETGRAPH_NODE_BORDER;
		gBig.nodes[n2].nodeType = NETGRAPH_NODE_BORDER;
		gBig.addEdge(n1, n2, 1000, 1, 0, 10);
		gBig.addEdge(n2, n1, 1000, 1, 0, 10);
		interDomainEdgeCache << QPair<int, int>(n1, n2);
		interDomainEdgeCache << QPair<int, int>(n2, n1);
	}

	foreach (NetGraphNode n, gBig.nodes) {
		gBig.nodes[n.index].x *= 10;
		gBig.nodes[n.index].y *= 10;
	}

	// add hosts
	if (ui->checkImportAddHosts->isChecked()) {
		foreach (NetGraphNode n, gBig.nodes) {
			if (n.nodeType == NETGRAPH_NODE_GATEWAY) {
				int host = gBig.addNode(NETGRAPH_NODE_HOST, QPointF(n.x + 3*NETGRAPH_NODE_RADIUS, n.y), n.ASNumber);
				gBig.addEdgeSym(host, n.index, 300, 1, 0, 20);
			}
		}
	}

	gBig.computeASHulls();

	QFileDialog fileDialog(this, "Save network graph");
	fileDialog.setFilter("Network graph file (*.graph)");
	fileDialog.setDefaultSuffix("graph");
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setDirectory(QDir("."));

	QString fileName;
	while (fileDialog.exec()) {
		QStringList fileNames = fileDialog.selectedFiles();
		if (!fileNames.isEmpty()) {
			fileName = fileNames.first();
//			if (!fileInDirectory(fileName, ".")) {
//				QMessageBox::warning(this, "Incorrect save location",
//									 QString("Please choose a location in the current working directory (%1). You picked '%2'.").arg(QDir(".").absolutePath()).arg(QDir(fileName).canonicalPath()));
//				fileName = QString();
//				continue;
//			}
		}
		break;
	}
	if (!fileName.isEmpty()) {
		gBig.fileName = fileName;
		gBig.saveToFile();
	}

	// plots
	QList<QOPlot> plots;
	{
		QOPlot plot;
		plot.title = "CDF for the geographical distance of links";
		plot.xlabel = "Distance (km)";
		plot.ylabel = "Fraction";
		QVector<QPointF> CDF = QOPlot::makeCDF(distances, 0);
		plot.addData(QOPlotCurveData::fromPoints(CDF, plot.title));
		plots << plot;
	}
	{
		QOPlot plot;
		plot.title = "Histogram for the geographical distance of links";
		plot.xlabel = "Distance (km)";
		plot.ylabel = "Count";
		qreal binWidth;
		QVector<QPointF> hist = QOPlot::makeHistogram(distances, binWidth);
		plot.addData(QOPlotCurveData::fromPoints(hist, plot.title));
		plots << plot;
	}
	{
		QOPlot plot;
		plot.title = "CDF for the node degrees";
		plot.xlabel = "Node degree";
		plot.ylabel = "Fraction";
		QVector<QPointF> CDF = QOPlot::makeCDF(degrees, 0);
		plot.addData(QOPlotCurveData::fromPoints(CDF, plot.title));
		plots << plot;
	}
	{
		QOPlot plot;
		plot.title = "Histogram for the node degree";
		plot.xlabel = "Node degree";
		plot.ylabel = "Count";
		qreal binWidth;
		QVector<QPointF> hist = QOPlot::makeHistogram(degrees, binWidth);
		plot.addData(QOPlotCurveData::fromPoints(hist, plot.title));
		plots << plot;
	}
	{
		QOPlot plot;
		plot.title = "Frequency for the node degree";
		plot.xlabel = "Node degree";
		plot.ylabel = "Count";
		QVector<QPointF> hist = QOPlot::makePoints(degreeFreq);
		plot.addData(QOPlotCurveData::fromPoints(hist, plot.title));
		plots << plot;
	}
	{
		QOPlot plot;
		plot.title = "Frequency for the domain degree";
		plot.xlabel = "Domain degree";
		plot.ylabel = "Count";
		QVector<QPointF> hist = QOPlot::makePoints(domainDegreeFreq);
		plot.addData(QOPlotCurveData::fromPoints(hist, plot.title));
		plots << plot;
	}

	// GUI
	QWidget *scroll;
	QWidget *tab;
    scroll = ui->scrollPlotsWidgetContents;
    tab = ui->tabResults;

	foreach (QObject *obj, scroll->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(scroll);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(scroll);
	foreach (QOPlot plot, plots) {
		QOPlotWidget *plotWidget = new QOPlotWidget(accordion, 0, 450, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		plotWidget->setPlot(plot);
		accordion->addWidget(plot.title, plotWidget);
	}

	scroll->layout()->addWidget(accordion);
	accordion->expandAll();
	emit tabChanged(tab);

	// cleanup
	foreach (NetGraph *g, graphs) {
		delete g;
	}
}
