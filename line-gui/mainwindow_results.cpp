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

#include <netinet/in.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../tomo/tomodata.h"
#include "../line-gui/intervalmeasurements.h"
#include "flowevent.h"

Simulation::Simulation()
{
	dir = "(new)";
	graphName = "untitled";
}

Simulation::Simulation(QString dir) : dir(dir)
{
	// load data from simulation.txt
	QFile file(dir + "/" + "simulation.txt");
	if (file.open(QFile::ReadOnly)) {
		// file opened successfully
		QTextStream t(&file);
		while (true) {
			QString line = t.readLine().trimmed();
			if (line.isNull())
				break;
			if (line.contains('=')) {
				QStringList tokens = line.split('=');
				if (tokens.count() != 2)
					continue;

				QString key = tokens.at(0).trimmed();
				QString val = tokens.at(1).trimmed();

				if (key == "graph") {
					graphName = val;
				}
			}
		}
		file.close();
	}
}

void MainWindow::doReloadTopologyList(QString)
{
}

void MainWindow::doReloadSimulationList(QString testId)
{
	QString currentEntry;
	if (currentSimulation >= 0) {
		currentEntry = simulations[currentSimulation].dir;
	}

	simulations.clear();
	ui->cmbSimulation->clear();

	QString topology;
	if (currentTopology >= 0) {
		topology = ui->cmbTopologies->itemText(currentTopology);
	} else {
		topology = "(untitled)";
	}

	// dummy entry for new sim
	{
		Simulation s;
		s.dir = "(new)";
		s.graphName = topology;
		simulations << s;
		ui->cmbSimulation->addItem(s.dir);
	}

	QDir dir;
	dir.setFilter(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks);
	dir.setSorting(QDir::Name);
	QFileInfoList list = dir.entryInfoList();
	for (int i = 0; i < list.size(); ++i) {
		QFileInfo fileInfo = list.at(i);

		if (fileInfo.fileName().startsWith("."))
			continue;

		if (!QFile::exists(fileInfo.fileName() + "/" + "simulation.txt"))
			continue;

		Simulation s (fileInfo.fileName());
		if (s.graphName == topology) {
			simulations << s;
			ui->cmbSimulation->addItem(s.dir);
		}
	}

	bool found = false;
	if (!testId.isEmpty()) {
		for (int i = 0; i < simulations.count(); i++) {
			if (simulations[i].dir == testId) {
				ui->cmbSimulation->setCurrentIndex(i);
				found = true;
				break;
			}
		}
	}
	if (!found) {
		for (int i = 0; i < simulations.count(); i++) {
			if (simulations[i].dir == currentEntry) {
				ui->cmbSimulation->setCurrentIndex(i);
				found = true;
				break;
			}
		}
	}
	editor->setReadOnly(ui->cmbSimulation->currentIndex() > 0);
}

