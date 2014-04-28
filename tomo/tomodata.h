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

#ifndef TOMODATA_H
#define TOMODATA_H

#include <QtCore>

class TomoData {
public:
	explicit TomoData();
	~TomoData();

    // m = number of paths
	qint32 m;
	// n = number of edges
	qint32 n;

	// pathstarts[index] = node index, maps a path index to the source node
	QVector<qint32> pathstarts;
	// pathends[index] = node index, maps a path index to the dest node
	QVector<qint32> pathends;

	// pathedges[pindex] = list of the indices of the edges in path pindex, in order
	QVector<QList<qint32> > pathedges;

	// A = routing matrix, A[i][j] = 1 iff path i contains edge j
	QVector<QVector<quint8> > A;

	// y[pindex] = transmission rate (0..1) of path pindex
	QVector<qreal> y;

	// xmeasured[eindex] = true transmission rate of edge eindex
	QVector<qreal> xmeasured;

	// the number of links in the path in the original (not optimized) topology, useful for optimized data; ignored if empty, falling back to pathedges[i].count
	QVector<int> pathlengths;

	// the number of actual links in the links (that's right!) in the optimized topology, useful for optimized data; ignored if empty, defaulting to 1
	QVector<int> edgelengths;

	// time range of the simulation (ns)
	quint64 tsMin;
	quint64 tsMax;

	// T[i][j] = transmission rate of edge j measured for packets belonging to path i
	// might be empty
	QVector<QVector<qreal> > T;

	// packetCounters[i][j] = number of packets received by edge j belonging to path i
	// might be empty
	QVector<QVector<qreal> > packetCounters;

	// traffic[i][j] = number of bytes received by edge j belonging to path i
	// might be empty
	QVector<QVector<qreal> > traffic;

	// qdelay[i][j] = total queuing delay (ns) experienced by packets forwarded on edge j belonging to path i
	// might be empty
	QVector<QVector<qreal> > qdelay;

	bool load(QString fileName);
	bool save(QString fileName) const;

	bool dumpAsText(QString fileName) const;

	void testEquations() {
		for (int ip = 0; ip < pathedges.count(); ip++) {
			double rateR = y[ip];
			double rateE = 1.0;
			for (int ie = 0; ie < pathedges[ip].count(); ie++) {
				rateE *= xmeasured[pathedges[ip][ie]];
			}
			printf("%f %f\n", rateR, rateE);
		}
	}

	bool isPathLossy(int i) const;
	int getPathLength(int i) const;

	bool isEdgeLossy(int j, qreal transmissionRate = -1.0) const;
	int getEdgeLength(int j) const;

	// transmissionRate = transmission probability of the sequence of edges
	// length = number of edges in the sequence
	// returns whether the sequence is lossy or not by comparing transmissionRate with some computed threshold
	static bool isEdgeSequenceLossy(qreal transmissionRate, int length);

	// always chaincall these
	// prunes definitely good edges
	TomoData &optimizedGood(QList<int> oracleGood = QList<int>());
	// groups undistinguishable edges together
	TomoData &optimizedUndistinguishable();

	QList<TomoData> &separate();

	QList<bool> unoptimizeLossynessGood(QList<bool> lossyness);
	QList<bool> unoptimizeLossynessUndistinguishable(QList<bool> lossyness);
	QList<bool> unoptimizeLossynessSeparated(QList<QList<bool> > lossyness);
	QList<qreal> unoptimizeRatesGood(QList<qreal> rates);
	QList<qreal> unoptimizeRatesUndistinguishable(QList<qreal> rates);
	QList<qreal> unoptimizeRatesSeparated(QList<QList<qreal> > rates);
	QList<int> unoptimizeLossyEdgesGood(QList<int> lossyEdges);
	QList<int> unoptimizeLossyEdgesUndistinguishable(QList<int> lossyEdges);
	QList<int> unoptimizeLossyEdgesSeparated(QList<QList<int> > lossyEdges);

	// simulates a perfect algorithm
	QList<bool> idealLossyness();

	QVector<bool> nonRedundantPaths() const;
	void clearOptimizations();
	
	static double goodLinkThresh() {return 0.995;}

	// Turns the data into binary (0/1 edge quality) and computes metrics.
	// Input:
	//  - xcomputed
	// Output:
	//	- sensitivity = ratio of correctly_detected_lossy_links / total_lossy_links, between 0 and 1;
	//  - specificity = ratio of correctly_detected_good_links / total_good_links, between 0 and 1;
	//  - accuracy = ratio of correctly_detected_links / total_links, between 0 and 1.
	//  - linkResult = true iff edge quality detected correctly
	//  - absError = abs(xmeasured, xcomputed)
	//  - errorFactor = max(absError, 1/absError);
	//  - linkLossyness = if edge is lossy according to xcomputed (i.e. xcomputed turned into binary form)
	void analogMetrics(QList<qreal> xcomputed, qreal &sensitivity, qreal &specificity, qreal &accuracy, QList<bool> &linkResult, QList<qreal> &absError, qreal &relDev, QList<qreal> &errorFactor, QList<bool> &linkLossyness);
	void binaryMetrics(QList<int> lossyX, qreal &sensitivity, qreal &specificity, qreal &accuracy, QList<bool> &linkResult, QList<bool> &linkLossyness, qreal &relDev);

	void metricsFromLossyness(QList<bool> lossyness, qreal &sensitivity, qreal &specificity, qreal &accuracy, QList<bool> &correct);
	void pruningMetrics(qreal &sensitivity, qreal &specificity, qreal &accuracy);

	// for optimizeGood
	QVector<int> mapLinks1to2;
	QVector<int> mapLinks2to1;

	// for optimizedUndistinguishable
	QVector<int> mapLinks1toGroups2;
	QVector<QList<int> > mapGroups2toLinks1;

	// for separated
	// path index to region index
	QVector<int> mapPaths1toRegions2;
	QVector<QSet<int> > mapRegions2toPaths1;
	// link index to region index
	QVector<int> mapLinks1toRegions2;
	QVector<QSet<int> > mapRegions2toLinks1;
	// path index mappings
	QVector<int> mapPaths1toRegionPaths2;
	QVector<QVector<int> > mapRegionPaths2toPaths1;
	// link index mappings
	QVector<int> mapLinks1toRegionLinks2;
	QVector<QVector<int> > mapRegionLinks2toLinks1;

	bool isSlowRankCNSVD(int m, int n);
	void rankCNSVD(int &rank, qreal &conditionNumber, QList<qreal> &svd);

protected:
	void resize();
	QSharedPointer<TomoData> data2;
	QList<TomoData> dataSeparated;
};

void dumpTomoData(TomoData &data, QString outputFile);

QDataStream& operator>>(QDataStream& s, TomoData& exp);

QDataStream& operator<<(QDataStream& s, const TomoData& exp);

#endif // TOMODATA_H
