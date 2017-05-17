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
#include "../util/tinyhistogram.h"

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

	resultsPlotGroup.clear();
	resultsPlotGroup.setSyncEnabled(ui->checkSyncPlots->isChecked());
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	const int textHeight = 400;
	// const int tableHeight = 400;
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
        content += QString("Number of links: %1\n").arg(tomoData.n);
		content += QString("Simulation time (s): %1\n").arg((tomoData.tsMax - tomoData.tsMin) / 1.0e9);
        content += QString("Transmission rates for paths (#delivered packets / #sent packets):");
		foreach (qreal v, tomoData.y) {
			content += QString(" %1").arg(v);
		}
		content += "\n";
        content += QString("Transmission rates for links (#delivered packets / #sent packets):");
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
        if (ui->txtResultsInterestingLinks->text() == "all") {
            interestingEdges.clear();
            for (int i = 0; i < editor->graph()->edges.count(); i++) {
                interestingEdges << i;
            }
        } else {
            QStringList tokens = ui->txtResultsInterestingLinks->text().split(QRegExp("[,\\s]"), QString::SkipEmptyParts);
            foreach (QString token, tokens) {
                bool ok;
                int i = token.toInt(&ok);
                if (ok && i >= 0 && i < editor->graph()->edges.count()) {
                    interestingEdges << (i - 1);
                }
            }
        }
	}

	QList<qint32> interestingPaths;
	if (ui->txtResultsInterestingPaths->text() == "all") {
        interestingPaths.clear();
        for (int i = 0; i < editor->graph()->paths.count(); i++) {
            interestingPaths << i;
        }
    } else {
        QStringList tokens = ui->txtResultsInterestingPaths->text().split(QRegExp("[,\\s]"), QString::SkipEmptyParts);
        foreach (QString token, tokens) {
            bool ok;
            int i = token.toInt(&ok);
            if (ok && i >= 0 && i < editor->graph()->paths.count()) {
                interestingPaths << (i - 1);
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
        EdgeTimelines edgeTimelines;
        if (readEdgeTimelines(edgeTimelines, editor->graph(), simulations[currentSimulation].dir)) {
            foreach (qint32 iEdge, interestingEdges) {
                for (int queue = -1; queue < editor->graph()->edges[iEdge].queueCount; queue++) {
                    if (queue >= 0 && editor->graph()->edges[iEdge].isNeutral())
						continue;
                    EdgeTimeline &timeline = edgeTimelines.timelines[iEdge][1 + queue];
                    if (timeline.items.isEmpty() || timeline.tsMin > timeline.tsMax)
                        continue;

                    {
                        const qint64 nelem = qMin(1000, timeline.items.count());
                        qDebug() << timeline.items.count() << "data points of which we show the first" << nelem;
                        const quint64 tsample = timeline.timelineSamplingPeriod;

                        accordion->addLabel(QString("Link timelines for link %1, %2sampling period %3 s").
                                            arg(iEdge + 1).
                                            arg(queue >= 0 ? QString("queue %1, ").arg(queue) : QString("")).
                                            arg(tsample * 1.0e-9));

                        QOPlotCurveData *arrivals_p = new QOPlotCurveData;
                        QOPlotCurveData *arrivals_B = new QOPlotCurveData;
						QOPlotCurveData *rate_B = new QOPlotCurveData;
						QOPlotCurveData *queue_sampled = new QOPlotCurveData;
						QOPlotCurveData *queue_sampled_limit = new QOPlotCurveData;
                        QOPlotCurveData *queue_numflows = new QOPlotCurveData;
                        QOPlotCurveData *lossRate = new QOPlotCurveData;

                        QString title = QString("Timeline for link %1 (%2 -> %3), %4sampling period %5 s").
                                        arg(iEdge + 1).
                                arg(editor->graph()->edges[iEdge].source + 1).
                                arg(editor->graph()->edges[iEdge].dest + 1).
                                arg(queue >= 0 ? QString("queue %1, ").arg(queue) : QString("")).
                                arg(tsample * 1.0e-9);

                        arrivals_p->x.reserve(nelem);
                        arrivals_p->y.reserve(nelem);
                        arrivals_p->pointSymbol = "";
                        for (int i = 0; i < nelem; i++) {
                            arrivals_p->x << (timeline.items[i].timestamp * 1.0e-9);
                            arrivals_p->y << timeline.items[i].arrivals_p / (tsample * 1.0e-9);
                        }

                        QString subtitle = " - Arrivals (packets)";
                        QOPlotWidget *plot_arrivals_p = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        plot_arrivals_p->plot.title = "" + subtitle;
                        plot_arrivals_p->plot.xlabel = "Time (s)";
                        plot_arrivals_p->plot.xSISuffix = true;
                        plot_arrivals_p->plot.ylabel = "Packets / second";
                        plot_arrivals_p->plot.ySISuffix = true;
                        plot_arrivals_p->plot.addData(arrivals_p);
                        plot_arrivals_p->plot.drag_y_enabled = false;
                        plot_arrivals_p->plot.zoom_y_enabled = false;
						plot_arrivals_p->plotFileName = QString("%1/e%2%3-ratep")
														.arg(simulations[currentSimulation].dir)
														.arg(iEdge + 1)
														.arg(queue >= 0
															 ? QString("-q%1").arg(queue + 1)
															 : QString());
                        plot_arrivals_p->autoAdjustAxes();
                        plot_arrivals_p->drawPlot();
                        accordion->addWidget(title + subtitle, plot_arrivals_p);
                        resultsPlotGroup.addPlot(plot_arrivals_p);

                        arrivals_B->x = arrivals_p->x;
						rate_B->x = arrivals_p->x;
                        arrivals_B->y.reserve(nelem);
						rate_B->y.reserve(nelem);
                        arrivals_B->pointSymbol = "";
						rate_B->pointSymbol = "";
						rate_B->pen.setColor(Qt::red);
                        for (int i = 0; i < nelem; i++) {
                            arrivals_B->y << timeline.items[i].arrivals_B * 8 / (tsample * 1.0e-9);
							rate_B->y << timeline.rate_Bps * 8;
                        }

                        subtitle = " - Arrivals (bits)";
                        QOPlotWidget *plot_arrivals_B = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        plot_arrivals_B->plot.title = "" + subtitle;
                        plot_arrivals_B->plot.xlabel = "Time (s)";
                        plot_arrivals_B->plot.xSISuffix = true;
                        plot_arrivals_B->plot.ylabel = "Bits / second";
                        plot_arrivals_B->plot.ySISuffix = true;
                        plot_arrivals_B->plot.addData(arrivals_B);
						plot_arrivals_B->plot.addData(rate_B);
                        plot_arrivals_B->plot.drag_y_enabled = false;
                        plot_arrivals_B->plot.zoom_y_enabled = false;
						plot_arrivals_B->plotFileName = QString("%1/e%2%3-rate")
														.arg(simulations[currentSimulation].dir)
														.arg(iEdge + 1)
														.arg(queue >= 0
															 ? QString("-q%1").arg(queue + 1)
															 : QString());
                        plot_arrivals_B->autoAdjustAxes();
                        plot_arrivals_B->drawPlot();
                        accordion->addWidget(title + subtitle, plot_arrivals_B);
                        resultsPlotGroup.addPlot(plot_arrivals_B);

                        queue_sampled->x = arrivals_p->x;
						queue_sampled_limit->x = arrivals_p->x;
                        queue_sampled->y.reserve(nelem);
						queue_sampled_limit->y.reserve(nelem);
                        queue_sampled->pointSymbol = "";
						queue_sampled_limit->pointSymbol = "";
						queue_sampled_limit->pen.setColor(Qt::red);
                        for (int i = 0; i < nelem; i++) {
                            queue_sampled->y << timeline.items[i].queue_sampled * 8;
							queue_sampled_limit->y << timeline.qcapacity * 8;
                        }

                        subtitle = " - Queue size (sampled)";
                        QOPlotWidget *plot_queue_sampled = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        plot_queue_sampled->plot.title = "" + subtitle;
                        plot_queue_sampled->plot.xlabel = "Time (s)";
                        plot_queue_sampled->plot.xSISuffix = true;
                        plot_queue_sampled->plot.ylabel = "Bits";
                        plot_queue_sampled->plot.ySISuffix = true;
                        plot_queue_sampled->plot.addData(queue_sampled);
						plot_queue_sampled->plot.addData(queue_sampled_limit);
                        plot_queue_sampled->plot.drag_y_enabled = false;
                        plot_queue_sampled->plot.zoom_y_enabled = false;
						plot_queue_sampled->plotFileName = QString("%1/e%2%3-queue")
														.arg(simulations[currentSimulation].dir)
														.arg(iEdge + 1)
														.arg(queue >= 0
															 ? QString("-q%1").arg(queue + 1)
															 : QString());
                        plot_queue_sampled->autoAdjustAxes();
                        plot_queue_sampled->drawPlot();
                        accordion->addWidget(title + subtitle, plot_queue_sampled);
                        resultsPlotGroup.addPlot(plot_queue_sampled);

                        subtitle = " - Number of flows";
                        QOPlotWidget *plot_numflows = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        plot_numflows->plot.title = "" + subtitle;
                        plot_numflows->plot.xlabel = "Time (s)";
                        plot_numflows->plot.xSISuffix = true;
                        plot_numflows->plot.ylabel = "Flows";
                        plot_numflows->plot.ySISuffix = true;
                        plot_numflows->plot.addData(queue_numflows);
                        plot_numflows->plot.drag_y_enabled = false;
                        plot_numflows->plot.zoom_y_enabled = false;
						plot_numflows->plotFileName = QString("%1/e%2%3-flows")
														.arg(simulations[currentSimulation].dir)
														.arg(iEdge + 1)
														.arg(queue >= 0
															 ? QString("-q%1").arg(queue + 1)
															 : QString());
                        plot_numflows->autoAdjustAxes();
                        plot_numflows->drawPlot();
                        accordion->addWidget(title + subtitle, plot_numflows);
                        resultsPlotGroup.addPlot(plot_numflows);

                        lossRate->x = arrivals_p->x;
                        lossRate->y.reserve(nelem);
                        lossRate->pointSymbol = "";
                        for (int i = 0; i < nelem; i++) {
                            lossRate->y << ((timeline.items[i].arrivals_p > 1.0e-9) ? (100.0 * timeline.items[i].qdrops_p / timeline.items[i].arrivals_p) :
                                                                                 0.0);
                        }

                        subtitle = " - Packet loss (%)";
                        QOPlotWidget *plot_lossRate = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        plot_lossRate->plot.title = "" + subtitle;
                        plot_lossRate->plot.xlabel = QString("Time (s) - sampled at %1s").arg(tsample / 1.0e9);
                        plot_lossRate->plot.xSISuffix = true;
                        plot_lossRate->plot.ylabel = "Packet loss (%)";
                        plot_lossRate->plot.ySISuffix = false;
                        plot_lossRate->plot.addData(lossRate);
                        plot_lossRate->plot.drag_y_enabled = false;
                        plot_lossRate->plot.zoom_y_enabled = false;
						plot_lossRate->plotFileName = QString("%1/e%2%3-loss")
														.arg(simulations[currentSimulation].dir)
														.arg(iEdge + 1)
														.arg(queue >= 0
															 ? QString("-q%1").arg(queue + 1)
															 : QString());
                        plot_lossRate->autoAdjustAxes();
                        plot_lossRate->drawPlot();
                        accordion->addWidget(title + subtitle, plot_lossRate);
                        resultsPlotGroup.addPlot(plot_lossRate);

                        subtitle = " - Packet loss (%, CDF)";
                        QOPlotWidget *plot_lossRateCDF = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        plot_lossRateCDF->plot.title = "" + subtitle;
                        plot_lossRateCDF->plot.xlabel = "Packet loss (%)";
                        plot_lossRateCDF->plot.xSISuffix = false;
                        plot_lossRateCDF->plot.ylabel = "Fraction";
                        plot_lossRateCDF->plot.ySISuffix = false;
                        plot_lossRateCDF->plot.addData(QOPlotCurveData::fromPoints(QOPlot::makeCDFSampled(lossRate->y.toList(), 0, 100), "CDF"));
                        plot_lossRateCDF->plot.drag_y_enabled = false;
                        plot_lossRateCDF->plot.zoom_y_enabled = false;
						plot_lossRateCDF->plotFileName = QString("%1/e%2%3-losscdf")
														.arg(simulations[currentSimulation].dir)
														.arg(iEdge + 1)
														.arg(queue >= 0
															 ? QString("-q%1").arg(queue + 1)
															 : QString());
                        plot_lossRateCDF->autoAdjustAxes();
                        plot_lossRateCDF->drawPlot();
                        accordion->addWidget(title + subtitle, plot_lossRateCDF);
                        resultsPlotGroup.addPlot(plot_lossRateCDF);
                    }
                }
            }
        }
	}

	accordion->addLabel("Path timelines");
    if (0) {
        PathTimelines pathTimelines;
        if (readPathTimelines(pathTimelines, editor->graph(), simulations[currentSimulation].dir)) {
			foreach (qint32 iPath, interestingPaths) {
				PathTimeline &timeline = pathTimelines.timelines[iPath];
				if (timeline.items.isEmpty() || timeline.tsMin > timeline.tsMax)
					continue;

				{
					const qint64 nelem = timeline.items.count();
					const quint64 tsample = timeline.timelineSamplingPeriod;

					accordion->addLabel(QString("Path timelines for path %1, sampling period %3 s").
										arg(iPath + 1).
										arg(tsample * 1.0e-9));

					QOPlotCurveData *arrivals_p = new QOPlotCurveData;
					QOPlotCurveData *arrivals_B = new QOPlotCurveData;
					QOPlotCurveData *delay_avg = new QOPlotCurveData;
					QOPlotCurveData *delay_max = new QOPlotCurveData;
					QOPlotCurveData *delay_min = new QOPlotCurveData;
					QOPlotCurveData *lossRate = new QOPlotCurveData;

					QString title = QString("Timeline for path %1 (%2 -> %3), sampling period %5 s").
									arg(iPath + 1).
									arg(editor->graph()->paths[iPath].source + 1).
									arg(editor->graph()->paths[iPath].dest + 1).
									arg(tsample * 1.0e-9);

					arrivals_p->x.reserve(nelem);
					arrivals_p->y.reserve(nelem);
					arrivals_p->pointSymbol = "";
					for (int i = 0; i < nelem; i++) {
						arrivals_p->x << (timeline.items[i].timestamp * 1.0e-9);
						arrivals_p->y << timeline.items[i].arrivals_p / (tsample * 1.0e-9);
					}

					QString subtitle = " - Sending rate (packets)";
					QOPlotWidget *plot_arrivals_p = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_arrivals_p->plot.title = "" + subtitle;
					plot_arrivals_p->plot.xlabel = "Time (s)";
					plot_arrivals_p->plot.xSISuffix = true;
					plot_arrivals_p->plot.ylabel = "Packets / second";
					plot_arrivals_p->plot.ySISuffix = true;
					plot_arrivals_p->plot.addData(arrivals_p);
					plot_arrivals_p->plot.drag_y_enabled = false;
					plot_arrivals_p->plot.zoom_y_enabled = false;
					plot_arrivals_p->plotFileName = QString("%1/p%2-ratep")
													.arg(simulations[currentSimulation].dir)
													.arg(iPath + 1);
					plot_arrivals_p->autoAdjustAxes();
					plot_arrivals_p->drawPlot();
					accordion->addWidget(title + subtitle, plot_arrivals_p);
					resultsPlotGroup.addPlot(plot_arrivals_p);

					arrivals_B->x = arrivals_p->x;
					arrivals_B->y.reserve(nelem);
					arrivals_B->pointSymbol = "";
					for (int i = 0; i < nelem; i++) {
						arrivals_B->y << timeline.items[i].arrivals_B * 8 / (tsample * 1.0e-9);
					}

					subtitle = " - Sending rate (bits)";
					QOPlotWidget *plot_arrivals_B = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_arrivals_B->plot.title = "" + subtitle;
					plot_arrivals_B->plot.xlabel = "Time (s)";
					plot_arrivals_B->plot.xSISuffix = true;
					plot_arrivals_B->plot.ylabel = "Bits / second";
					plot_arrivals_B->plot.ySISuffix = true;
					plot_arrivals_B->plot.addData(arrivals_B);
					plot_arrivals_B->plot.drag_y_enabled = false;
					plot_arrivals_B->plot.zoom_y_enabled = false;
					plot_arrivals_B->plotFileName = QString("%1/p%2-rate")
													.arg(simulations[currentSimulation].dir)
													.arg(iPath + 1);
					plot_arrivals_B->autoAdjustAxes();
					plot_arrivals_B->drawPlot();
					accordion->addWidget(title + subtitle, plot_arrivals_B);
					resultsPlotGroup.addPlot(plot_arrivals_B);

					delay_avg->y.reserve(nelem);
					delay_avg->pointSymbol = "";
					for (int i = 0; i < nelem; i++) {
						delay_avg->x << arrivals_p->x[i];
						delay_avg->y << (timeline.items[i].exits_p ? timeline.items[i].delay_total / timeline.items[i].exits_p : 0) / 1.0e6;
					}

					subtitle = " - Delay (interval average)";
					QOPlotWidget *plot_delay_avg = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_delay_avg->plot.title = "";
					plot_delay_avg->plot.xlabel = "Time [s]";
					plot_delay_avg->plot.xSISuffix = true;
					plot_delay_avg->plot.ylabel = "Delay [ms]";
					plot_delay_avg->plot.ySISuffix = true;
					plot_delay_avg->plot.legendVisible = false;
					plot_delay_avg->plot.addData(delay_avg);
					plot_delay_avg->plot.drag_y_enabled = false;
					plot_delay_avg->plot.zoom_y_enabled = false;
					plot_delay_avg->plotFileName = QString("%1/p%2-delayavg")
												   .arg(simulations[currentSimulation].dir)
												   .arg(iPath + 1);
					plot_delay_avg->autoAdjustAxes();
					plot_delay_avg->drawPlot();
					accordion->addWidget(title + subtitle, plot_delay_avg);
					resultsPlotGroup.addPlot(plot_delay_avg);

					delay_max->y.reserve(nelem);
					delay_max->pointSymbol = "";
					for (int i = 0; i < nelem; i++) {
						delay_max->x << arrivals_p->x[i];
						delay_max->y << timeline.items[i].delay_max / 1.0e6;
					}

					subtitle = " - Delay (interval maximums)";
					QOPlotWidget *plot_delay_max = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_delay_max->plot.title = "";
					plot_delay_max->plot.xlabel = "Time [s]";
					plot_delay_max->plot.xSISuffix = true;
					plot_delay_max->plot.ylabel = "Delay [ms]";
					plot_delay_max->plot.ySISuffix = true;
					plot_delay_max->plot.legendVisible = false;
					plot_delay_max->plot.addData(delay_max);
					plot_delay_max->plot.drag_y_enabled = false;
					plot_delay_max->plot.zoom_y_enabled = false;
					plot_delay_max->plotFileName = QString("%1/p%2-delaymax")
												   .arg(simulations[currentSimulation].dir)
												   .arg(iPath + 1);
					plot_delay_max->autoAdjustAxes();
					plot_delay_max->drawPlot();
					accordion->addWidget(title + subtitle, plot_delay_max);
					resultsPlotGroup.addPlot(plot_delay_max);

					delay_min->y.reserve(nelem);
					delay_min->pointSymbol = "";
					for (int i = 0; i < nelem; i++) {
						delay_min->x << arrivals_p->x[i];
						delay_min->y << timeline.items[i].delay_min / 1.0e6;
					}

					subtitle = " - Delay (interval minimums)";
					QOPlotWidget *plot_delay_min = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_delay_min->plot.title = "";
					plot_delay_min->plot.xlabel = "Time [s]";
					plot_delay_min->plot.xSISuffix = true;
					plot_delay_min->plot.ylabel = "Delay [ms]";
					plot_delay_min->plot.ySISuffix = true;
					plot_delay_min->plot.legendVisible = false;
					plot_delay_min->plot.addData(delay_min);
					plot_delay_min->plot.drag_y_enabled = false;
					plot_delay_min->plot.zoom_y_enabled = false;
					plot_delay_min->plotFileName = QString("%1/p%2-delaymin")
												   .arg(simulations[currentSimulation].dir)
												   .arg(iPath + 1);
					plot_delay_min->autoAdjustAxes();
					plot_delay_min->drawPlot();
					accordion->addWidget(title + subtitle, plot_delay_min);
					resultsPlotGroup.addPlot(plot_delay_min);

					lossRate->x = arrivals_p->x;
					lossRate->y.reserve(nelem);
					lossRate->pointSymbol = "";
					for (int i = 0; i < nelem; i++) {
						lossRate->y << ((timeline.items[i].arrivals_p > 1.0e-9) ? (100.0 * timeline.items[i].drops_p / timeline.items[i].arrivals_p) :
																				  0.0);
					}

					subtitle = " - Packet loss (%)";
					QOPlotWidget *plot_lossRate = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_lossRate->plot.title = "" + subtitle;
					plot_lossRate->plot.xlabel = QString("Time (s) - sampled at %1s").arg(tsample / 1.0e9);
					plot_lossRate->plot.xSISuffix = true;
					plot_lossRate->plot.ylabel = "Packet loss (%)";
					plot_lossRate->plot.ySISuffix = false;
					plot_lossRate->plot.addData(lossRate);
					plot_lossRate->plot.drag_y_enabled = false;
					plot_lossRate->plot.zoom_y_enabled = false;
					plot_lossRate->plotFileName = QString("%1/p%2-loss")
												  .arg(simulations[currentSimulation].dir)
												  .arg(iPath + 1);
					plot_lossRate->autoAdjustAxes();
					plot_lossRate->drawPlot();
					accordion->addWidget(title + subtitle, plot_lossRate);
					resultsPlotGroup.addPlot(plot_lossRate);

					subtitle = " - Packet loss (%, CDF)";
					QOPlotWidget *plot_lossRateCDF = new QOPlotWidget(accordion, 0, graphHeight, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
					plot_lossRateCDF->plot.title = "" + subtitle;
					plot_lossRateCDF->plot.xlabel = "Packet loss (%)";
					plot_lossRateCDF->plot.xSISuffix = false;
					plot_lossRateCDF->plot.ylabel = "Fraction";
					plot_lossRateCDF->plot.ySISuffix = false;
					plot_lossRateCDF->plot.addData(QOPlotCurveData::fromPoints(QOPlot::makeCDFSampled(lossRate->y.toList(), 0, 100), "CDF"));
					plot_lossRateCDF->plot.drag_y_enabled = false;
					plot_lossRateCDF->plot.zoom_y_enabled = false;
					plot_lossRateCDF->plotFileName = QString("%1/p%2-losscdf")
													 .arg(simulations[currentSimulation].dir)
													 .arg(iPath + 1);
					plot_lossRateCDF->autoAdjustAxes();
					plot_lossRateCDF->drawPlot();
					accordion->addWidget(title + subtitle, plot_lossRateCDF);
					resultsPlotGroup.addPlot(plot_lossRateCDF);
				}
			}
        }
	}

	accordion->addLabel("Flow timelines");
    if (0) {
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
                plotThroughput->plot.title = "" + subtitle;
				plotThroughput->plot.xlabel = "Time (s)";
				plotThroughput->plot.xSISuffix = false;
				plotThroughput->plot.ylabel = "Throughput (Mbps)";
				plotThroughput->plot.ySISuffix = false;
				plotThroughput->plot.drag_y_enabled = false;
				plotThroughput->plot.zoom_y_enabled = false;
				plotThroughput->plot.legendVisible = false;
				plotThroughput->plotFileName = QString("%1/p%2-rate")
												.arg(simulations[currentSimulation].dir)
												.arg(iPath + 1);

				subtitle = " - Loss (%)";
				QOPlotWidget *plotLoss = new QOPlotWidget(accordion,
																0,
																graphHeight,
																QSizePolicy(QSizePolicy::MinimumExpanding,
																			QSizePolicy::Fixed));
                plotLoss->plot.title = "" + subtitle;
				plotLoss->plot.xlabel = "Time (s)";
				plotLoss->plot.xSISuffix = false;
				plotLoss->plot.ylabel = "Loss (%)";
				plotLoss->plot.ySISuffix = false;
				plotLoss->plot.drag_y_enabled = false;
				plotLoss->plot.zoom_y_enabled = false;
				plotLoss->plot.legendVisible = false;
				plotLoss->plotFileName = QString("%1/p%2-loss")
												.arg(simulations[currentSimulation].dir)
												.arg(iPath + 1);

                subtitle = " - Throughput cap for the *opposite* path (Mbps)";
				QOPlotWidget *plotThroughputCap = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
                plotThroughputCap->plot.title = "" + subtitle;
				plotThroughputCap->plot.xlabel = "Time (s)";
				plotThroughputCap->plot.xSISuffix = true;
                plotThroughputCap->plot.ylabel = "Throughput cap for the opposite (Mbps)";
				plotThroughputCap->plot.ySISuffix = false;
				plotThroughputCap->plot.drag_y_enabled = false;
				plotThroughputCap->plot.zoom_y_enabled = false;
				plotThroughputCap->plot.legendVisible = false;
				plotThroughputCap->plotFileName = QString("%1/p%2-ratecaprev")
												.arg(simulations[currentSimulation].dir)
												.arg(iPath + 1);

				subtitle = " - One-way delay (ms)";
				QOPlotWidget *plotRtt = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
                plotRtt->plot.title = "" + subtitle;
				plotRtt->plot.xlabel = "Time (s)";
				plotRtt->plot.xSISuffix = true;
				plotRtt->plot.ylabel = "One-way delay (ms)";
				plotRtt->plot.ySISuffix = false;
				plotRtt->plot.drag_y_enabled = false;
				plotRtt->plot.zoom_y_enabled = false;
				plotRtt->plot.legendVisible = false;
				plotRtt->plotFileName = QString("%1/p%2-delay")
												.arg(simulations[currentSimulation].dir)
												.arg(iPath + 1);

				subtitle = " - Receive window (B)";
				QOPlotWidget *plotRcvWin = new QOPlotWidget(accordion,
														  0,
														  graphHeight,
														  QSizePolicy(QSizePolicy::MinimumExpanding,
																	  QSizePolicy::Fixed));
                plotRcvWin->plot.title = "" + subtitle;
				plotRcvWin->plot.xlabel = "Time (s)";
				plotRcvWin->plot.xSISuffix = true;
				plotRcvWin->plot.ylabel = "Receive window (B)";
				plotRcvWin->plot.ySISuffix = false;
				plotRcvWin->plot.drag_y_enabled = false;
				plotRcvWin->plot.zoom_y_enabled = false;
				plotRcvWin->plot.legendVisible = false;
				plotRcvWin->plotFileName = QString("%1/p%2-rwin")
												.arg(simulations[currentSimulation].dir)
												.arg(iPath + 1);

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
                    curveLoss->pointSymbol = "";

					QOPlotCurveData *curveThroughput = new QOPlotCurveData;
					curveThroughput->legendVisible = true;
					curveThroughput->legendLabel = flowLabel;
                    curveThroughput->pointSymbol = "";

					QOPlotCurveData *curveThroughputCap = new QOPlotCurveData;
					curveThroughputCap->legendVisible = true;
					curveThroughputCap->legendLabel = flowLabel;
                    curveThroughputCap->pointSymbol = "";

					QOPlotCurveData *curveRtt = new QOPlotCurveData;
					curveRtt->legendVisible = true;
					curveRtt->legendLabel = flowLabel;
                    curveRtt->pointSymbol = "";

					QOPlotCurveData *curveRcvWin = new QOPlotCurveData;
					curveRcvWin->legendVisible = true;
					curveRcvWin->legendLabel = flowLabel;
                    curveRcvWin->pointSymbol = "";

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
				resultsPlotGroup.addPlot(plotLoss);

				plotThroughput->plot.differentColors();
				plotThroughput->autoAdjustAxes();
				plotThroughput->drawPlot();
				accordion->addWidget(plotThroughput->plot.title, plotThroughput);
				resultsPlotGroup.addPlot(plotThroughput);

				plotThroughputCap->plot.differentColors();
				plotThroughputCap->autoAdjustAxes();
				plotThroughputCap->drawPlot();
				accordion->addWidget(plotThroughputCap->plot.title, plotThroughputCap);
				resultsPlotGroup.addPlot(plotThroughputCap);

				plotRtt->plot.differentColors();
				plotRtt->autoAdjustAxes();
				plotRtt->drawPlot();
				accordion->addWidget(plotRtt->plot.title, plotRtt);
				resultsPlotGroup.addPlot(plotRtt);

				plotRcvWin->plot.differentColors();
				plotRcvWin->autoAdjustAxes();
				plotRcvWin->drawPlot();
				accordion->addWidget(plotRcvWin->plot.title, plotRcvWin);
				resultsPlotGroup.addPlot(plotRcvWin);
			}
		}
	}

	if (QFile::exists(simulations[currentSimulation].dir + "/" + QString("injection.data"))) {
		TrafficTraceRecord traceRecord;
    if (0 && traceRecord.load(simulations[currentSimulation].dir + "/" + QString("injection.data"))) {
			if (0) {
				TrafficTraceRecord traceRecordRaw;
				if (traceRecordRaw.rawLoad(QString(simulations[currentSimulation].dir + "/" + QString("injection.data")).toLatin1().constData())) {
					if (traceRecordRaw.tsStart != traceRecord.tsStart) {
						qDebug() << "different data!!!";
						Q_ASSERT_FORCE(false);
					}
					if (traceRecordRaw.events.count() != traceRecord.events.count()) {
						qDebug() << "different data!!!";
						Q_ASSERT_FORCE(false);
					}
					for (int iEvent = 0; iEvent < traceRecord.events.count(); iEvent++) {
						if (traceRecordRaw.events[iEvent].traceIndex != traceRecord.events[iEvent].traceIndex ||
							traceRecordRaw.events[iEvent].packetIndex != traceRecord.events[iEvent].packetIndex ||
							traceRecordRaw.events[iEvent].injectionTime != traceRecord.events[iEvent].injectionTime ||
							traceRecordRaw.events[iEvent].exitTime != traceRecord.events[iEvent].exitTime ||
							traceRecordRaw.events[iEvent].theoreticalDelay != traceRecord.events[iEvent].theoreticalDelay) {
							qDebug() << "different data!!!";
							Q_ASSERT_FORCE(false);
						}
					}
				} else {
					qDebug() << "raw load error!!!";
					Q_ASSERT_FORCE(false);
				}
			}
			for (int iTrace = 0; iTrace < editor->graph()->trafficTraces.count(); iTrace++) {
				editor->graph()->trafficTraces[iTrace].loadFromPcap();
			}

			qint64 splitInterval = 60ULL * 1000ULL * 1000ULL * 1000ULL;
			QVector<TinyHistogram> injectionDelays;
			QVector<TinyHistogram> theoreticalDelays;
			QVector<TinyHistogram> processingDelays;
			QVector<TinyHistogram> relativeDelayErrors;
			QVector<qint32> packets;
			QVector<qint32> drops;

			qint64 tsStart = (qint64)traceRecord.tsStart;
			for (int iEvent = 0; iEvent < traceRecord.events.count(); iEvent++) {
				qint32 iTrace = traceRecord.events[iEvent].traceIndex;
				qint32 iPacket = traceRecord.events[iEvent].packetIndex;
				if (0 <= iTrace && iTrace < editor->graph()->trafficTraces.count() &&
					0 <= iPacket && iPacket < editor->graph()->trafficTraces[iTrace].packets.count()) {
					// Origin: 0
					qint64 idealInjectionTime = (qint64)editor->graph()->trafficTraces[iTrace].packets[iPacket].timestamp;
					int iSplit = idealInjectionTime / splitInterval;
					while (iSplit >= injectionDelays.count()) {
						injectionDelays << TinyHistogram(32);
						theoreticalDelays << TinyHistogram(32);
						processingDelays << TinyHistogram(32);
						relativeDelayErrors << TinyHistogram(32);
						packets << 0;
						drops << 0;
					}

					qint64 realInjectionTime = (qint64)traceRecord.events[iEvent].injectionTime - tsStart;

					qint64 delta = realInjectionTime - idealInjectionTime;

					if (delta < 0) {
						qDebug() << __FILE__ << __LINE__ << "negative delay";
						Q_ASSERT_FORCE(false);
					}

					injectionDelays[iSplit].recordEvent(delta);
					packets[iSplit]++;
					if (traceRecord.events[iEvent].exitTime == 0) {
						drops[iSplit]++;
					} else {
						qint64 absoluteDelayError = traceRecord.events[iEvent].exitTime - traceRecord.events[iEvent].injectionTime
													- traceRecord.events[iEvent].theoreticalDelay;
						processingDelays[iSplit].recordEvent(absoluteDelayError);
						qint64 relativeDelayError = (absoluteDelayError * 100) / qMax(1ULL, traceRecord.events[iEvent].theoreticalDelay);
						relativeDelayErrors[iSplit].recordEvent(relativeDelayError);
						theoreticalDelays[iSplit].recordEvent(traceRecord.events[iEvent].theoreticalDelay);
					}
				} else {
					qDebug() << __FILE__ << __LINE__ << "bad index";
					Q_ASSERT_FORCE(false);
				}
			}

            //qDebug() << __FILE__ << __LINE__ << "===============================================";
			for (int iSplit = 0; iSplit < injectionDelays.count(); iSplit++) {
				qDebug() << __FILE__ << __LINE__ << "-----------------------------------------------";
				qDebug() << __FILE__ << __LINE__ << "Interval:" << time2String(iSplit * splitInterval) << "to" << time2String((iSplit + 1) * splitInterval);
				qDebug() << __FILE__ << __LINE__ << "Packets:" << intWithCommas2String(packets[iSplit]);
				qDebug() << __FILE__ << __LINE__ << "Drops:" << intWithCommas2String(drops[iSplit]);
				qDebug() << __FILE__ << __LINE__ << "Injection delays:" << injectionDelays[iSplit].toString(time2String);
				qDebug() << __FILE__ << __LINE__ << "Theoretical delays:" << theoreticalDelays[iSplit].toString(time2String);
				qDebug() << __FILE__ << __LINE__ << "Processing delays:" << processingDelays[iSplit].toString(time2String);
				qDebug() << __FILE__ << __LINE__ << "Relative delay errors (%):" << relativeDelayErrors[iSplit].toString(intWithCommas2String);
			}
            //qDebug() << __FILE__ << __LINE__ << "===============================================";

			for (int iTrace = 0; iTrace < editor->graph()->trafficTraces.count(); iTrace++) {
				editor->graph()->trafficTraces[iTrace].clear();
			}
		} else {
			qDebug() << "Could not load injection.data";
		}
	} else {
		qDebug() << "No injection.data";
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabChanged(ui->tabResults);

	if (saveResultPlots) {
		QApplication::processEvents();

		foreach (QDisclosure *d, accordion->disclosures) {
			QWidget *w = d->widget();
			if (QOPlotWidget *plot = dynamic_cast<QOPlotWidget*>(w)) {
				d->expand();
				QApplication::processEvents();

				while (plot->height() < 20) {
					usleep(100000);
					QApplication::processEvents();
				}
				plot->saveImage(plot->plotFileName + ".png");
			}

			d->collapse();
			QApplication::processEvents();
		}
		saveResultPlots = false;
	}
}

void MainWindow::on_btnResultsSavePlots_clicked()
{
	// TODO
	saveResultPlots = true;
	loadSimulation();
}

void MainWindow::on_checkSyncPlots_toggled(bool )
{
	resultsPlotGroup.setSyncEnabled(ui->checkSyncPlots->isChecked());
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

void MainWindow::on_txtResultsInterestingPaths_returnPressed()
{
    on_txtResultsInterestingLinks_returnPressed();
}