void MainWindow::loadSimulation()
{
	//QMutexLockerDbg locker(editor->graph()->mutex, __FUNCTION__); Q_UNUSED(locker);
	ui->labelCurrentSimulation->setText("");
	if (currentSimulation <= 0)
		return;
	if (!editor->loadGraph(simulations[currentSimulation].dir + "/" + simulations[currentSimulation].graphName + ".graph"))
		return;
	ui->labelCurrentSimulation->setText(simulations[currentSimulation].dir);

	{
		QString paramsFileName = simulations[currentSimulation].dir + "/run-params.data";
		RunParams runParams;
		if (loadRunParams(runParams, paramsFileName)) {
			// TODO maybe save the current ones? that is tricky...
			setUIToRunParams(runParams);
		}
	}

	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	const int textHeight = 400;
	const int tableHeight = 400;
	const int graphHeight = 300;

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);

	accordion->addLabel("Emulator");
	// emulator.out
	{
		QString content;
		QString fileName = simulations[currentSimulation].dir + "/" + "emulator.out";
		if (!readFile(fileName, content, true)) {
			emit logError(ui->txtBatch, QString("Failed to open file %1").arg(fileName));
		}
		QTextEdit *txt = new QTextEdit();
        QFont font("Liberation Mono");
        font.setStyleHint(QFont::Monospace);
        txt->setFont(font);
		txt->setFontPointSize(9);
		txt->setPlainText(content);
		txt->setMinimumHeight(textHeight);
		accordion->addWidget("Emulator output", txt);
	}
	// emulator.err
	{
		QString content;
		QString fileName = simulations[currentSimulation].dir + "/" + "emulator.err";
		if (!readFile(fileName, content, true)) {
			emit logError(ui->txtBatch, QString("Failed to open file %1").arg(fileName));
		}
		QTextEdit *txt = new QTextEdit();
        QFont font("Liberation Mono");
        font.setStyleHint(QFont::Monospace);
        txt->setFont(font);
        txt->setFontPointSize(9);
        txt->setPlainText(content);
		txt->setMinimumHeight(textHeight);
		accordion->addWidget("Emulator stderr", txt);
	}
    {
        QString content;
        QString fileName = simulations[currentSimulation].dir + "/" + "edgestats.txt";
        if (!readFile(fileName, content, true)) {
			emit logError(ui->txtBatch, QString("Failed to open file %1").arg(fileName));
        }
        QTextEdit *txt = new QTextEdit();
        txt->setPlainText(content);
		txt->setMinimumHeight(textHeight);
        accordion->addWidget("Link statistics", txt);
    }

	accordion->addLabel("Graph");
	{
		QString content;
		QString fileName = simulations[currentSimulation].dir + "/" + "graph.txt";
		if (!readFile(fileName, content, true)) {
			content = editor->graph()->toText();
			readFile(fileName, content, true);
		}
		QTextEdit *txt = new QTextEdit();
		txt->setPlainText(content);
		txt->setMinimumHeight(textHeight);
		accordion->addWidget("Graph", txt);
	}

	accordion->addLabel("Transmission records");
	{
		TomoData tomoData;
		QString fileName = simulations[currentSimulation].dir + "/" + "tomo-records.dat";
		if (!tomoData.load(fileName)) {
			emit logError(ui->txtBatch, QString("Failed to open file %1").arg(fileName));
		}

		QString content;
		content += QString("Number of paths: %1\n").arg(tomoData.m);
		content += QString("Number of edges: %1\n").arg(tomoData.n);
		content += QString("Simulation time (s): %1\n").arg((tomoData.tsMax - tomoData.tsMin) / 1.0e9);
		content += QString("Transmission rates for paths:");
		foreach (qreal v, tomoData.y) {
			content += QString(" %1").arg(v);
		}
		content += "\n";
		content += QString("Transmission rates for edges:");
		foreach (qreal v, tomoData.xmeasured) {
			content += QString(" %1").arg(v);
		}
		content += "\n";

		QTextEdit *txt = new QTextEdit();
        QFont font("Liberation Mono");
        font.setStyleHint(QFont::Monospace);
        txt->setFont(font);
		txt->setFontPointSize(9);
		txt->setPlainText(content);
		txt->setMinimumHeight(textHeight);
		accordion->addWidget("Data", txt);
	}

	QList<qint32> interestingEdges;
	for (int i = 0; i < editor->graph()->edges.count(); i++) {
		if (!editor->graph()->edges[i].isNeutral()) {
			interestingEdges << i;
		}
	}
	{
		QStringList tokens = ui->txtResultsInterestingLinks->text().split(QRegExp("[,\\s]"), QString::SkipEmptyParts);
		foreach (QString token, tokens) {
			bool ok;
			int i = token.toInt(&ok);
			if (ok && i >= 0 && i < editor->graph()->edges.count()) {
				interestingEdges << (i - 1);
			}
		}
	}

    quint64 intervalSize = 0;
	accordion->addLabel("Interval transmission records");
	{
		ExperimentIntervalMeasurements experimentIntervalMeasurements;
		QString fileName = simulations[currentSimulation].dir + "/" + "interval-measurements.data";
		if (!experimentIntervalMeasurements.load(fileName)) {
			emit logError(ui->txtBatch, QString("Failed to open file %1").arg(fileName));
		} else {
			intervalSize = experimentIntervalMeasurements.intervalSize;

			QVector<bool> trueEdgeNeutrality(editor->graph()->edges.count());
			for (int i = 0; i < editor->graph()->edges.count(); i++) {
				trueEdgeNeutrality[i] = editor->graph()->edges[i].isNeutral();
			}

			QVector<bool> edgeIsHostEdge(editor->graph()->edges.count());
			for (int i = 0; i < editor->graph()->edges.count(); i++) {
				edgeIsHostEdge[i] = editor->graph()->nodes[editor->graph()->edges[i].source].nodeType == NETGRAPH_NODE_HOST ||
									editor->graph()->nodes[editor->graph()->edges[i].dest].nodeType == NETGRAPH_NODE_HOST;
			}

			QVector<int> pathTrafficClass(editor->graph()->paths.count());
			QHash<QPair<qint32, qint32>, qint32> pathEndpoints2index;
			QHash<qint32, QPair<qint32, qint32> > pathIndex2endpoints;
			for (int p = 0; p < editor->graph()->paths.count(); p++) {
				QPair<qint32, qint32> endpoints(editor->graph()->paths[p].source, editor->graph()->paths[p].dest);
				pathEndpoints2index[endpoints] = p;
				pathIndex2endpoints[p] = endpoints;
			}
			foreach (NetGraphConnection c, editor->graph()->connections) {
				QPair<qint32, qint32> endpoints = QPair<qint32, qint32>(c.dest, c.source);
				if (pathEndpoints2index.contains(endpoints)) {
					pathTrafficClass[pathEndpoints2index[endpoints]] = c.trafficClass;
				}
			}
			foreach (NetGraphConnection c, editor->graph()->connections) {
				QPair<qint32, qint32> endpoints = QPair<qint32, qint32>(c.source, c.dest);
				if (pathEndpoints2index.contains(endpoints)) {
					pathTrafficClass[pathEndpoints2index[endpoints]] = c.trafficClass;
				}
			}

			{
				QTableWidget *table = new QTableWidget();
				QStringList columnHeaders = QStringList() << "Edge" << "Type" << "Neutrality";
				for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
					columnHeaders << QString("Interval %1").arg(interval + 1);
				}
				table->setColumnCount(columnHeaders.count());
				table->setHorizontalHeaderLabels(columnHeaders);
				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0)
						continue;
					table->setRowCount(table->rowCount() + 1);

					QTableWidgetItem *cell;
					int col = 0;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, e + 1);
					cell->setData(Qt::DisplayRole, e + 1);
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					cell = new QTableWidgetItem();
					QString edgeType = edgeIsHostEdge[e] ? "Access" : "Internal";
					cell->setData(Qt::EditRole, edgeType);
					cell->setData(Qt::DisplayRole, edgeType);
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					cell = new QTableWidgetItem();
					QString neutrality = trueEdgeNeutrality[e] ? "Neutral" : "Non-neutral";
					cell->setData(Qt::EditRole, neutrality);
					cell->setData(Qt::DisplayRole, neutrality);
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
						cell = new QTableWidgetItem();
						qint64 numDropped = experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsDropped;
						qint64 numSent = experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsInFlight;
						if (numSent == 0) {
							cell->setData(Qt::EditRole, " ");
							cell->setData(Qt::DisplayRole, " ");
							cell->setData(Qt::ToolTipRole, QString());
						} else {
							qreal rate = 1.0 -
										 qreal(numDropped) /
										 qreal(numSent);
							cell->setData(Qt::EditRole, rate);
							cell->setData(Qt::DisplayRole, rate);
							cell->setData(Qt::ToolTipRole,
										  QString("%1 dropped out of %2")
										  .arg(numDropped)
										  .arg(numSent));
						}
						table->setItem(table->rowCount() - 1, col, cell);
						col++;
					}
				}

				table->setSortingEnabled(true);
				table->sortByColumn(0);
				table->setMinimumHeight(tableHeight);
				accordion->addWidget("Edge transmission rates", table);
			}

			for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
				if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0)
					continue;
				if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsDropped /
					qreal(experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight) < 0.001)
					continue;
				interestingEdges << e;
			}
			interestingEdges = interestingEdges.toSet().toList();
			qSort(interestingEdges);

			{
				// loss rate timeline
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					if (!interestingEdges.contains(e)) {
						if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0)
							continue;
						if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsDropped /
							qreal(experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight) < 0.01)
							continue;
					}

					QOPlotCurveData *lossRateData = new QOPlotCurveData();

					lossRateData->x.reserve(experimentIntervalMeasurements.numIntervals());
					lossRateData->y.reserve(experimentIntervalMeasurements.numIntervals());
					lossRateData->pointSymbol = "o";
					for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
						qreal loss;
						if (experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsInFlight > 0) {
							loss = qreal(experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsDropped) /
								   qreal(experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsInFlight);
						} else {
							loss = 0;
						}
						qreal t = interval * intervalSize;
						lossRateData->x << (t * 1.0e-9);
						lossRateData->y << (loss * 100.0);
					}

					QString title = QString("Loss for edge %1").arg(e + 1);
					QOPlotWidget *plot_lossRate = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_lossRate->plot.title = title;
					plot_lossRate->plot.xlabel = "Time (s)";
					plot_lossRate->plot.xSISuffix = true;
					plot_lossRate->plot.ylabel = "Loss (%)";
					plot_lossRate->plot.ySISuffix = false;
					plot_lossRate->plot.addData(lossRateData);
					plot_lossRate->plot.drag_y_enabled = false;
					plot_lossRate->plot.zoom_y_enabled = false;
					plot_lossRate->autoAdjustAxes();
					plot_lossRate->drawPlot();
					accordion->addWidget(title, plot_lossRate);
				}
			}

			{
				QTableWidget *table = new QTableWidget();
				QList<qreal> thresholds = QList<qreal>() << 0.100 << 0.050 << 0.040 << 0.030 << 0.025 << 0.020 << 0.015 << 0.010 << 0.005 << 0.0025;
				QStringList columnHeaders = QStringList() << "Edge" << "Neutrality";
				foreach (qreal threshold, thresholds) {
					columnHeaders << QString("%1%").arg(threshold * 100.0, 0, 'f', 1);
				}

				table->setColumnCount(columnHeaders.count());
				table->setHorizontalHeaderLabels(columnHeaders);
				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				// experimentIntervalMeasurements.intervalMeasurements[e].perPathEdgeMeasurements[e]
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0)
						continue;

					for (int p = -1; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (p >= 0 &&
							experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;
						table->setRowCount(table->rowCount() + 1);

						QTableWidgetItem *cell;
						int col = 0;

						cell = new QTableWidgetItem();
						cell->setData(Qt::EditRole, p < 0 ? QString("%1").arg(e + 1) :
															QString("%1 Path %2 (%3)").arg(e + 1).arg(p + 1).arg(pathTrafficClass[p]));
						cell->setData(Qt::DisplayRole, p < 0 ? QString("%1").arg(e + 1) :
															   QString("%1 Path %2 (%3)").arg(e + 1).arg(p + 1).arg(pathTrafficClass[p]));
						table->setItem(table->rowCount() - 1, col, cell);
						col++;

						cell = new QTableWidgetItem();
						QString neutrality = trueEdgeNeutrality[e] ? "Neutral" : "Non-neutral";
						cell->setData(Qt::EditRole, neutrality);
						cell->setData(Qt::DisplayRole, neutrality);
						table->setItem(table->rowCount() - 1, col, cell);
						col++;

						foreach (qreal threshold, thresholds) {
							qreal lossyProbability = 0;
							for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
								if (p < 0) {
									if (experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsInFlight > 0) {
										qreal loss = qreal(experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsDropped) /
													 qreal(experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsInFlight);
										if (loss >= threshold) {
											lossyProbability += 1.0;
										}
									}
								} else {
									QInt32Pair ep = QInt32Pair(e, p);
									if (experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight > 0) {
										qreal loss = qreal(experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsDropped) /
													 qreal(experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight);
										if (loss >= threshold) {
											lossyProbability += 1.0;
										}
									}
								}
							}
							lossyProbability /= qMax(1, experimentIntervalMeasurements.numIntervals());
							cell = new QTableWidgetItem();
							cell->setData(Qt::EditRole, lossyProbability);
							cell->setData(Qt::DisplayRole, lossyProbability);
							table->setItem(table->rowCount() - 1, col, cell);
							col++;
						}
					}
				}

				table->setSortingEnabled(true);
				table->sortByColumn(0, Qt::AscendingOrder);
				table->setMinimumHeight(tableHeight);
                accordion->addWidget("Edge congestion probability (by threshold)", table);
			}

			{
				const qint32 bottleneck = 0;
				QList<QInt32Pair> pathPairs;
				for (int p1 = 0; p1 < editor->graph()->paths.count(); p1++) {
					QSet<NetGraphEdge> edges1 = editor->graph()->paths[p1].edgeSet;
					for (int p2 = p1 + 1; p2 < editor->graph()->paths.count(); p2++) {
						QSet<NetGraphEdge> edges2 = editor->graph()->paths[p2].edgeSet;
						edges2 = edges2.intersect(edges1);
						if (edges2.count() == 1 && edges2.begin()->index == bottleneck) {
							pathPairs << QInt32Pair(p1, p2);
						}
					}
				}

				QTableWidget *table = new QTableWidget();
				QList<qreal> thresholds = QList<qreal>() << 0.100 << 0.050 << 0.040 << 0.030 << 0.025 << 0.020 << 0.015 << 0.010 << 0.005 << 0.0025;
				QStringList columnHeaders = QStringList() << "Path pair" << "Class";
				foreach (qreal threshold, thresholds) {
					columnHeaders << QString("%1%").arg(threshold * 100.0, 0, 'f', 1);
				}

				table->setColumnCount(columnHeaders.count());
				table->setHorizontalHeaderLabels(columnHeaders);
				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				// experimentIntervalMeasurements.intervalMeasurements[e].perPathEdgeMeasurements[e]
				foreach (QInt32Pair pathPair, pathPairs) {
					int p1 = pathPair.first;
					int p2 = pathPair.second;
					Q_ASSERT_FORCE(p1 < experimentIntervalMeasurements.numPaths);
					Q_ASSERT_FORCE(p2 < experimentIntervalMeasurements.numPaths);
					table->setRowCount(table->rowCount() + 1);

					QTableWidgetItem *cell;
					int col = 0;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, QString("(%1, %2)").arg(p1 + 1).arg(p2 + 1));
					cell->setData(Qt::DisplayRole, QString("(%1, %2)").arg(p1 + 1).arg(p2 + 1));
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					cell = new QTableWidgetItem();
					QString neutrality = QString("%1-%2").arg(pathTrafficClass[p1] + 1).arg(pathTrafficClass[p2] + 1);
					cell->setData(Qt::EditRole, neutrality);
					cell->setData(Qt::DisplayRole, neutrality);
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					foreach (qreal threshold, thresholds) {
						qreal lossyProbability1 = 0;
						qreal lossyProbability2 = 0;
						qreal lossyProbability12 = 0;
						for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
							qreal loss1 = 1.0 - experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p1].successRate();
							if (loss1 >= threshold) {
								lossyProbability1 += 1.0;
							}
							qreal loss2 = 1.0 - experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p2].successRate();
							if (loss2 >= threshold) {
								lossyProbability2 += 1.0;
							}
							if (loss1 >= threshold && loss2 >= threshold) {
								lossyProbability12 += 1.0;
							}
						}
						lossyProbability1 /= qMax(1, experimentIntervalMeasurements.numIntervals());
						lossyProbability2 /= qMax(1, experimentIntervalMeasurements.numIntervals());
						lossyProbability12 /= qMax(1, experimentIntervalMeasurements.numIntervals());

						qreal pLinkLossyComputed = 1.0 - (1.0 - lossyProbability1) * (1.0 - lossyProbability2) /
												   qMax(0.0001, 1.0 - lossyProbability12);

						cell = new QTableWidgetItem();
						cell->setData(Qt::EditRole, pLinkLossyComputed);
						cell->setData(Qt::DisplayRole, pLinkLossyComputed);
						table->setItem(table->rowCount() - 1, col, cell);
						col++;
					}
				}

				table->setSortingEnabled(true);
				table->sortByColumn(0, Qt::AscendingOrder);
				table->setMinimumHeight(tableHeight);
                accordion->addWidget("Edge computed congestion probability (by threshold)", table);
			}

			{
				QTableWidget *table = new QTableWidget();
				QStringList columnHeaders = QStringList() << "Path" << "Neutrality group";
				for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
					columnHeaders << QString("Interval %1").arg(interval + 1);
				}
				table->setColumnCount(columnHeaders.count());
				table->setHorizontalHeaderLabels(columnHeaders);
				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
					if (experimentIntervalMeasurements.globalMeasurements.pathMeasurements[p].numPacketsInFlight == 0)
						continue;
					table->setRowCount(table->rowCount() + 1);

					QTableWidgetItem *cell;
					int col = 0;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, p + 1);
					cell->setData(Qt::DisplayRole, p + 1);
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, pathTrafficClass[p] + 1);
					cell->setData(Qt::DisplayRole, pathTrafficClass[p] + 1);
					table->setItem(table->rowCount() - 1, col, cell);
					col++;

					for (int interval = 0; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
						cell = new QTableWidgetItem();
						if (experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p].numPacketsInFlight == 0) {
							cell->setData(Qt::EditRole, " ");
							cell->setData(Qt::DisplayRole, " ");
						} else {
							qint64 numDropped = experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p].numPacketsDropped;
							qint64 numSent = experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p].numPacketsInFlight;
							qreal rate = 1.0 -
										 qreal(numDropped) /
										 qreal(numSent);
							cell->setData(Qt::EditRole, rate);
							cell->setData(Qt::DisplayRole, rate);
							cell->setData(Qt::ToolTipRole,
										  QString("%1 dropped out of %2")
										  .arg(numDropped)
										  .arg(numSent));
						}
						table->setItem(table->rowCount() - 1, col, cell);
						col++;
					}
				}

				table->setSortingEnabled(true);
				table->sortByColumn(0);
				table->setMinimumHeight(tableHeight);
				accordion->addWidget("Path transmission rates", table);
			}

			{
				QTableWidget *table = new QTableWidget();
				QStringList columnHeaders = QStringList();
				columnHeaders << "Path";
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					columnHeaders << QString("Edge %1").arg(e + 1);
				}
				table->setColumnCount(experimentIntervalMeasurements.numEdges + 1);
				table->setHorizontalHeaderLabels(columnHeaders);

				table->setRowCount(experimentIntervalMeasurements.numPaths + 1);
				QStringList rowHeaders = QStringList();
				for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
					rowHeaders << QString("Path %1 (%2 -> %3, group %4)").
								  arg(p + 1).
								  arg(pathIndex2endpoints[p].first + 1).
								  arg(pathIndex2endpoints[p].second + 1).
								  arg(pathTrafficClass[p] + 1);
					QTableWidgetItem *cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rowHeaders.last());
					cell->setData(Qt::DisplayRole, rowHeaders.last());
					table->setItem(p, 0, cell);
				}

				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					int col = e + 1;
					if (!trueEdgeNeutrality[e]) {
						table->setColumnHidden(col, true);
						continue;
					}
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0) {
						table->setColumnHidden(col, true);
						continue;
					}
					bool allPathsGood = true;
					for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

						QTableWidgetItem *cell;

						cell = new QTableWidgetItem();
						qint64 numDropped = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped;
						qint64 numSent = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight;
						qreal rate = 1.0 -
									 qreal(numDropped) /
									 qreal(numSent);
						allPathsGood = allPathsGood && (rate == 1.0);
						cell->setData(Qt::EditRole, rate);
						cell->setData(Qt::DisplayRole, rate);
						cell->setData(Qt::ToolTipRole,
									  QString("%1 dropped out of %2")
									  .arg(numDropped)
									  .arg(numSent));
						table->setItem(p, col, cell);
					}
					table->setColumnHidden(col, allPathsGood);
				}

				table->setSortingEnabled(true);
				table->sortByColumn(1);
				table->setMinimumHeight(tableHeight);
				table->resizeColumnsToContents();
				accordion->addWidget("Per path edge transmission rates (neutral edges)", table);
			}

			{
				QTableWidget *table = new QTableWidget();
				QStringList columnHeaders = QStringList();
				columnHeaders << "Path";
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					columnHeaders << QString("Edge %1").arg(e + 1);
				}
				table->setColumnCount(experimentIntervalMeasurements.numEdges + 1);
				table->setHorizontalHeaderLabels(columnHeaders);

				table->setRowCount(experimentIntervalMeasurements.numPaths);
				QStringList rowHeaders = QStringList();
				for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
					rowHeaders << QString("Path %1 (%2 -> %3, group %4)").
								  arg(p + 1).
								  arg(pathIndex2endpoints[p].first + 1).
								  arg(pathIndex2endpoints[p].second + 1).
								  arg(pathTrafficClass[p] + 1);
					QTableWidgetItem *cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rowHeaders.last());
					cell->setData(Qt::DisplayRole, rowHeaders.last());
					table->setItem(p, 0, cell);
				}

				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					int col = e + 1;
					if (trueEdgeNeutrality[e]) {
						table->setColumnHidden(col, true);
						continue;
					}
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0) {
						table->setColumnHidden(col, true);
						continue;
					}
					bool allPathsGood = true;
					for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

						QTableWidgetItem *cell;

						cell = new QTableWidgetItem();
						qint64 numDropped = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped;
						qint64 numSent = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight;
						qreal rate = 1.0 -
									 qreal(numDropped) /
									 qreal(numSent);
						allPathsGood = allPathsGood && (rate == 1.0);
						cell->setData(Qt::EditRole, rate);
						cell->setData(Qt::DisplayRole, rate);
						cell->setData(Qt::ToolTipRole,
									  QString("%1 dropped out of %2")
									  .arg(numDropped)
									  .arg(numSent));
						table->setItem(p, col, cell);
					}
					table->setColumnHidden(col, allPathsGood);
				}

				table->setSortingEnabled(true);
				table->sortItems(1);
				table->setMinimumHeight(tableHeight);
				table->resizeColumnsToContents();
				accordion->addWidget("Per path edge transmission rates (non-neutral edges)", table);
			}

			{
				QTableWidget *table = new QTableWidget();
				QStringList columnHeaders = QStringList() << "Edge" << "Policy" <<
															 "Min" <<
															 "Max" <<
															 "Avg" <<
															 "Max-min" <<
															 "Variance" <<
															 "Min (1)" <<
															 "Max (1)" <<
															 "Avg (1)" <<
															 "Max-min (1)" <<
															 "Variance (1)" <<
															 "Min (2)" <<
															 "Max (2)" <<
															 "Avg (2)" <<
															 "Max-min (2)" <<
															 "Variance (2)";
				table->setColumnCount(columnHeaders.count());
				table->setHorizontalHeaderLabels(columnHeaders);

				table->setRowCount(experimentIntervalMeasurements.numEdges);

				table->setEditTriggers(QAbstractItemView::NoEditTriggers);

				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					QTableWidgetItem *cell;
					int col = 0;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, e + 1);
					cell->setData(Qt::DisplayRole, e + 1);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					QString neutrality = trueEdgeNeutrality[e] ? "Neutral" : "Non-neutral";
					cell->setData(Qt::EditRole, neutrality);
					cell->setData(Qt::DisplayRole, neutrality);
					table->setItem(e, col, cell);
					col++;

					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0) {
						table->setRowHidden(e, true);
						continue;
					}
					qreal rateMin = 1.0e99;
					qreal rateMax = 0.0;
					qreal numRates = 0;
					qreal sumRates = 0;

					qreal rateMin0 = 1.0e99;
					qreal rateMax0 = 0.0;
					qreal numRates0 = 0;
					qreal sumRates0 = 0;

					qreal rateMin1 = 1.0e99;
					qreal rateMax1 = 0.0;
					qreal numRates1 = 0;
					qreal sumRates1 = 0;
					for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

						qreal rate = 1.0 -
									 qreal(experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped) /
									 qreal(experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight);
						rateMin = qMin(rateMin, rate);
						rateMax = qMax(rateMax, rate);
						sumRates += rate;
						numRates += 1;

						if (pathTrafficClass[p] == 0) {
							rateMin0 = qMin(rateMin0, rate);
							rateMax0 = qMax(rateMax0, rate);
							sumRates0 += rate;
							numRates0 += 1;
						} else if (pathTrafficClass[p] == 1) {
							rateMin1 = qMin(rateMin1, rate);
							rateMax1 = qMax(rateMax1, rate);
							sumRates1 += rate;
							numRates1 += 1;
						}
					}
					qreal avgRate = sumRates/numRates;
					qreal avgRate0 = numRates0 > 0 ? sumRates0/numRates0 : 0;
					qreal avgRate1 = numRates1 > 0 ? sumRates1/numRates1 : numRates1;

					qreal varRate = 0;
					qreal varRate0 = 0;
					qreal varRate1 = 0;
					for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

						qreal rate = 1.0 -
									 qreal(experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped) /
									 qreal(experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight);
						varRate += (rate - avgRate) * (rate - avgRate) / numRates;

						if (pathTrafficClass[p] == 0) {
							varRate0 += (rate - avgRate0) * (rate - avgRate0) / numRates0;
						} else if (pathTrafficClass[p] == 1) {
							varRate1 += (rate - avgRate1) * (rate - avgRate1) / numRates1;
						}
					}

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMin);
					cell->setData(Qt::DisplayRole, rateMin);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMax);
					cell->setData(Qt::DisplayRole, rateMax);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, avgRate);
					cell->setData(Qt::DisplayRole, avgRate);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMax - rateMin);
					cell->setData(Qt::DisplayRole, rateMax - rateMin);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, varRate);
					cell->setData(Qt::DisplayRole, varRate);
					table->setItem(e, col, cell);
					col++;

					// group 1
					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMin0);
					cell->setData(Qt::DisplayRole, rateMin0);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMax0);
					cell->setData(Qt::DisplayRole, rateMax0);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, avgRate0);
					cell->setData(Qt::DisplayRole, avgRate0);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMax0 - rateMin0);
					cell->setData(Qt::DisplayRole, rateMax0 - rateMin0);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, varRate0);
					cell->setData(Qt::DisplayRole, varRate0);
					table->setItem(e, col, cell);
					col++;

					// group 2
					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMin1);
					cell->setData(Qt::DisplayRole, rateMin1);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMax1);
					cell->setData(Qt::DisplayRole, rateMax1);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, avgRate1);
					cell->setData(Qt::DisplayRole, avgRate1);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, rateMax1 - rateMin1);
					cell->setData(Qt::DisplayRole, rateMax1 - rateMin1);
					table->setItem(e, col, cell);
					col++;

					cell = new QTableWidgetItem();
					cell->setData(Qt::EditRole, varRate1);
					cell->setData(Qt::DisplayRole, varRate1);
					table->setItem(e, col, cell);
					col++;
				}

				table->setSortingEnabled(true);
				table->sortByColumn(0);
				table->setMinimumHeight(tableHeight);
				accordion->addWidget("Edge neutrality", table);
			}
		}
	}

	accordion->addLabel("Application output");
	{
		QString content;
		{
			QDir dir;
			dir.setPath(simulations[currentSimulation].dir);
			dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
			dir.setSorting(QDir::Name);
			QFileInfoList list = dir.entryInfoList();
			for (int i = 0; i < list.size(); ++i) {
				QFileInfo fileInfo = list.at(i);

				if (fileInfo.fileName().startsWith("connection_") || fileInfo.fileName().startsWith("multiplexer")) {
					QString text;
					readFile(fileInfo.filePath(), text, true);
					content += QString("%1:\n%2\n").arg(fileInfo.fileName()).arg(text);
				}
			}
		}
		content = content.trimmed();

		QTextEdit *txt = new QTextEdit();
        QFont font("Liberation Mono");
        font.setStyleHint(QFont::Monospace);
        txt->setFont(font);
		txt->setFontPointSize(9);
		txt->setPlainText(content);
		txt->setMinimumHeight(textHeight);
		accordion->addWidget("Text", txt);
	}

	quint64 tsMin = ULONG_LONG_MAX;
	accordion->addLabel("Link timelines");
	{
		//for (int iEdge = 0; iEdge < editor->graph()->edges.count(); iEdge++) {
		foreach (qint32 iEdge, interestingEdges) {
			for (int queue = -1; queue < editor->graph()->edges[iEdge].queueCount; queue++) {

				QString fileName = simulations[currentSimulation].dir + "/" + QString("timelines-edge-%1%2.dat").
								   arg(iEdge).
								   arg(queue >= 0 ? QString("-queue-%1").arg(queue) : QString(""));
				QFile file(fileName);
				if (!file.open(QIODevice::ReadOnly)) {
					emit logError(ui->txtBatch, QString("Failed to open file %1").arg(fileName));
				}
				QDataStream in(&file);
				in.setVersion(QDataStream::Qt_4_0);

				quint64 tsMax, tsample;
				quint64 rate_Bps;
				qint32 delay_ms;
				quint64 qcapacity;

				QVector<quint64> vector_timestamp;
				QVector<quint64> vector_arrivals_p;
				QVector<quint64> vector_arrivals_B;
				QVector<quint64> vector_qdrops_p;
				QVector<quint64> vector_qdrops_B;
				QVector<quint64> vector_rdrops_p;
				QVector<quint64> vector_rdrops_B;
				QVector<quint64> vector_queue_sampled;
				QVector<quint64> vector_queue_avg;
				QVector<quint64> vector_queue_max;
				QVector<quint64> vector_queue_numflows;

				// edge timeline range
				in >> tsMin;
				in >> tsMax;

				// edge properties
				in >> tsample;
				in >> rate_Bps;
				in >> delay_ms;
				in >> qcapacity;

				in >> vector_timestamp;
				in >> vector_arrivals_p;
				in >> vector_arrivals_B;
				in >> vector_qdrops_p;
				in >> vector_qdrops_B;
				in >> vector_rdrops_p;
				in >> vector_rdrops_B;
				in >> vector_queue_sampled;
				in >> vector_queue_avg;
				in >> vector_queue_max;
				in >> vector_queue_numflows;

				if (vector_timestamp.isEmpty() || tsMin > tsMax)
					continue;

                {
					quint64 nelem = (tsMax - tsMin)/tsample + (((tsMax - tsMin)%tsample) ? 1 : 0);

					if (vector_timestamp.isEmpty())
						continue;

					if (queue < 0 && tsample == intervalSize) {
						// dump some data to txt
						QString strNFlows;
						foreach (quint64 v, vector_queue_numflows) {
							strNFlows += QString("%1\n").arg(v);
						}
						saveFile(simulations[currentSimulation].dir + "/" + QString("nflows-edge-%1.txt").arg(iEdge + 1), strNFlows);
					}

					accordion->addLabel(QString("Link timelines for edge %1, %2sampling period %3 s").
										arg(iEdge + 1).
										arg(queue >= 0 ? QString("queue %1, ").arg(queue) : QString("")).
										arg(tsample * 1.0e-9));

					QOPlotCurveData *arrivals_p = new QOPlotCurveData;
					QOPlotCurveData *arrivals_B = new QOPlotCurveData;
					QOPlotCurveData *qdrops_p = new QOPlotCurveData;
					QOPlotCurveData *qdrops_B = new QOPlotCurveData;
					QOPlotCurveData *rdrops_p = new QOPlotCurveData;
					QOPlotCurveData *rdrops_B = new QOPlotCurveData;
					QOPlotCurveData *queue_sampled = new QOPlotCurveData;
					QOPlotCurveData *queue_avg = new QOPlotCurveData;
					QOPlotCurveData *queue_max = new QOPlotCurveData;
					QOPlotCurveData *queue_numflows = new QOPlotCurveData;
					QOPlotCurveData *lossRate = new QOPlotCurveData;

					QString title = QString("Timeline for edge %1 -> %2, %3sampling period %4 s").
							arg(editor->graph()->edges[iEdge].source + 1).
							arg(editor->graph()->edges[iEdge].dest + 1).
							arg(queue >= 0 ? QString("queue %1, ").arg(queue) : QString("")).
							arg(tsample * 1.0e-9);

					arrivals_p->x.reserve(nelem);
					arrivals_p->y.reserve(nelem);
					arrivals_p->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						arrivals_p->x << (vector_timestamp.at(i) * 1.0e-9);
						arrivals_p->y << vector_arrivals_p.at(i);
					}

					QString subtitle = " - Arrivals (packets)";
					QOPlotWidget *plot_arrivals_p = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_arrivals_p->plot.title = title + subtitle;
					plot_arrivals_p->plot.xlabel = "Time (s)";
					plot_arrivals_p->plot.xSISuffix = true;
					plot_arrivals_p->plot.ylabel = "Packets";
					plot_arrivals_p->plot.ySISuffix = true;
					plot_arrivals_p->plot.addData(arrivals_p);
					plot_arrivals_p->plot.drag_y_enabled = false;
					plot_arrivals_p->plot.zoom_y_enabled = false;
					plot_arrivals_p->autoAdjustAxes();
					plot_arrivals_p->drawPlot();
					accordion->addWidget(title + subtitle, plot_arrivals_p);

					arrivals_B->x = arrivals_p->x;
					arrivals_B->y.reserve(nelem);
					arrivals_B->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						arrivals_B->y << vector_arrivals_B.at(i) * 8;
					}

					subtitle = " - Arrivals (bits)";
					QOPlotWidget *plot_arrivals_B = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_arrivals_B->plot.title = title + subtitle;
					plot_arrivals_B->plot.xlabel = "Time (s)";
					plot_arrivals_B->plot.xSISuffix = true;
					plot_arrivals_B->plot.ylabel = "Bits";
					plot_arrivals_B->plot.ySISuffix = true;
					plot_arrivals_B->plot.addData(arrivals_B);
					plot_arrivals_B->plot.drag_y_enabled = false;
					plot_arrivals_B->plot.zoom_y_enabled = false;
					plot_arrivals_B->autoAdjustAxes();
					plot_arrivals_B->drawPlot();
					accordion->addWidget(title + subtitle, plot_arrivals_B);

					qdrops_p->x = arrivals_p->x;
					qdrops_p->y.reserve(nelem);
					qdrops_p->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						qdrops_p->y << vector_qdrops_p.at(i);
					}

					subtitle = " - Queue drops (packets)";
					QOPlotWidget *plot_qdrops_p = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_qdrops_p->plot.title = title + subtitle;
					plot_qdrops_p->plot.xlabel = "Time (s)";
					plot_qdrops_p->plot.xSISuffix = true;
					plot_qdrops_p->plot.ylabel = "Packets";
					plot_qdrops_p->plot.ySISuffix = true;
					plot_qdrops_p->plot.addData(qdrops_p);
					plot_qdrops_p->plot.drag_y_enabled = false;
					plot_qdrops_p->plot.zoom_y_enabled = false;
					plot_qdrops_p->autoAdjustAxes();
					plot_qdrops_p->drawPlot();
					accordion->addWidget(title + subtitle, plot_qdrops_p);

					qdrops_B->x = arrivals_p->x;
					qdrops_B->y.reserve(nelem);
					qdrops_B->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						qdrops_B->y << vector_qdrops_B.at(i) * 8;
					}

					subtitle = " - Queue drops (bits)";
					QOPlotWidget *plot_qdrops_B = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_qdrops_B->plot.title = title + subtitle;
					plot_qdrops_B->plot.xlabel = "Time (s)";
					plot_qdrops_B->plot.xSISuffix = true;
					plot_qdrops_B->plot.ylabel = "Bits";
					plot_qdrops_B->plot.ySISuffix = true;
					plot_qdrops_B->plot.addData(qdrops_B);
					plot_qdrops_B->plot.drag_y_enabled = false;
					plot_qdrops_B->plot.zoom_y_enabled = false;
					plot_qdrops_B->autoAdjustAxes();
					plot_qdrops_B->drawPlot();
					accordion->addWidget(title + subtitle, plot_qdrops_B);

					rdrops_p->x = arrivals_p->x;
					rdrops_p->y.reserve(nelem);
					rdrops_p->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						rdrops_p->y << vector_rdrops_p.at(i);
					}

					subtitle = " - Random drops (packets)";
					QOPlotWidget *plot_rdrops_p = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_rdrops_p->plot.title = title + subtitle;
					plot_rdrops_p->plot.xlabel = "Time (s)";
					plot_rdrops_p->plot.xSISuffix = true;
					plot_rdrops_p->plot.ylabel = "Packets";
					plot_rdrops_p->plot.ySISuffix = true;
					plot_rdrops_p->plot.addData(rdrops_p);
					plot_rdrops_p->plot.drag_y_enabled = false;
					plot_rdrops_p->plot.zoom_y_enabled = false;
					plot_rdrops_p->autoAdjustAxes();
					plot_rdrops_p->drawPlot();
					accordion->addWidget(title + subtitle, plot_rdrops_p);

					rdrops_B->x = arrivals_p->x;
					rdrops_B->y.reserve(nelem);
					rdrops_B->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						rdrops_B->y << vector_rdrops_B.at(i) * 8;
					}

					subtitle = " - Random drops (bits)";
					QOPlotWidget *plot_rdrops_B = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_rdrops_B->plot.title = title + subtitle;
					plot_rdrops_B->plot.xlabel = "Time (s)";
					plot_rdrops_B->plot.xSISuffix = true;
					plot_rdrops_B->plot.ylabel = "Bits";
					plot_rdrops_B->plot.ySISuffix = true;
					plot_rdrops_B->plot.addData(rdrops_B);
					plot_rdrops_B->plot.drag_y_enabled = false;
					plot_rdrops_B->plot.zoom_y_enabled = false;
					plot_rdrops_B->autoAdjustAxes();
					plot_rdrops_B->drawPlot();
					accordion->addWidget(title + subtitle, plot_rdrops_B);

					queue_sampled->x = arrivals_p->x;
					queue_sampled->y.reserve(nelem);
					queue_sampled->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						queue_sampled->y << vector_queue_sampled.at(i) * 8;
					}

					subtitle = " - Queue size (sampled)";
					QOPlotWidget *plot_queue_sampled = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_queue_sampled->plot.title = title + subtitle;
					plot_queue_sampled->plot.xlabel = "Time (s)";
					plot_queue_sampled->plot.xSISuffix = true;
					plot_queue_sampled->plot.ylabel = "Bits";
					plot_queue_sampled->plot.ySISuffix = true;
					plot_queue_sampled->plot.addData(queue_sampled);
					plot_queue_sampled->plot.drag_y_enabled = false;
					plot_queue_sampled->plot.zoom_y_enabled = false;
					plot_queue_sampled->autoAdjustAxes();
					plot_queue_sampled->drawPlot();
					accordion->addWidget(title + subtitle, plot_queue_sampled);

					queue_max->x = arrivals_p->x;
					queue_max->y.reserve(nelem);
					queue_max->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						queue_max->y << vector_queue_max.at(i) * 8;
					}

					subtitle = " - Queue size (interval maximums)";
					QOPlotWidget *plot_queue_max = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_queue_max->plot.title = title + subtitle;
					plot_queue_max->plot.xlabel = "Time (s)";
					plot_queue_max->plot.xSISuffix = true;
					plot_queue_max->plot.ylabel = "Bits";
					plot_queue_max->plot.ySISuffix = true;
					plot_queue_max->plot.addData(queue_max);
					plot_queue_max->plot.drag_y_enabled = false;
					plot_queue_max->plot.zoom_y_enabled = false;
					plot_queue_max->autoAdjustAxes();
					plot_queue_max->drawPlot();
					accordion->addWidget(title + subtitle, plot_queue_max);

					queue_avg->x = arrivals_p->x;
					queue_avg->y.reserve(nelem);
					queue_avg->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						queue_avg->y << vector_queue_avg.at(i) * 8;
					}

                    subtitle = " - Queue size (interval average)";
					QOPlotWidget *plot_queue_avg = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_queue_avg->plot.title = title + subtitle;
					plot_queue_avg->plot.xlabel = "Time (s)";
					plot_queue_avg->plot.xSISuffix = true;
					plot_queue_avg->plot.ylabel = "Bits";
					plot_queue_avg->plot.ySISuffix = true;
					plot_queue_avg->plot.addData(queue_avg);
					plot_queue_avg->plot.drag_y_enabled = false;
					plot_queue_avg->plot.zoom_y_enabled = false;
					plot_queue_avg->autoAdjustAxes();
					plot_queue_avg->drawPlot();
					accordion->addWidget(title + subtitle, plot_queue_avg);

					queue_numflows->x = arrivals_p->x;
					queue_numflows->y.reserve(nelem);
					queue_numflows->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						queue_numflows->y << vector_queue_numflows.at(i);
					}

					subtitle = " - Number of flows";
					QOPlotWidget *plot_numflows = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_numflows->plot.title = title + subtitle;
					plot_numflows->plot.xlabel = "Time (s)";
					plot_numflows->plot.xSISuffix = true;
					plot_numflows->plot.ylabel = "Flows";
					plot_numflows->plot.ySISuffix = true;
					plot_numflows->plot.addData(queue_numflows);
					plot_numflows->plot.drag_y_enabled = false;
					plot_numflows->plot.zoom_y_enabled = false;
					plot_numflows->autoAdjustAxes();
					plot_numflows->drawPlot();
					accordion->addWidget(title + subtitle, plot_numflows);

					lossRate->x = arrivals_p->x;
					lossRate->y.reserve(nelem);
					lossRate->pointSymbol = "o";
					for (int i = 0; i < vector_timestamp.count(); i++) {
						lossRate->y << ((vector_arrivals_p.at(i) > 1.0e-9) ? (100.0 * vector_qdrops_p.at(i) / vector_arrivals_p.at(i)) :
																			 0.0);
					}

					subtitle = " - Packet loss (%)";
					QOPlotWidget *plot_lossRate = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_lossRate->plot.title = title + subtitle;
					plot_lossRate->plot.xlabel = QString("Time (s) - sampled at %1s").arg(tsample / 1.0e9);
					plot_lossRate->plot.xSISuffix = true;
					plot_lossRate->plot.ylabel = "Packet loss (%)";
					plot_lossRate->plot.ySISuffix = false;
					plot_lossRate->plot.addData(lossRate);
					plot_lossRate->plot.drag_y_enabled = false;
					plot_lossRate->plot.zoom_y_enabled = false;
					plot_lossRate->autoAdjustAxes();
					plot_lossRate->drawPlot();
					accordion->addWidget(title + subtitle, plot_lossRate);

					subtitle = " - Packet loss (%, CDF)";
					QOPlotWidget *plot_lossRateCDF = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_lossRateCDF->plot.title = title + subtitle;
					plot_lossRateCDF->plot.xlabel = "Packet loss (%)";
					plot_lossRateCDF->plot.xSISuffix = false;
					plot_lossRateCDF->plot.ylabel = "Fraction";
					plot_lossRateCDF->plot.ySISuffix = false;
					plot_lossRateCDF->plot.addData(QOPlotCurveData::fromPoints(QOPlot::makeCDFSampled(lossRate->y.toList(), 0, 100), "CDF"));
					plot_lossRateCDF->plot.drag_y_enabled = false;
					plot_lossRateCDF->plot.zoom_y_enabled = false;
					plot_lossRateCDF->autoAdjustAxes();
					plot_lossRateCDF->drawPlot();
					accordion->addWidget(title + subtitle, plot_lossRateCDF);
				}
			}
		}
	}

	accordion->addLabel("Flow timelines");
	{
		const int maxFlows = 200;
		QString fileName = simulations[currentSimulation].dir + "/" + QString("sampled-path-flows.data");
		SampledPathFlowEvents sampledPathFlowEvents;
		if (sampledPathFlowEvents.load(fileName)) {
			QVector<QHash<quint64, SampledFlowEvents> > &pathFlows = sampledPathFlowEvents.pathFlows;
			if (tsMin == ULONG_LONG_MAX) {
				for (int iPath = 0; iPath < editor->graph()->paths.count(); iPath++) {
					foreach (quint64 flowKey, pathFlows[iPath].uniqueKeys()) {
						if (!pathFlows[iPath][flowKey].flowEvents.isEmpty()) {
							tsMin = qMin(tsMin, pathFlows[iPath][flowKey].flowEvents.first().tsEvent);
						}
					}
				}
			}

			for (int iPath = 0; iPath < editor->graph()->paths.count(); iPath++) {
				if (pathFlows[iPath].isEmpty())
					continue;

				accordion->addLabel(QString("Path timelines for path %1:  %2 -> %3")
									.arg(iPath + 1)
									.arg(editor->graph()->paths[iPath].source + 1)
									.arg(editor->graph()->paths[iPath].dest + 1));

				QString title = QString("Timeline for path %1:  %2 -> %3")
								.arg(iPath + 1)
								.arg(editor->graph()->paths[iPath].source + 1)
								.arg(editor->graph()->paths[iPath].dest + 1);

				QString	subtitle = " - Throughput (Mbps)";
				QOPlotWidget *plotThroughput = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
				plotThroughput->plot.title = title + subtitle;
				plotThroughput->plot.xlabel = "Time (s)";
				plotThroughput->plot.xSISuffix = false;
				plotThroughput->plot.ylabel = "Throughput (Mbps)";
				plotThroughput->plot.ySISuffix = false;
				plotThroughput->plot.drag_y_enabled = false;
				plotThroughput->plot.zoom_y_enabled = false;
				plotThroughput->plot.legendVisible = false;

				subtitle = " - Loss (%)";
				QOPlotWidget *plotLoss = new QOPlotWidget(accordion,
																0,
																graphHeight,
																QSizePolicy(QSizePolicy::MinimumExpanding,
																			QSizePolicy::Fixed));
				plotLoss->plot.title = title + subtitle;
				plotLoss->plot.xlabel = "Time (s)";
				plotLoss->plot.xSISuffix = false;
				plotLoss->plot.ylabel = "Loss (%)";
				plotLoss->plot.ySISuffix = false;
				plotLoss->plot.drag_y_enabled = false;
				plotLoss->plot.zoom_y_enabled = false;
				plotLoss->plot.legendVisible = false;

                subtitle = " - Throughput cap for the *opposite* path (Mbps)";
				QOPlotWidget *plotThroughputCap = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
				plotThroughputCap->plot.title = title + subtitle;
				plotThroughputCap->plot.xlabel = "Time (s)";
				plotThroughputCap->plot.xSISuffix = true;
                plotThroughputCap->plot.ylabel = "Throughput cap for the opposite (Mbps)";
				plotThroughputCap->plot.ySISuffix = false;
				plotThroughputCap->plot.drag_y_enabled = false;
				plotThroughputCap->plot.zoom_y_enabled = false;
				plotThroughputCap->plot.legendVisible = false;

				subtitle = " - One-way delay (ms)";
				QOPlotWidget *plotRtt = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
				plotRtt->plot.title = title + subtitle;
				plotRtt->plot.xlabel = "Time (s)";
				plotRtt->plot.xSISuffix = true;
				plotRtt->plot.ylabel = "One-way delay (ms)";
				plotRtt->plot.ySISuffix = false;
				plotRtt->plot.drag_y_enabled = false;
				plotRtt->plot.zoom_y_enabled = false;
				plotRtt->plot.legendVisible = false;

				subtitle = " - Receive window (B)";
				QOPlotWidget *plotRcvWin = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
				plotRcvWin->plot.title = title + subtitle;
				plotRcvWin->plot.xlabel = "Time (s)";
				plotRcvWin->plot.xSISuffix = true;
				plotRcvWin->plot.ylabel = "Receive window (B)";
				plotRcvWin->plot.ySISuffix = false;
				plotRcvWin->plot.drag_y_enabled = false;
				plotRcvWin->plot.zoom_y_enabled = false;
				plotRcvWin->plot.legendVisible = false;

				int nFlows = 0;
				foreach (quint64 flowKey, pathFlows[iPath].uniqueKeys()) {
					nFlows++;
					if (nFlows > maxFlows)
						break;
					quint8 protocol;
					quint16 srcPort;
					quint16 dstPort;
					SampledPathFlowEvents::decodeKey(flowKey, protocol, srcPort, dstPort);
					QString protocolLabel;
					if (protocol == IPPROTO_TCP) {
						protocolLabel = "TCP";
					} else if (protocol == IPPROTO_UDP) {
						protocolLabel = "UDP";
					} else if (protocol == IPPROTO_ICMP) {
						protocolLabel = "ICMP";
					} else {
						protocolLabel = QString::number(protocol);
					}
					QString flowLabel = QString("%1 %2:%3")
							.arg(protocolLabel)
							.arg(srcPort)
							.arg(dstPort);

					QOPlotCurveData *curveLoss = new QOPlotCurveData;
					curveLoss->legendVisible = true;
					curveLoss->legendLabel = flowLabel;
					curveLoss->pointSymbol = "o";

					QOPlotCurveData *curveThroughput = new QOPlotCurveData;
					curveThroughput->legendVisible = true;
					curveThroughput->legendLabel = flowLabel;
					curveThroughput->pointSymbol = "o";

					QOPlotCurveData *curveThroughputCap = new QOPlotCurveData;
					curveThroughputCap->legendVisible = true;
					curveThroughputCap->legendLabel = flowLabel;
					curveThroughputCap->pointSymbol = "o";

					QOPlotCurveData *curveRtt = new QOPlotCurveData;
					curveRtt->legendVisible = true;
					curveRtt->legendLabel = flowLabel;
					curveRtt->pointSymbol = "o";

					QOPlotCurveData *curveRcvWin = new QOPlotCurveData;
					curveRcvWin->legendVisible = true;
					curveRcvWin->legendLabel = flowLabel;
					curveRcvWin->pointSymbol = "o";

					// seconds
					qreal rtt = 0.001;
					qreal tcpReceiveWindowScale = 1.0;
					for (int iEvent = 0; iEvent < pathFlows[iPath][flowKey].flowEvents.count(); iEvent++) {
						FlowEvent event = pathFlows[iPath][flowKey].flowEvents[iEvent];
						if (event.isTcpSyn()) {
							tcpReceiveWindowScale = 1 << event.tcpReceiveWindowScale;
						}
						rtt = event.fordwardDelay > 0 ? event.fordwardDelay * 1.0e-9 : rtt;
						// bytes
						qreal window = event.isTcpSyn() ? event.tcpReceiveWindow :
														  event.tcpReceiveWindow * tcpReceiveWindowScale;
						// seconds
						qreal period = 1.0;
						if (iEvent + 1 < pathFlows[iPath][flowKey].flowEvents.count()) {
							period = (pathFlows[iPath][flowKey].flowEvents[iEvent + 1].tsEvent -
									 event.tsEvent) * 1.0e-9;
						}
						// Mbps
						qreal throughput = event.bytesTotal / period * 1.0e-6 * 8.0;
						// Mbps
						qreal throughputCap = window / (2 * rtt) * 1.0e-6 * 8.0;
						// percent
						qreal loss = event.packetsDropped / qreal(event.packetsTotal) * 100.0;
						// update curves
						curveLoss->x << ((event.tsEvent - tsMin) * 1.0e-9);
						curveLoss->y << loss;
						curveThroughput->x << ((event.tsEvent - tsMin) * 1.0e-9);
						curveThroughput->y << throughput;
						curveThroughputCap->x << ((event.tsEvent - tsMin) * 1.0e-9);
						curveThroughputCap->y << throughputCap;
						curveRtt->x << ((event.tsEvent - tsMin) * 1.0e-9);
						curveRtt->y << (rtt * 1.0e3);
						curveRcvWin->x << ((event.tsEvent - tsMin) * 1.0e-9);
						curveRcvWin->y << window;
					}

					plotLoss->plot.addData(curveLoss);
					plotThroughput->plot.addData(curveThroughput);
					plotThroughputCap->plot.addData(curveThroughputCap);
					plotRtt->plot.addData(curveRtt);
					plotRcvWin->plot.addData(curveRcvWin);
				}

				plotLoss->plot.differentColors();
				plotLoss->autoAdjustAxes();
				plotLoss->drawPlot();
				accordion->addWidget(plotLoss->plot.title, plotLoss);

				plotThroughput->plot.differentColors();
				plotThroughput->autoAdjustAxes();
				plotThroughput->drawPlot();
				accordion->addWidget(plotThroughput->plot.title, plotThroughput);

				plotThroughputCap->plot.differentColors();
				plotThroughputCap->autoAdjustAxes();
				plotThroughputCap->drawPlot();
				accordion->addWidget(plotThroughputCap->plot.title, plotThroughputCap);

				plotRtt->plot.differentColors();
				plotRtt->autoAdjustAxes();
				plotRtt->drawPlot();
				accordion->addWidget(plotRtt->plot.title, plotRtt);

				plotRcvWin->plot.differentColors();
				plotRcvWin->autoAdjustAxes();
				plotRcvWin->drawPlot();
				accordion->addWidget(plotRcvWin->plot.title, plotRcvWin);
			}
		}
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabChanged(ui->tabResults);
}

