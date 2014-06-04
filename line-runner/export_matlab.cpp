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

#include "export_matlab.h"

#include "intervalmeasurements.h"
#include "netgraph.h"
#include "run_experiment_params.h"
#include "util.h"

bool exportGraphToMatlab(QString graphFileName, QString dir);

bool exportMatlab(QString paramsFileName) {
	RunParams runParams;
	if (!loadRunParams(runParams, paramsFileName)) {
		return false;
	}

	qDebug() << QString("Exporting matlab files for graph %1, working dir %2")
				.arg(runParams.graphName)
				.arg(runParams.workingDir);

	if (!exportMatlab(runParams.workingDir, runParams.graphName)) {
		return false;
	}

	return true;
}

bool exportMatlab(QString resultsDir, QString graphName) {
    QString graphFileName = resultsDir + "/" + graphName + ".graph";
	{
		QDir dir(resultsDir);
		dir.mkpath(graphName + "-matlab");
	}
	NetGraph g;
	g.setFileName(graphFileName);
	if (!g.loadFromFile()) {
		return false;
	}

	QString exportDir = resultsDir + "/" + graphName + "-matlab";
	exportGraphToMatlab(graphFileName, exportDir);

	ExperimentIntervalMeasurements experimentIntervalMeasurements;
	QString fileName = resultsDir + "/" + "interval-measurements.data";
	if (!experimentIntervalMeasurements.load(fileName)) {
		qError() << QString("Failed to open file %1").arg(fileName);
		return false;
	}

	QVector<bool> trueEdgeNeutrality(g.edges.count());
	for (int e = 0; e < g.edges.count(); e++) {
		trueEdgeNeutrality[e] = g.edges[e].isNeutral();
	}

	int totalPathLength = 0;
	int minPathLength = 9999;
	int maxPathLength = 0;
	foreach (NetGraphPath p, g.paths) {
		if (p.edgeList.isEmpty())
			continue;
		minPathLength = qMin(minPathLength, p.edgeList.count());
		maxPathLength = qMax(maxPathLength, p.edgeList.count());
		totalPathLength += p.edgeList.count();
	}
	qDebug() << "Path length: min" << minPathLength << "avg" << (totalPathLength / qreal(g.paths.count())) <<
				"max" << maxPathLength;

	// adjust to actual edge neutrality
	{
		for (int e = 0; e < g.edges.count(); e++) {
			if (!trueEdgeNeutrality[e]) {
				bool actuallyNeutral = true;
				for (int interval = 1; interval < experimentIntervalMeasurements.numIntervals(); interval++) {
					for (int p = 0; p < g.paths.count(); p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].successRate() < 0.99 &&
							experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight > 0) {
							actuallyNeutral = false;
                            qDebug() << "Link" << (e+1) << "Confirmed non-neutral" << "Path:" << (p+1) << "rate:" <<
										experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].successRate();
							break;
						}
					}
					if (!actuallyNeutral)
						break;
				}
				if (actuallyNeutral) {
                    qDebug() << "Link" << (e+1) << "is actually neutral (maybe not used)";
					trueEdgeNeutrality[e] = true;
				}
			}
		}
	}

	// count congested links
	{
		int numCongested = 0;
		for (int e = 0; e < g.edges.count(); e++) {
			bool congested = false;
			for (int interval = 1; interval < experimentIntervalMeasurements.numIntervals() - 1; interval++) {
				for (int p = 0; p < g.paths.count(); p++) {
					QInt32Pair ep = QInt32Pair(e, p);
					if (experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].successRate() < 0.985 &&
						experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight > 0) {
						congested = true;
                        qDebug() << "Link" << (e+1) << "Confirmed congested" << "Path:" << (p+1) << "rate:" <<
									experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].successRate();
						break;
					}
				}
				if (congested)
					break;
			}
			if (congested) {
				numCongested++;
			}
		}
		qDebug() << "Number of congested links:" << numCongested;
	}

	QVector<int> pathTrafficClass(g.paths.count());
	QHash<QPair<qint32, qint32>, qint32> pathEndpoints2index;
	for (int p = 0; p < g.paths.count(); p++) {
		pathEndpoints2index[QPair<qint32, qint32>(g.paths[p].source,
												  g.paths[p].dest)] = p;
	}
	foreach (NetGraphConnection c, g.connections) {
		QPair<qint32, qint32> endpoints = QPair<qint32, qint32>(c.dest, c.source);
		if (pathEndpoints2index.contains(endpoints)) {
			pathTrafficClass[pathEndpoints2index[endpoints]] = c.trafficClass;
		}
	}
	foreach (NetGraphConnection c, g.connections) {
		QPair<qint32, qint32> endpoints = QPair<qint32, qint32>(c.source, c.dest);
		if (pathEndpoints2index.contains(endpoints)) {
			pathTrafficClass[pathEndpoints2index[endpoints]] = c.trafficClass;
		}
	}

	{
		QFile file(QString("%1/%2-linkNeutrality.txt").arg(exportDir).arg(graphName));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QTextStream out(&file);
			for (int i = 0; i < g.edges.count(); i++) {
				out << QString("%1 ").arg(trueEdgeNeutrality[i] ? 1 : 0);
			}
			out << endl;
			out << flush;
			file.close();
		}
	}

	{
		QFile file(QString("%1/%2-pathGroups.txt").arg(exportDir).arg(graphName));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QTextStream out(&file);
			for (int p = 0; p < g.paths.count(); p++) {
				if (g.paths[p].edgeSet.isEmpty()) {
					continue;
				}
				out << QString("%1 ").arg(pathTrafficClass[p] + 1);
			}
			out << endl;
			out << flush;
			file.close();
		}
	}

	{
		QFile file(QString("%1/%2-numberOfIntervals.txt").arg(exportDir).arg(graphName));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QTextStream out(&file);
			out << QString("%1").arg(experimentIntervalMeasurements.numIntervals() - 2) << endl;
			out << flush;
			file.close();
		}
	}

	{
		QFile file(QString("%1/%2-pathRates.txt").arg(exportDir).arg(graphName));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QTextStream out(&file);
			for (int interval = 1; interval < experimentIntervalMeasurements.numIntervals() - 1; interval++) {
				for (int p = 0; p < g.paths.count(); p++) {
					quint64 numPackets = experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p].numPacketsInFlight;
					qreal rate;
					if (numPackets == 0) {
						rate = 1.0;
					} else {
						rate = experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p].successRate();
					}
					if (g.paths[p].edgeSet.isEmpty()) {
						if (rate != 1.0) {
							qError() << "Bad path:" << p;
							return false;
						}
						continue;
					}
					out << QString("%1 ").arg(rate);
				}
				out << endl;
			}
			out << flush;
			file.close();
		}
	}

	{
		QFile file(QString("%1/%2-linkRatesOverall.txt").arg(exportDir).arg(graphName));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QTextStream out(&file);
			for (int interval = 1; interval < experimentIntervalMeasurements.numIntervals() - 1; interval++) {
				for (int e = 0; e < g.edges.count(); e++) {
					quint64 numPackets = experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].numPacketsInFlight;
					qreal rate;
					if (numPackets == 0) {
						rate = 1.0;
					} else {
						rate = experimentIntervalMeasurements.intervalMeasurements[interval].edgeMeasurements[e].successRate();
					}
					out << QString("%1 ").arg(rate);
				}
				out << endl;
			}
			out << flush;
			file.close();
		}
	}

	foreach (int trafficClass, pathTrafficClass.toList().toSet()) {
		QFile file(QString("%1/%2-linkRatesGroup%3.txt").arg(exportDir).arg(graphName).arg(trafficClass + 1));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QTextStream out(&file);
			for (int interval = 1; interval < experimentIntervalMeasurements.numIntervals() - 1; interval++) {
				for (int e = 0; e < g.edges.count(); e++) {
					quint64 numInFlight = 0;
					quint64 numDropped = 0;
					for (int p = 0; p < g.paths.count(); p++) {
						if (pathTrafficClass[p] != trafficClass)
							continue;
						QInt32Pair ep = QInt32Pair(e, p);
						numInFlight += experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight;
						numDropped += experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsDropped;
					}
					qreal rate;
					if (numInFlight == 0) {
						rate = 1.0;
					} else {
						rate = 1.0 - qreal(numDropped) / qreal(numInFlight);
					}
					out << QString("%1 ").arg(rate);
				}
				out << endl;
			}
			out << flush;
			file.close();
		}
	}

	{
		QFile file(QString("%1/%2-pathLinkParametersByInterval.txt").arg(exportDir).arg(graphName));
		if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(file.fileName());
			return false;
		} else {
			QVector<QVector<bool> > routingMatrix; // Note: full matrix, INCLUDING empty paths. Used for fast lookups below.
			routingMatrix.resize(g.paths.count());
			for (int p = 0; p < g.paths.count(); p++) {
				QVector<bool> row;
				row.resize(g.edges.count());
				foreach (NetGraphEdge e, g.paths[p].edgeList) {
					row[e.index] = true;
				}
				routingMatrix[p] = row;
			}

			QTextStream out(&file);
			for (int interval = 1; interval < experimentIntervalMeasurements.numIntervals() - 1; interval++) {
				for (int p = 0; p < g.paths.count(); p++) {
					if (g.paths[p].edgeList.isEmpty()) {
						continue;
					}
					// dropped packets, per link, for path p
					for (int e = 0; e < g.edges.count(); e++) {
						qint64 dropped;
						if (routingMatrix[p][e]) {
							QInt32Pair ep = QInt32Pair(e, p);
							dropped = experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsDropped;
						} else {
							dropped = -1;
						}
						out << QString("%1 ").arg(dropped);
					}
					out << endl;
					// forwarded packets, per link, for path p
					for (int e = 0; e < g.edges.count(); e++) {
						qint64 forwarded;
						if (routingMatrix[p][e]) {
							QInt32Pair ep = QInt32Pair(e, p);
							forwarded = experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight -
										experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsDropped;
						} else {
							forwarded = -1;
						}
						out << QString("%1 ").arg(forwarded);
					}
					out << endl;
				}
			}
			out << flush;
			file.close();
		}
	}
	return true;
}

