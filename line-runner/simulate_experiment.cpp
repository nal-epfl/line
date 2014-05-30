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

#include "simulate_experiment.h"

#include "intervalmeasurements.h"
#include "util.h"

bool runSimulation(NetGraph &g, RunParams runParams) {
	QString testId = runParams.workingDir.split('/', QString::SkipEmptyParts).last();
	{
		QDir dir(".");
		dir.mkpath(runParams.workingDir);
	}

	const int numTrafficClasses = 2;

	// Ensures that #congested intervals / #intervals = prob. of congestion, except for effects caused by class
	// dependencies.
	const bool perfectCongestionSampling = true;
	// Ensures that #fwd packets / #packets = transmission rate for each link queue.
	const bool perfectDropSampling = true;

	for (int c = 0; c < g.connections.count(); c++) {
		Q_ASSERT_FORCE(g.connections[c].trafficClass < numTrafficClasses);
	}

	// Neutral links are by default good with the same probability for all classes.
	// Non-neutral links are by default good for class 0, congested for all other classes with potentially different
	// probabilities.
	//
	// In addition there are congested links. Depending on their type:
	// * Congested neutral links are congested with the same probability for all classes.
	// * Congested non-neutral links are congested with various probabilities for all classes.
	//
	// In any case, if class n experiences congestion for a non-neutral link, any class > n will experience congestion,
	// regardless of its probability.

	// We pick a set of congestedLinkFraction * numInternalLinks links, including all the non-neutral links and some
	// neutral links. These links form a pool of links that are congested with a high probability.
	const qreal congestedLinkFraction = runParams.congestedLinkFraction;

	// The probability that the links from the congested pool are congested, per interval, is sampled from a uniform
	// distribution bounded by these two values.
	const qreal minProbCongestedLinkIsCongested = runParams.minProbCongestedLinkIsCongested;
	const qreal maxProbCongestedLinkIsCongested = runParams.maxProbCongestedLinkIsCongested;

	// The probability that the links from outside the congested pool are congested, per interval, is sampled from a
	// uniform distribution bounded by these two values.
	const qreal minProbGoodLinkIsCongested = runParams.minProbGoodLinkIsCongested;
	const qreal maxProbGoodLinkIsCongested = runParams.maxProbGoodLinkIsCongested;

	// For each interval we flip a coin to decide if a link is congested or not, for each traffic class (taking into
	// account neutrality). After this decision, we must generate the loss rate.

	// The loss rate of the congested links, per interval, per class, is sampled from a uniform distribution bounded by
	// these two values.
	const qreal minLossRateOfCongestedLink = runParams.minLossRateOfCongestedLink;
	const qreal maxLossRateOfCongestedLink = runParams.maxLossRateOfCongestedLink;

	// The loss rate of the good links, per interval, per class, is sampled from a uniform distribution bounded by
	// these two values.
	const qreal minLossRateOfGoodLink = runParams.minLossRateOfGoodLink;
	const qreal maxLossRateOfGoodLink = runParams.maxLossRateOfGoodLink;

	// Uniform noise is added to obtain transmission rates per link, per interval, per flow.
	// This noise introduces additional non-neutrality, and will make neutral links behave slightly non-neutrally.
	// The noise is specified in additive units. It always increases the loss rate, never decreases it.
	const qreal congestedRateNoise = runParams.congestedRateNoise;
	const qreal goodRateNoise = runParams.goodRateNoise;

	std::mt19937 randGen(rand());

	QList<qint32> internalLinks = g.getInternalLinks();
	QList<qint32> usedNonNeutralInternalLinks;
	QList<qint32> usedNeutralInternalLinks;
	foreach (qint32 e, internalLinks) {
		if (g.edges[e].used) {
			if (g.edges[e].isNeutral()) {
				usedNeutralInternalLinks << e;
			} else {
				usedNonNeutralInternalLinks << e;
			}
		}
	}

	int congestedLinkPoolSize = congestedLinkFraction *
								(usedNonNeutralInternalLinks.count() + usedNeutralInternalLinks.count());
	QList<qint32> congestedLinkPool;
	if (congestedLinkPool.count() < congestedLinkPoolSize) {
		QList<qint32> candidateLinks = usedNeutralInternalLinks;
		candidateLinks.append(usedNonNeutralInternalLinks);
		qShuffle(candidateLinks);
		candidateLinks = candidateLinks.mid(0, congestedLinkPoolSize - congestedLinkPool.count());
		congestedLinkPool.append(candidateLinks);
		qSort(congestedLinkPool);
	}

	// Assign congestion probabilities to all links
	QList<QList<qreal> > linkCongestionProbabilityPerClass;
	for (int e = 0; e < g.edges.count(); e++) {
		QList<qreal> probPerClass;
		for (int c = 0; c < numTrafficClasses; c++) {
			// all links are good by default
			probPerClass << 0.0;
		}
		linkCongestionProbabilityPerClass << probPerClass;
	}
	foreach (qint32 e, usedNeutralInternalLinks) {
		for (int c = 0; c < numTrafficClasses; c++) {
			if (c == 0) {
				// neutral links are good by default
				linkCongestionProbabilityPerClass[e][c] = frand2mt(randGen,
																   minProbGoodLinkIsCongested,
																   maxProbGoodLinkIsCongested);
			} else {
				// all traffic classes have the same probability of experiencing congestion for a neutral link
				linkCongestionProbabilityPerClass[e][c] = linkCongestionProbabilityPerClass[e][0];
			}
		}
	}
	foreach (qint32 e, usedNonNeutralInternalLinks) {
		// non neutral links are good by default for class 0, congested for the other classes
		for (int c = 0; c < numTrafficClasses; c++) {
			if (c == 0) {
				linkCongestionProbabilityPerClass[e][c] = frand2mt(randGen,
																   minProbGoodLinkIsCongested,
																   maxProbGoodLinkIsCongested);
			} else {
				linkCongestionProbabilityPerClass[e][c] = frand2mt(randGen,
																   minProbCongestedLinkIsCongested,
																   maxProbCongestedLinkIsCongested);
			}
		}
	}
	foreach (qint32 e, congestedLinkPool) {
		if (g.edges[e].isNeutral()) {
			for (int c = 0; c < numTrafficClasses; c++) {
				if (c == 0) {
					// congested link
					linkCongestionProbabilityPerClass[e][c] = frand2mt(randGen,
																	   minProbCongestedLinkIsCongested,
																	   maxProbCongestedLinkIsCongested);
				} else {
					// all traffic classes have the same probability of experiencing congestion for a neutral link
					linkCongestionProbabilityPerClass[e][c] = linkCongestionProbabilityPerClass[e][0];
				}
			}
		} else {
			for (int c = 0; c < numTrafficClasses; c++) {
				// congested with various probabilities for all classes.
				linkCongestionProbabilityPerClass[e][c] = frand2mt(randGen,
																   minProbCongestedLinkIsCongested,
																   maxProbCongestedLinkIsCongested);
			}
		}
	}

	const int numIntervals = runParams.estimatedDuration / runParams.intervalSize;

	QVector<QVector<QVector<bool> > > linkCongestionPerClassPerInterval;
	linkCongestionPerClassPerInterval.resize(g.edges.count());
	QVector<int> intervals(numIntervals);
	for (int i = 0; i < numIntervals; i++) {
		intervals[i] = i;
	}
	foreach (NetGraphEdge e, g.edges) {
		linkCongestionPerClassPerInterval[e.index].resize(numTrafficClasses);
		for (int c = 0; c < numTrafficClasses; c++) {
			linkCongestionPerClassPerInterval[e.index][c].resize(numIntervals);
		}
		bool neutral = e.isNeutral();
		if (!perfectCongestionSampling) {
			for (int i = 0; i < numIntervals; i++) {
				bool congested = false;
				for (int c = 0; c < numTrafficClasses; c++) {
					if (neutral) {
						if (c == 0) {
							congested = frandex2mt(randGen) < linkCongestionProbabilityPerClass[e.index][c];
						}
					} else {
						if (!congested) {
							congested = frandex2mt(randGen) < linkCongestionProbabilityPerClass[e.index][c];
						}
					}
					linkCongestionPerClassPerInterval[e.index][c][i] = congested;
				}
			}
		} else {
			if (neutral) {
				qShuffle(intervals);
				for (int i = 0; i < numIntervals * linkCongestionProbabilityPerClass[e.index][0]; i++) {
					// All traffic classes are congested at the same time
					for (int c = 0; c < numTrafficClasses; c++) {
						linkCongestionPerClassPerInterval[e.index][c][intervals[i]] = true;
					}
				}
			} else {
				qShuffle(intervals);
				// Congest class 0 first, which makes all other classes congested
				for (int i = 0; i < numIntervals * linkCongestionProbabilityPerClass[e.index][0]; i++) {
					for (int c = 0; c < numTrafficClasses; c++) {
						linkCongestionPerClassPerInterval[e.index][c][intervals[i]] = true;
					}
				}
				// Now congest classes 1+
				for (int c = 0; c < numTrafficClasses; c++) {
					qShuffle(intervals);
					for (int i = 0; i < numIntervals * linkCongestionProbabilityPerClass[e.index][c]; i++) {
						linkCongestionPerClassPerInterval[e.index][c][intervals[i]] = true;
					}
				}
			}
		}
	}

	QVector<qint32> connection2path = g.getConnection2PathMapping();
	QVector<QVector<qint32> > pathEdges;
	{
		pathEdges.resize(g.paths.count());
		for (int p = 0; p < g.paths.count(); p++) {
			pathEdges[p].resize(g.paths[p].edgeList.count());
			int iEdge = 0;
			foreach (NetGraphEdge e, g.paths[p].edgeList) {
				pathEdges[p][iEdge] = e.index;
				iEdge++;
			}
		}
	}
	// Generate interval data
	{
		ExperimentIntervalMeasurements experimentIntervalMeasurements;
		experimentIntervalMeasurements.initialize(0,
												  runParams.estimatedDuration,
												  runParams.intervalSize,
												  g.edges.count(),
												  g.paths.count(),
                                                  g.getSparseRoutingMatrixTransposed(),
                                                  1400);

		for (int i = 0; i < numIntervals; i++) {
			qDebug() << QString("Generating data for interval %1...").arg(i + 1);
			// The current time is stored in t.
			// Add 1 ns to make sure we are not exactly between intervals; normally not a problem, but better be safe.
			quint64 t = i * runParams.intervalSize + 1;

			// 1st index: link, 2nd index: connection (flow)
			QList<QList<qreal> > linkTransRatesPerFlow;
			foreach (NetGraphEdge e, g.edges) {
				QList<qreal> transRatePerClass;
				for (int c = 0; c < numTrafficClasses; c++) {
					bool congested = linkCongestionPerClassPerInterval[e.index][c][i];
					if (congested) {
						transRatePerClass << 1.0 - frand2mt(randGen,
															minLossRateOfCongestedLink,
															maxLossRateOfCongestedLink);
					} else {
						transRatePerClass << 1.0 - frand2mt(randGen,
															minLossRateOfGoodLink,
															maxLossRateOfGoodLink);
					}
				}
				// Add noise to get transmission rates per flow
				QList<qreal> transRatePerFlow;
				for (int c = 0; c < g.connections.count(); c++) {
					int trafficClass = g.connections[c].trafficClass;
					bool congested = linkCongestionPerClassPerInterval[e.index][trafficClass][i];
					qreal cleanRate = transRatePerClass[trafficClass];
					qreal noise = congested ? congestedRateNoise : goodRateNoise;
					qreal noisyRate = frand2mt(randGen,
											   qMax(0.0, cleanRate),
											   qMin(1.0, cleanRate + noise));
					transRatePerFlow << noisyRate;
				}
				linkTransRatesPerFlow << transRatePerFlow;
			}

			// Forward packets over each connection
			const int numPackets = 10000;
            const int packetSize = 1000;
			for (int c = 0; c < g.connections.count(); c++) {
				int p = connection2path[c];
				if (!perfectDropSampling) {
					for (int packet = 0; packet < numPackets; packet++) {
                        experimentIntervalMeasurements.countPacketInFLightPath(p, t, t, packetSize, 1);
						bool dropped = false;
						foreach (qint32 e, pathEdges[p]) {
							qreal transRate = linkTransRatesPerFlow[e][c];
							// Count packet before queueing
                            experimentIntervalMeasurements.countPacketInFLightEdge(e, p, t, t, packetSize, 1);
							// Decide if it is dropped
							qreal v = frandex2mt(randGen);
							if (v > transRate) {
								// drop
								dropped = true;
							}
							// Count as dropped if necessary, and stop forwarding along the path
							if (dropped) {
                                experimentIntervalMeasurements.countPacketDropped(e, p, t, t, packetSize, 1);
								// The packet does not reach any other edges. As a side effect, this reduces the number of
								// samples for edges that are further on the path, reducing the quality of the sampling.
								break;
							}
						}
					}
				} else {
					int packetsInFlight = numPackets;
                    experimentIntervalMeasurements.countPacketInFLightPath(p, t, t, packetSize, packetsInFlight);
					foreach (qint32 e, pathEdges[p]) {
                        experimentIntervalMeasurements.countPacketInFLightEdge(e, p, t, t, packetSize, packetsInFlight);
						qreal transRate = linkTransRatesPerFlow[e][c];
						int numDropped = (1.0 - transRate) * packetsInFlight;
                        experimentIntervalMeasurements.countPacketDropped(e, p, t, t, packetSize, numDropped);
						packetsInFlight -= numDropped;
						if (packetsInFlight <= 0) {
							break;
						}
					}
				}
			}
		}

		experimentIntervalMeasurements.save(runParams.workingDir + "/interval-measurements.data");
	}

	// Write some other necessary files
	{
		QString fileNamePrefix = QString("%1/").arg(runParams.workingDir);
		QString fileName = fileNamePrefix + QString("multiplexer");
		QString result = "fake";
		saveFile(fileName + ".out", result);
		saveFile(fileName + ".err", result);
		fileName = fileNamePrefix + QString("emulator");
		result = "fake";
		saveFile(fileName + ".out", result);
		saveFile(fileName + ".err", result);
	}
	saveFile(runParams.workingDir + "/simulation.txt",
			 QString("graph=%1")
			 .arg(QString(g.fileName).replace(".graph", "").split('/', QString::SkipEmptyParts).last()));
	saveFile(runParams.workingDir + "/graph.txt", g.toText());
	QString oldName = g.fileName;
	g.fileName = runParams.workingDir + "/" + g.fileName.split('/', QString::SkipEmptyParts).last();
	g.saveToFile();
	g.fileName = oldName;
	qDebug() << QString("Test done.");

	return true;
}

