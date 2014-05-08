/*
 *	Copyright (C) 2014 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license which this program may be distributed under.
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

#include "result_processing.h"

#include <QtCore>

#include "intervalmeasurements.h"
#include "netgraph.h"
#include "run_experiment_params.h"
#include "tomodata.h"

QString guessGraphName(QString workingDir) {
    QString simulationText;
    if (!readFile(workingDir + "/" + "simulation.txt", simulationText)) {
        return QString();
    }
    foreach (QString line, simulationText.split("\n")) {
        QStringList tokens = line.split("=");
        if (tokens.count() == 2) {
            if (tokens[0] == "graph") {
                return tokens[1];
            }
        }
    }
    return QString();
}

QString guessExperimentSuffix(QString workingDir) {
    // quad.2014.04.14.17.43.46.418114995566664.1118666809-link-100Mbps-pareto-1Mb-pareto-1Mb-policing-1.0-0.3-buffers-large-none-1-drop-tail-rtt-48-120
    workingDir = workingDir.split("/").last();
    if (workingDir.contains("-")) {
        workingDir = workingDir.mid(workingDir.indexOf('-') + 1);
        return workingDir;
    }
    return "";
}

bool processResults(QString paramsFileName) {
    QString workingDir;
    QString graphName;
    QString experimentSuffix;

	RunParams runParams;
	if (!loadRunParams(runParams, paramsFileName)) {
        workingDir = paramsFileName;
        graphName = guessGraphName(workingDir);
        experimentSuffix = guessExperimentSuffix(workingDir);

        if (graphName.isEmpty() || experimentSuffix.isEmpty()) {
            return false;
        }
    } else {
        workingDir = runParams.workingDir;
        graphName = runParams.graphName;
        experimentSuffix = runParams.experimentSuffix;
    }

    qDebug() << QString("Exporting results for graph %1, working dir %2, suffix %3")
                .arg(graphName)
                .arg(workingDir)
                .arg(experimentSuffix);

    return processResults(workingDir, graphName, experimentSuffix);
}

bool loadGraph(QString workingDir, QString graphName, NetGraph &g)
{
    // Remove path
    if (graphName.contains("/")) {
        graphName = graphName.mid(graphName.lastIndexOf("/") + 1);
    }
    // Add extension
    if (!graphName.endsWith(".graph"))
        graphName += ".graph";
    // Add path
    g.setFileName(workingDir + "/" + graphName);
    if (!g.loadFromFile()) {
        return false;
    }
    return true;
}

bool computePathCongestionProbabilities(QString workingDir, QString graphName, QString experimentSuffix)
{
    NetGraph g;
    if (!loadGraph(workingDir, graphName, g))
        return false;

    QVector<int> pathTrafficClass = g.getPathTrafficClassStrict(true);

    QList<qreal> thresholds = QList<qreal>()
                              << 0.100  // 10 % = 1/10
                              << 0.050
                              << 0.040
                              << 0.030
                              << 0.025
                              << 0.020
                              << 0.015
                              << 0.010  // 1 % = 1/100
                              << 0.005
                              << 0.0025
                              << 0.0010  // 0.1 % = 1/1k
                              << 0.0005
                              << 0.00025
                              << 0.00020
                              << 0.00015
                              << 0.000010  // 0.001 % = 1/100k
                              << 0.000001;  // 0.0001 % = 1/M

    // 1st index: threshold
    // 2nd index: path
    QVector<QVector<qreal> > pathCongestionProbabilities;

    ExperimentIntervalMeasurements experimentIntervalMeasurements;
    if (!experimentIntervalMeasurements.load(workingDir + "/" + "interval-measurements.data")) {
        return false;
    }

    Q_ASSERT_FORCE(experimentIntervalMeasurements.numPaths == g.paths.count());

    foreach (qreal threshold, thresholds) {
        QVector<qreal> pathCongestionProbByThreshold;
        for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
            qreal congestionProbability = 0;
            int numIntervals = 0;
            const qreal firstTransientCutSec = 10;
            const qreal lastTransientCutSec = 10;
            const int firstTransientCut = firstTransientCutSec * 1.0e9 / experimentIntervalMeasurements.intervalSize;
            const int lastTransientCut = lastTransientCutSec * 1.0e9 / experimentIntervalMeasurements.intervalSize;
            for (int interval = firstTransientCut; interval < experimentIntervalMeasurements.numIntervals() - lastTransientCut; interval++) {
                bool ok;
                qreal loss = 1.0 - experimentIntervalMeasurements.intervalMeasurements[interval].pathMeasurements[p].successRate(&ok);
                if (!ok)
                    continue;
                numIntervals++;
                if (loss >= threshold) {
                    congestionProbability += 1.0;
                }
            }
            congestionProbability /= qMax(1, numIntervals);
            pathCongestionProbByThreshold.append(congestionProbability);
        }
        pathCongestionProbabilities.append(pathCongestionProbByThreshold);
    }

    // save result

    QString dataFile;
    dataFile += "Experiment\t" + experimentSuffix + "\n";
    for (int t = -2; t < thresholds.count(); t++) {
        if (t == -2) {
            dataFile += "Path";
        } else if (t == -1) {
            dataFile += "Class";
        } else {
            dataFile += QString("%1").arg(thresholds[t] * 100.0);
        }
        for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
            if (pathTrafficClass[p] < 0)
                continue;
            if (t == -2) {
                dataFile += QString("\tP%1").arg(p + 1);
            } else if (t == -1) {
                dataFile += QString("\t%1").arg(pathTrafficClass[p]);
            } else {
                dataFile += QString("\t%1").arg(pathCongestionProbabilities[t][p] * 100.0, 0, 'f', 2);
            }
        }
        dataFile += "\n";
    }
    if (!saveFile(workingDir + "/" + "path-congestion-probs.txt", dataFile))
        return false;

    return true;
}

bool dumpPathIntervalData(QString workingDir, QString graphName, QString experimentSuffix)
{
    NetGraph g;
    if (!loadGraph(workingDir, graphName, g))
        return false;

    QVector<int> pathTrafficClass = g.getPathTrafficClassStrict(true);

    // 1st index: interval
    // 2nd index: path
    QVector<QVector<qreal> > pathLossRates;

    ExperimentIntervalMeasurements experimentIntervalMeasurements;
    if (!experimentIntervalMeasurements.load(workingDir + "/" + "interval-measurements.data")) {
        return false;
    }

    Q_ASSERT_FORCE(experimentIntervalMeasurements.numPaths == g.paths.count());

    for (int i = 0; i < experimentIntervalMeasurements.numIntervals(); i++) {
        QVector<qreal> pathLossRatesForInterval;
        for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
            qreal loss = 1.0 - experimentIntervalMeasurements.intervalMeasurements[i].pathMeasurements[p].successRate();
            pathLossRatesForInterval.append(loss);
        }
        pathLossRates.append(pathLossRatesForInterval);
    }

    // save result

    QString dataFile;
    dataFile += "Experiment\t" + experimentSuffix + "\n";
    for (int i = -2; i < experimentIntervalMeasurements.numIntervals(); i++) {
        if (i == -2) {
            dataFile += "Path";
        } else if (i == -1) {
            dataFile += "Class";
        } else {
            dataFile += QString("%1").arg(i);
        }
        for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
            if (pathTrafficClass[p] < 0)
                continue;
            if (i == -2) {
                dataFile += QString("\tP%1").arg(p + 1);
            } else if (i == -1) {
                dataFile += QString("\t%1").arg(pathTrafficClass[p]);
            } else {
                dataFile += QString("\t%1").arg(pathLossRates[i][p] * 100.0, 0, 'f', 2);
            }
        }
        dataFile += "\n";
    }
    if (!saveFile(workingDir + "/" + "path-interval-data.txt", dataFile))
        return false;

    return true;
}

bool processResults(QString workingDir, QString graphName, QString experimentSuffix)
{
    NetGraph g;
    if (!loadGraph(workingDir, graphName, g))
        return false;

	saveFile(workingDir + "/" + "graph.txt", g.toText());
    saveFile(workingDir + "/" + "experiment-suffix.txt", experimentSuffix);
    computePathCongestionProbabilities(workingDir, graphName, experimentSuffix);
    dumpPathIntervalData(workingDir, graphName, experimentSuffix);

#if 0
	const int compiledResultsLink = 0;
	QList<QList<QString> > compiledResultsCols;
	const int compiledColExperiment = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Experiment");  // 0

	const int compiledColLinkRateOverall = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Link rate overall");  // 1
	const int compiledColLinkRateClass1 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Link rate class 1");  // 2
	const int compiledColLinkRateClass2 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Link rate class 2");  // 3

	const int compiledColLinkLossOverall = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Loss rate overall");  // 4
	const int compiledColLinkLossClass1 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Loss rate class 1");  // 5
	const int compiledColLinkLossClass2 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Loss rate class 2");  // 6

	const int compiledColPCongOverall_025 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=0.25%");  // 7
	const int compiledColPCongOverall_05 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=0.5%");
	const int compiledColPCongOverall_10 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=1.0%");
	const int compiledColPCongOverall_15 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=1.5%");
	const int compiledColPCongOverall_20 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=2.0%");
	const int compiledColPCongOverall_25 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=2.5%");
	const int compiledColPCongOverall_30 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=3.0%");
	const int compiledColPCongOverall_40 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=4.0%");
	const int compiledColPCongOverall_50 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=5.0%");
	const int compiledColPCongOverall_100 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion overall, t=10.0%");

	const int compiledColPComputed_025 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=0.25%");  // 17
	const int compiledColPComputed_05 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=0.5%");
	const int compiledColPComputed_10 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=1.0%");
	const int compiledColPComputed_15 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=1.5%");
	const int compiledColPComputed_20 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=2.0%");
	const int compiledColPComputed_25 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=2.5%");
	const int compiledColPComputed_30 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=3.0%");
	const int compiledColPComputed_40 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=4.0%");
	const int compiledColPComputed_50 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=5.0%");
	const int compiledColPComputed_100 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion computed, t=10.0%");

	const int compiledColPCongClass1_025 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 1, t=0.25%");  // 27
	const int compiledColPCongClass1_05 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 1, t=0.5%");
	const int compiledColPCongClass1_10 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 1, t=1.0%");
	const int compiledColPCongClass1_15 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 1, t=1.5%");
	const int compiledColPCongClass1_20 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 1, t=2.0%");
	const int compiledColPCongClass1_25 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class1, t=2.5%");
	const int compiledColPCongClass1_30 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class1, t=3.0%");
	const int compiledColPCongClass1_40 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class1, t=4.0%");
	const int compiledColPCongClass1_50 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class1, t=5.0%");
	const int compiledColPCongClass1_100 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class1, t=10.0%");

	const int compiledColPCongClass2_025 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 2, t=0.25%");  // 37
	const int compiledColPCongClass2_05 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 2, t=0.5%");
	const int compiledColPCongClass2_10 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 2, t=1.0%");
	const int compiledColPCongClass2_15 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 2, t=1.5%");
	const int compiledColPCongClass2_20 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion class 2, t=2.0%");
	const int compiledColPCongClass2_25 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class2, t=2.5%");
	const int compiledColPCongClass2_30 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class2, t=3.0%");
	const int compiledColPCongClass2_40 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class2, t=4.0%");
	const int compiledColPCongClass2_50 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class2, t=5.0%");
	const int compiledColPCongClass2_100 = compiledResultsCols.count();
	compiledResultsCols << (QList<QString>() << "Prob. congestion Class2, t=10.0%");

	compiledResultsCols[compiledColExperiment] << experimentSuffix;

	{
		TomoData tomoData;
		QString fileName = workingDir + "/" + "tomo-records.dat";
		if (!tomoData.load(fileName)) {
			return false;
		}

		if (tomoData.xmeasured.count() > compiledResultsLink) {
			compiledResultsCols[compiledColLinkRateOverall] << QString::number(tomoData.xmeasured[compiledResultsLink] * 100.0, 'f', 1);
			compiledResultsCols[compiledColLinkLossOverall] << QString::number(100.0 - tomoData.xmeasured[compiledResultsLink] * 100.0, 'f', 1);
		} else {
			return false;
		}
	}

	{
		ExperimentIntervalMeasurements experimentIntervalMeasurements;
		QString fileName = workingDir + "/" + "interval-measurements.data";
		if (!experimentIntervalMeasurements.load(fileName)) {
			return false;
		} else {
			QVector<bool> trueEdgeNeutrality(g.edges.count());
			for (int i = 0; i < g.edges.count(); i++) {
				trueEdgeNeutrality[i] = g.edges[i].isNeutral();
			}

			QVector<bool> edgeIsHostEdge(g.edges.count());
			for (int i = 0; i < g.edges.count(); i++) {
				edgeIsHostEdge[i] = g.nodes[g.edges[i].source].nodeType == NETGRAPH_NODE_HOST ||
									g.nodes[g.edges[i].dest].nodeType == NETGRAPH_NODE_HOST;
			}

			QVector<int> pathTrafficClass(g.paths.count());
			QHash<QPair<qint32, qint32>, qint32> pathEndpoints2index;
			QHash<qint32, QPair<qint32, qint32> > pathIndex2endpoints;
			for (int p = 0; p < g.paths.count(); p++) {
				QPair<qint32, qint32> endpoints(g.paths[p].source, g.paths[p].dest);
				pathEndpoints2index[endpoints] = p;
				pathIndex2endpoints[p] = endpoints;
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
				QList<qreal> thresholds = QList<qreal>() << 0.100 << 0.050 << 0.040 << 0.030 << 0.025 << 0.020 << 0.015 << 0.010 << 0.005 << 0.0025;

				// experimentIntervalMeasurements.intervalMeasurements[e].perPathEdgeMeasurements[e]
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0)
						continue;

					for (int p = -1; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (p >= 0 &&
							experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

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

							if (e == compiledResultsLink) {
								if (p < 0) {
									if (threshold * 100.0 == 0.25) {
										compiledResultsCols[compiledColPCongOverall_025] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 0.5) {
										compiledResultsCols[compiledColPCongOverall_05] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 1.0) {
										compiledResultsCols[compiledColPCongOverall_10] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 1.5) {
										compiledResultsCols[compiledColPCongOverall_15] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 2.0) {
										compiledResultsCols[compiledColPCongOverall_20] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 2.5) {
										compiledResultsCols[compiledColPCongOverall_25] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 3.0) {
										compiledResultsCols[compiledColPCongOverall_30] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 4.0) {
										compiledResultsCols[compiledColPCongOverall_40] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 5.0) {
										compiledResultsCols[compiledColPCongOverall_50] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 10.0) {
										compiledResultsCols[compiledColPCongOverall_100] << QString::number(lossyProbability * 100.0, 'f', 1);
									}
								} else if (pathTrafficClass[p] + 1 == 1) {
									if (threshold * 100.0 == 0.25) {
										compiledResultsCols[compiledColPCongClass1_025] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 0.5) {
										compiledResultsCols[compiledColPCongClass1_05] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 1.0) {
										compiledResultsCols[compiledColPCongClass1_10] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 1.5) {
										compiledResultsCols[compiledColPCongClass1_15] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 2.0) {
										compiledResultsCols[compiledColPCongClass1_20] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 2.5) {
										compiledResultsCols[compiledColPCongClass1_25] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 3.0) {
										compiledResultsCols[compiledColPCongClass1_30] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 4.0) {
										compiledResultsCols[compiledColPCongClass1_40] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 5.0) {
										compiledResultsCols[compiledColPCongClass1_50] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 10.0) {
										compiledResultsCols[compiledColPCongClass1_100] << QString::number(lossyProbability * 100.0, 'f', 1);
									}
								} else if (pathTrafficClass[p] + 1 == 2) {
									if (threshold * 100.0 == 0.25) {
										compiledResultsCols[compiledColPCongClass2_025] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 0.5) {
										compiledResultsCols[compiledColPCongClass2_05] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 1.0) {
										compiledResultsCols[compiledColPCongClass2_10] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 1.5) {
										compiledResultsCols[compiledColPCongClass2_15] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 2.0) {
										compiledResultsCols[compiledColPCongClass2_20] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 2.5) {
										compiledResultsCols[compiledColPCongClass2_25] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 3.0) {
										compiledResultsCols[compiledColPCongClass2_30] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 4.0) {
										compiledResultsCols[compiledColPCongClass2_40] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 5.0) {
										compiledResultsCols[compiledColPCongClass2_50] << QString::number(lossyProbability * 100.0, 'f', 1);
									} else if (threshold * 100.0 == 10.0) {
										compiledResultsCols[compiledColPCongClass2_100] << QString::number(lossyProbability * 100.0, 'f', 1);
									}
								}
							}
						}
					}
				}
			}

			{
				const qint32 bottleneck = 0;
				QList<QInt32Pair> pathPairs;
				for (int p1 = 0; p1 < g.paths.count(); p1++) {
					QSet<NetGraphEdge> edges1 = g.paths[p1].edgeSet;
					for (int p2 = p1 + 1; p2 < g.paths.count(); p2++) {
						QSet<NetGraphEdge> edges2 = g.paths[p2].edgeSet;
						edges2 = edges2.intersect(edges1);
						if (edges2.count() == 1 && edges2.begin()->index == bottleneck) {
							pathPairs << QInt32Pair(p1, p2);
						}
					}
				}

				QList<qreal> thresholds = QList<qreal>() << 0.100 << 0.050 << 0.040 << 0.030 << 0.025 << 0.020 << 0.015 << 0.010 << 0.005 << 0.0025;

				// experimentIntervalMeasurements.intervalMeasurements[e].perPathEdgeMeasurements[e]
				foreach (QInt32Pair pathPair, pathPairs) {
					int p1 = pathPair.first;
					int p2 = pathPair.second;
					Q_ASSERT_FORCE(p1 < experimentIntervalMeasurements.numPaths);
					Q_ASSERT_FORCE(p2 < experimentIntervalMeasurements.numPaths);

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

						if (threshold * 100.0 == 0.25) {
							compiledResultsCols[compiledColPComputed_025] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 0.5) {
							compiledResultsCols[compiledColPComputed_05] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 1.0) {
							compiledResultsCols[compiledColPComputed_10] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 1.5) {
							compiledResultsCols[compiledColPComputed_15] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 2.0) {
							compiledResultsCols[compiledColPComputed_20] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 2.5) {
							compiledResultsCols[compiledColPComputed_25] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 3.0) {
							compiledResultsCols[compiledColPComputed_30] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 4.0) {
							compiledResultsCols[compiledColPComputed_40] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 5.0) {
							compiledResultsCols[compiledColPComputed_50] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						} else if (threshold * 100.0 == 10.0) {
							compiledResultsCols[compiledColPComputed_100] << QString::number(pLinkLossyComputed * 100.0, 'f', 1);
						}
					}
				}
			}

			{
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					if (!trueEdgeNeutrality[e]) {
						continue;
					}
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0) {
						continue;
					}
					bool allPathsGood = true;
					for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

						qint64 numDropped = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped;
						qint64 numSent = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight;
						qreal rate = 1.0 -
									 qreal(numDropped) /
									 qreal(numSent);
						allPathsGood = allPathsGood && (rate == 1.0);

						if (e == compiledResultsLink) {
							if (pathTrafficClass[p] + 1 == 1) {
								compiledResultsCols[compiledColLinkRateClass1] << QString::number(rate * 100.0, 'f', 1);
								compiledResultsCols[compiledColLinkLossClass1] << QString::number(100.0 - rate * 100.0, 'f', 1);
							} else if (pathTrafficClass[p] + 1 == 2) {
								compiledResultsCols[compiledColLinkRateClass2] << QString::number(rate * 100.0, 'f', 1);
								compiledResultsCols[compiledColLinkLossClass2] << QString::number(100.0 - rate * 100.0, 'f', 1);
							}
						}
					}
				}
			}

			{
				for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
					if (trueEdgeNeutrality[e]) {
						continue;
					}
					if (experimentIntervalMeasurements.globalMeasurements.edgeMeasurements[e].numPacketsInFlight == 0) {
						continue;
					}
					bool allPathsGood = true;
					for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
							continue;

						qint64 numDropped = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped;
						qint64 numSent = experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight;
						qreal rate = 1.0 -
									 qreal(numDropped) /
									 qreal(numSent);
						allPathsGood = allPathsGood && (rate == 1.0);

						if (e == compiledResultsLink) {
							if (pathTrafficClass[p] + 1 == 1) {
								compiledResultsCols[compiledColLinkRateClass1] << QString::number(rate * 100.0, 'f', 1);
								compiledResultsCols[compiledColLinkLossClass1] << QString::number(100.0 - rate * 100.0, 'f', 1);
							} else if (pathTrafficClass[p] + 1 == 2) {
								compiledResultsCols[compiledColLinkRateClass2] << QString::number(rate * 100.0, 'f', 1);
								compiledResultsCols[compiledColLinkLossClass2] << QString::number(100.0 - rate * 100.0, 'f', 1);
							}
						}
					}
				}
			}
		}
	}

	{
		int nRows = 0;
		for (int iCol = 0; iCol < compiledResultsCols.count(); iCol++) {
			nRows = qMax(nRows, compiledResultsCols[iCol].count());
		}
		for (int iCol = 0; iCol < compiledResultsCols.count(); iCol++) {
			for (int iRow = compiledResultsCols[iCol].count(); iRow < nRows; iRow++) {
				compiledResultsCols[iCol] << " ";
			}
		}

		QString content;
		for (int iRow = 1; iRow < nRows; iRow++) {
			for (int iCol = 0; iCol < compiledResultsCols.count(); iCol++) {
				content += compiledResultsCols[iCol][iRow];
				if (iCol < compiledResultsCols.count() - 1) {
					content += "\t";
				}
			}
			content += "\n";
		}
		QString fileName = workingDir + "/" + "compiled-stats.txt";
		saveFile(fileName, content);
	}
#endif

	return true;
}