bool exportGraphToMatlab(QString graphFileName, QString dir) {
	NetGraph g;
	g.setFileName(graphFileName);
	if (!g.loadFromFile()) {
		qError() << QString("Could not open graph %1").arg(graphFileName);
		return false;
	}
	qDebug() << QString("Exporting graph %1").arg(graphFileName);
	QString namePrefix = graphFileName.replace(QRegExp("\\.graph$", Qt::CaseInsensitive), "")
						 .replace(QRegExp(".*/", Qt::CaseInsensitive), "")
						 .replace("/", "");
	{
		QFile fileMatrix(QString("%1/%2-routingMatrix.txt").arg(dir).arg(namePrefix));
		if (!fileMatrix.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(fileMatrix.fileName());
			return false;
		} else {
			QTextStream outMatrix(&fileMatrix);
			foreach (NetGraphPath p, g.paths) {
				if (p.edgeSet.isEmpty())
					continue;
				foreach (NetGraphEdge e, g.edges) {
					if (p.edgeSet.contains(e)) {
						outMatrix << "1 ";
					} else {
						outMatrix << "0 ";
					}
				}
				outMatrix << endl;
			}
			outMatrix << flush;
			fileMatrix.close();
		}
	}

	{
		QFile fileLinks(QString("%1/%2-linkInformation.txt").arg(dir).arg(namePrefix));
		if (!fileLinks.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(fileLinks.fileName());
			return false;
		} else {
			QTextStream outLinks(&fileLinks);
			foreach (NetGraphEdge e, g.edges) {
				bool internalEdge = (g.nodes[e.source].nodeType != NETGRAPH_NODE_HOST) &&
									(g.nodes[e.dest].nodeType != NETGRAPH_NODE_HOST);
				outLinks << QString("%1 %2 %3").arg(e.source + 1).arg(e.dest + 1).arg(internalEdge ? "1" : "0") << endl;
			}
			outLinks << flush;
			fileLinks.close();
		}
	}

	{
		QFile filePaths(QString("%1/%2-pathInformation.txt").arg(dir).arg(namePrefix));
		if (!filePaths.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
			qError() << QString("Could not create file %1").arg(filePaths.fileName());
			return false;
		} else {
			QTextStream outPaths(&filePaths);
			foreach (NetGraphPath p, g.paths) {
				if (p.edgeSet.isEmpty())
					continue;
				outPaths << QString("%1 %2").arg(p.source + 1).arg(p.dest + 1) << endl;
			}
			outPaths << flush;
			filePaths.close();
		}
	}
	return true;
}
