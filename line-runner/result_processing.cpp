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

bool processResults(QString paramsFileName, quint64 resamplePeriod) {
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

    return processResults(workingDir, graphName, experimentSuffix, resamplePeriod);
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

bool computePathCongestionProbabilities(QString workingDir, QString graphName, QString experimentSuffix, quint64 resamplePeriod)
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

    experimentIntervalMeasurements = experimentIntervalMeasurements.resample(resamplePeriod);

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

bool dumpPathIntervalData(QString workingDir, QString graphName, QString experimentSuffix, quint64 resamplePeriod)
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

    experimentIntervalMeasurements = experimentIntervalMeasurements.resample(resamplePeriod);

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

bool nonNeutralityAnalysis(QString workingDir, QString graphName, QString experimentSuffix, quint64 resamplePeriod)
{
    const int numClasses = 2;
    const qreal threshold = 1.602e-19;

    NetGraph g;
    if (!loadGraph(workingDir, graphName, g))
        return false;

    QVector<int> pathTrafficClass = g.getPathTrafficClassStrict(true);

    foreach (qreal c, pathTrafficClass) {
        Q_ASSERT_FORCE(0 <= c && c < numClasses);
    }

    ExperimentIntervalMeasurements experimentIntervalMeasurements;
    if (!experimentIntervalMeasurements.load(workingDir + "/" + "interval-measurements.data")) {
        return false;
    }

    experimentIntervalMeasurements = experimentIntervalMeasurements.resample(resamplePeriod);

    Q_ASSERT_FORCE(experimentIntervalMeasurements.numEdges == g.edges.count());
    Q_ASSERT_FORCE(experimentIntervalMeasurements.numPaths == g.paths.count());

    const qreal firstTransientCutSec = 10;
    const qreal lastTransientCutSec = 10;
    const int firstTransientCut = firstTransientCutSec * 1.0e9 / experimentIntervalMeasurements.intervalSize;
    const int lastTransientCut = lastTransientCutSec * 1.0e9 / experimentIntervalMeasurements.intervalSize;

    // first index: edge
    // second index: class
    // third index: arbitrary path index
    QList<QList<QList<qreal> > > edgeClassPathCongProbs;
    for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
        edgeClassPathCongProbs << QList<QList<qreal> >();
        for (int c = 0; c < numClasses; c++) {
            edgeClassPathCongProbs[e] << QList<qreal>();
            edgeClassPathCongProbs[e] << QList<qreal>();
        }
        for (int p = 0; p < experimentIntervalMeasurements.numPaths; p++) {
            QInt32Pair ep = QInt32Pair(e, p);
            if (experimentIntervalMeasurements.globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight == 0)
                continue;

            qreal congestionProbability = 0;
            int numIntervals = 0;
            for (int interval = firstTransientCut; interval < experimentIntervalMeasurements.numIntervals() - lastTransientCut; interval++) {
                bool ok;
                qreal loss = 1.0 -
                             experimentIntervalMeasurements.intervalMeasurements[interval].perPathEdgeMeasurements[ep].successRate(&ok);
                if (!ok)
                    continue;
                numIntervals++;
                if (loss >= threshold) {
                    congestionProbability += 1.0;
                }
            }
            congestionProbability /= qMax(1, numIntervals);
            edgeClassPathCongProbs[e][pathTrafficClass[p]] << congestionProbability;
        }
    }

    // save result

    QString dataFile;
    dataFile += QString("Experiment\t%1\tinterval\t%2\n").arg(experimentSuffix).arg(experimentIntervalMeasurements.intervalSize / 1.0e9);
    for (int e = 0; e < experimentIntervalMeasurements.numEdges; e++) {
        dataFile += QString("Link\t%1\t%2").arg(e + 1).arg(g.edges[e].isNeutral() ? "neutral" : g.edges[e].policerCount > 1 ? "policing" : "shaping");
        dataFile += "\n";
        dataFile += "\n";
        for (int c = 0; c < numClasses; c++) {
            dataFile += QString("Class\t%1").arg(c + 1);
            foreach (qreal prob, edgeClassPathCongProbs[e][c]) {
                dataFile += QString("\t%1").arg(prob * 100.0);
            }
            dataFile += "\n";
        }
        dataFile += "\n";
    }
    if (!saveFile(workingDir + "/" + "edge-class-path-cong-prob.txt", dataFile))
        return false;

    return true;
}

bool processResults(QString workingDir, QString graphName, QString experimentSuffix, quint64 resamplePeriod)
{
    NetGraph g;
    if (!loadGraph(workingDir, graphName, g))
        return false;

	saveFile(workingDir + "/" + "graph.txt", g.toText());
    saveFile(workingDir + "/" + "experiment-suffix.txt", experimentSuffix);
    computePathCongestionProbabilities(workingDir, graphName, experimentSuffix, resamplePeriod);
    dumpPathIntervalData(workingDir, graphName, experimentSuffix, resamplePeriod);
    nonNeutralityAnalysis(workingDir, graphName, experimentSuffix, resamplePeriod);

	return true;
}