void MainWindow::on_cmbSimulation_currentIndexChanged(int index)
{
	currentSimulation = index;

	// 0 is (new)
	if (currentSimulation > 0) {
		loadSimulation();
	} else {
		if (currentTopology > 0) {
			editor->loadGraph(ui->cmbTopologies->itemText(currentTopology) + ".graph");
		}
	}
    editor->setReadOnly(ui->cmbSimulation->currentIndex() > 0);
	updateWindowTitle();
}

void MainWindow::on_cmbTopologies_currentIndexChanged(int index)
{
	currentTopology = index;
	doReloadSimulationList(QString());

	// 0 is (untitled)
	if (currentTopology > 0) {
		editor->loadGraph(ui->cmbTopologies->itemText(currentTopology) + ".graph");
	} else {
		editor->clear();
	}
    editor->setReadOnly(ui->cmbSimulation->currentIndex() > 0);
	updateWindowTitle();
}

void MainWindow::on_btnResultsRename_clicked()
{
	if (currentSimulation <= 0)
		return;

	bool ok;
	QString newName = QInputDialog::getText(this,
											"Rename simulation",
											"New name:",
											QLineEdit::Normal,
											simulations[currentSimulation].dir,
											&ok);
	if (ok) {
		QDir dir("./" + newName);
		if (!newName.isEmpty() && !newName.contains("/") && !dir.exists() && !QDir(".").exists(newName)) {
			if (QDir(".").rename(simulations[currentSimulation].dir, newName)) {
				simulations[currentSimulation].dir = newName;
                emit reloadSimulationList(newName);
			} else {
				QMessageBox::critical(this,
									  "Error",
									  QString("The name you entered (%1) could not be used.").arg(newName),
									  QMessageBox::Ok);
			}
		} else {
			QMessageBox::critical(this,
								  "Error",
								  QString("The name you entered (%1) could not be used.").arg(newName),
								  QMessageBox::Ok);
		}
	}
}

void MainWindow::on_btnSelectNewSimulation_clicked()
{
	ui->cmbSimulation->setCurrentIndex(0);
}

void MainWindow::on_txtResultsInterestingLinks_returnPressed()
{
    if (currentSimulation <= 0)
        return;
    emit reloadSimulationList(simulations[currentSimulation].dir);
}
