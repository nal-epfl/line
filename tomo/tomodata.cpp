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

#include "tomodata.h"

#include "../util/util.h"
#include "../util/chronometer.h"

// Prune a link if it belongs to at least one path (1) or to all paths? (0)
#define _AGGRESSIVE_ 1

TomoData::TomoData()
{
	m = n = 0;
	tsMin = tsMax = 0;
}

TomoData::~TomoData()
{
	data2.clear();
}

QVector<QBitArray> arrayToBitArray(QVector<QVector<quint8> > A)
{
	QVector<QBitArray> result;
	result.resize(A.count());

	for (int i = 0; i < A.count(); i++) {
		result[i].resize(A[i].count());
		for (int j = 0; j < A[i].count(); j++) {
			result[i].setBit(j, A[i][j]);
		}
	}
	return result;
}

QVector<QVector<quint8> > bitArrayToArray(QVector<QBitArray> a)
{
	QVector<QVector<quint8> > result;
	result.resize(a.count());

	for (int i = 0; i < a.count(); i++) {
		result[i].resize(a[i].count());
		for (int j = 0; j < a[i].count(); j++) {
			result[i][j] = a[i].testBit(j);
		}
	}
	return result;
}

bool TomoData::load(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;
	if (in.status() != QDataStream::Ok)
		return false;

	return true;
}

bool TomoData::save(QString fileName) const
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << *this;
	if (out.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Error reading file:" << fileName;
		return false;
	}

	return true;
}

bool TomoData::dumpAsText(QString fileName) const
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	QTextStream out(&file);
    out << "Links: " << n << "\n";
	out << "Paths: " << m << "\n";
	for (int p = 0; p < m; p++) {
		out << "Path " << p << " :" << "\n";
		out << "Src-node: " << pathstarts[p] << "\n";
		out << "Dst-node: " << pathends[p] << "\n";
        out << "Links:";
		foreach (int e, pathedges[p]) {
			out << " " << e;
		}
		out << "\n";
	}
    out << "Link transmission rates (x):";
	foreach (qreal v, xmeasured) {
		out << " " << v;
	}
	out << "\n";

	out << "Path transmission rates (y):";
	foreach (qreal v, y) {
		out << " " << v;
	}
	out << "\n";

	out << "Routing matrix:" << "\n";
	for (int p = 0; p < m; p++) {
		for (int e = 0; e < n; e++) {
			out << A[p][e] << (e == n-1 ? "\n" : " ");
		}
	}

	if (!T.isEmpty()) {
        out << "Transmission rates per path, per link:" << "\n";
		for (int p = 0; p < m; p++) {
			for (int e = 0; e < n; e++) {
				out << T[p][e] << (e == n-1 ? "\n" : " ");
			}
		}
	}

	if (!packetCounters.isEmpty()) {
        out << "Packet counters per path, per link:" << "\n";
		for (int p = 0; p < m; p++) {
			for (int e = 0; e < n; e++) {
				out << packetCounters[p][e] << (e == n-1 ? "\n" : " ");
			}
		}
	}

	return true;
}

bool TomoData::isPathLossy(int i) const
{
	if (pathedges[i].isEmpty())
		return false;
	return isEdgeSequenceLossy(y[i], getPathLength(i));
}

int TomoData::getPathLength(int i) const
{
	if (pathlengths.isEmpty())
		return pathedges[i].count();
	return pathlengths[i];
}

bool TomoData::isEdgeLossy(int j, qreal transmissionRate) const
{
	if (transmissionRate < 0) {
		return isEdgeSequenceLossy(xmeasured[j], getEdgeLength(j));
	} else {
		return isEdgeSequenceLossy(transmissionRate, getEdgeLength(j));
	}
}

int TomoData::getEdgeLength(int j) const
{
	if (edgelengths.isEmpty())
		return 1;
	return edgelengths[j];
}

bool TomoData::isEdgeSequenceLossy(qreal transmissionRate, int length)
{
	return transmissionRate < pow(TomoData::goodLinkThresh(), qMax(length * 0.25, 1.0) );
}

void TomoData::clearOptimizations()
{
	data2.clear();
	mapLinks1to2.clear();
	mapLinks2to1.clear();
	mapLinks1toGroups2.clear();
	mapGroups2toLinks1.clear();
}

TomoData &TomoData::optimizedGood(QList<int> oracleGood)
{
	if (!data2.isNull())
		return *data2;

	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);

	// the minimum transmission rate of a good link
	// const double goodTresh = 0.98;

	// the maximum transmission rate of a bad bath is pow(goodTresh, pathLength);
	// PathLen = 10 gives 90%

	// optimize
	QVector<bool> linkUsed;
	linkUsed.resize(n);
	for (int j = 0; j < n; j++) {
		linkUsed[j] = false;
		for (int i = 0; i < m && !linkUsed[j]; i++) {
			linkUsed[j] = linkUsed[j] || A[i][j];
		}
	}

	QVector<bool> linkDefinitelyGood;
	linkDefinitelyGood.resize(n);
	for (int j = 0; j < n; j++) {
		linkDefinitelyGood[j] = false;
#if 0
		// aggressive version
		for (int i = 0; i < m; i++) {
			if (A[i][j] && !isPathLossy(i)) {
				// if there is at least one good path, prune link
				linkDefinitelyGood[j] = true;
				break;
			}
		}
#else
		// error-robust version
		for (int i = 0; i < m; i++) {
			if (A[i][j]) {
				// initialize to true
				linkDefinitelyGood[j] = true;
			}
		}
		for (int i = 0; i < m; i++) {
			if (A[i][j] && isPathLossy(i)) {
				// if there is at least one bad path, do not prune link
				linkDefinitelyGood[j] = false;
				break;
			}
		}
#endif
	}
	foreach (int j, oracleGood) {
		linkDefinitelyGood[j] = true;
	}

	data2 = QSharedPointer<TomoData>(new TomoData);

	mapLinks1to2.resize(n);
	// remove a link (column) if !used or definitelyGood
	// keep it if used && !definitelyGood
	for (int j = 0; j < n; j++) {
		if (linkUsed[j] && !linkDefinitelyGood[j]) {
			mapLinks1to2[j] = data2->n;
			data2->n++;
		} else {
			mapLinks1to2[j] = -1;
		}
	}
	mapLinks2to1.resize(data2->n);
	for (int j = 0; j < n; j++) {
		if (mapLinks1to2[j] >= 0) {
			mapLinks2to1[mapLinks1to2[j]] = j;
		}
	}

	data2->m = m;
	data2->pathstarts = pathstarts;
	data2->pathends = pathends;
	data2->pathedges.resize(data2->m);
	for (int i = 0; i < m; i++) {
		foreach (qint32 e, pathedges[i]) {
			if (mapLinks1to2[e] >= 0) {
				data2->pathedges[i] << mapLinks1to2[e];
			}
		}
	}
	data2->pathlengths.resize(data2->m);
	for (int i = 0; i < m; i++) {
		data2->pathlengths[i] = getPathLength(i);
	}

	for (int i = 0; i < m; i++) {
		QVector<quint8> line;
		for (int j = 0; j < n; j++) {
			if (mapLinks1to2[j] >= 0) {
				line << A[i][j];
			}
		}
		data2->A << line;
	}
	if (!T.isEmpty()) {
		for (int i = 0; i < m; i++) {
			QVector<qreal> line;
			for (int j = 0; j < n; j++) {
				if (mapLinks1to2[j] >= 0) {
					line << T[i][j];
				}
			}
			data2->T << line;
		}
	}
	if (!packetCounters.isEmpty()) {
		for (int i = 0; i < m; i++) {
			QVector<qreal> line;
			for (int j = 0; j < n; j++) {
				if (mapLinks1to2[j] >= 0) {
					line << packetCounters[i][j];
				}
			}
			data2->packetCounters << line;
		}
	}

	data2->y = y;
	for (int j = 0; j < n; j++) {
		if (mapLinks1to2[j] >= 0) {
			data2->xmeasured << xmeasured[j];
		}
	}

	// XXX
	// update transmission rates for paths
//	for (int i = 0; i < m; i++) {
//		qreal y2 = 1.0;
//		foreach (qint32 e, data2->pathedges[i]) {
//			y2 *= data2->xmeasured[e];
//		}
//		data2->y[i] = y2;
//	}

	// check path trans rates for original
	for (int i = 0; i < m; i++) {
		qreal y1 = y[i];
		qreal y2 = 1.0;
		foreach (qint32 e, pathedges[i]) {
			y2 *= xmeasured[e];
		}
		Q_UNUSED(y1);
		Q_UNUSED(y2);
//		if (!qFuzzyCompare(y1, y2))
//			qDebug() << y1 << y2;
//		Q_ASSERT_FORCE(qFuzzyCompare(y1, y2));
	}

	// check path trans rates for optimized
	for (int i = 0; i < m; i++) {
		qreal y1 = data2->y[i];
		qreal y2 = 1.0;
		foreach (qint32 e, data2->pathedges[i]) {
			y2 *= data2->xmeasured[e];
		}
		Q_UNUSED(y1);
		Q_UNUSED(y2);
//		if (!qFuzzyCompare(y1, y2))
//			qDebug() << y1 << y2;
//		Q_ASSERT_FORCE(qFuzzyCompare(y1, y2));
	}

	data2->tsMin = tsMin;
	data2->tsMax = tsMax;

	return *data2;
}

TomoData &TomoData::optimizedUndistinguishable()
{
	if (data2)
		return *data2;

	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);

	data2 = QSharedPointer<TomoData>(new TomoData);

	mapLinks1toGroups2.resize(n);

	// initially we put each link in its own group
	for (int j = 0; j < n; j++) {
		mapLinks1toGroups2[j] = j;
	}

	// find duplicate columns in the routing matrix
	// the current group index is always <= current column index
	// so if we see that mapLinks1toGroups2[c] != c, the column was
	// grouped to another on its left
	// we compute the group's true transmission rate on the fly
	// we also keep columns at bitarrays  for performance
	QList<QBitArray> columns;
	QList<uint> columnHashes;
	{
		for (int j = 0; j < n; j++) {
			QBitArray col;
			col.resize(m);
			for (int i = 0; i < m; i++) {
				if (A[i][j])
					col.setBit(i);
			}
			columns << col;
			columnHashes << qHash(col);
		}
	}
	int currentGroup = -1;
	qreal groupTransRate = 1.0;
	for (int c1 = 0; c1 < n; c1++) {
		// skip this link if it was grouped with something else
		if (mapLinks1toGroups2[c1] != c1)
			continue;
		// start making a new group
		if (currentGroup >= 0) {
			// save the transmission rate of the previous group
			data2->xmeasured << groupTransRate;
			groupTransRate = 1.0;
		}
		currentGroup++;

		// add current column to the group
		mapLinks1toGroups2[c1] = currentGroup;
		mapGroups2toLinks1.resize(currentGroup + 1);
		mapGroups2toLinks1[currentGroup] << c1;
		groupTransRate *= xmeasured[c1];

		// compare to columns from the right
		for (int c2 = c1 + 1; c2 < n; c2++) {
			// skip this link if it was grouped with something else
			if (mapLinks1toGroups2[c2] != c2)
				continue;
			bool identical = columnHashes[c1] == columnHashes[c2] ? columns[c1] == columns[c2] : false;
#if 0
			// slow
			bool identical2 = true;
			for (int i = 0; i < m && identical2; i++) {
				if (A[i][c1] != A[i][c2]) {
					identical2 = false;
				}
			}
			Q_ASSERT_FORCE(identical == identical2);
#endif
			if (identical) {
				mapLinks1toGroups2[c2] = currentGroup;
				mapGroups2toLinks1[currentGroup] << c2;
				groupTransRate *= xmeasured[c2];
			}
		}
	}
	int groupCount = currentGroup + 1;

	if (data2->xmeasured.count() < groupCount) {
		// save the transmission rate of the last group
		data2->xmeasured << groupTransRate;
	}

	// check group trans rates
	for (int i = 0; i < groupCount; i++) {
		qreal x1 = data2->xmeasured[i];
		qreal x2 = 1.0;
		foreach (int j, mapGroups2toLinks1[i]) {
			x2 *= xmeasured[j];
		}
		if (!qFuzzyCompare(x1, x2))
			qDebug() << x1 << x2;
		Q_ASSERT_FORCE(qFuzzyCompare(x1, x2));
	}

	data2->m = m;
	data2->n = groupCount;
	data2->pathstarts = pathstarts;
	data2->pathends = pathends;
	data2->pathedges.resize(data2->m);
	data2->pathlengths.resize(data2->m);
	for (int i = 0; i < m; i++) {
		data2->pathlengths[i] = getPathLength(i);
	}

	data2->edgelengths.resize(data2->n);
	for (int i = 0; i < data2->n; i++) {
		data2->edgelengths[i] = mapGroups2toLinks1[i].count();
	}

	// zero matrix
	{
		QVector<quint8> line;
		line.resize(data2->n);
		for (int i = 0; i < m; i++) {
			data2->A << line;
		}
	}

	for (int i = 0; i < m; i++) {
		QSet<int> groupedEdges;
		foreach (qint32 e, pathedges[i]) {
			groupedEdges << mapLinks1toGroups2[e];
		}
		data2->pathedges[i] = groupedEdges.toList();
		foreach (int j, data2->pathedges[i]) {
			data2->A[i][j] = 1;
		}
	}

	if (!T.isEmpty()) {
		// zero matrix
		{
			QVector<qreal> line;
			line.resize(data2->n);
			for (int i = 0; i < m; i++) {
				data2->T << line;
			}
		}
		for (int i = 0; i < m; i++) {
			for (int j = 0; j < data2->n; j++) {
				if (data2->A[i][j]) {
					data2->T[i][j] = 1.0;
					foreach (int e, mapGroups2toLinks1[j]) {
						data2->T[i][j] *= T[i][e];
					}
				}
			}
		}
	}

	if (!packetCounters.isEmpty()) {
		// zero matrix
		{
			QVector<qreal> line;
			line.resize(data2->n);
			for (int i = 0; i < m; i++) {
				data2->packetCounters << line;
			}
		}
		for (int i = 0; i < m; i++) {
			for (int j = 0; j < data2->n; j++) {
				if (data2->A[i][j]) {
					data2->packetCounters[i][j] = 0;
					foreach (int e, mapGroups2toLinks1[j]) {
						data2->packetCounters[i][j] = qMax(data2->packetCounters[i][j], packetCounters[i][e]);
					}
				}
			}
		}
	}

	data2->y = y;

	// check path trans rates for original
	for (int i = 0; i < m; i++) {
		qreal y1 = y[i];
		qreal y2 = 1.0;
		foreach (qint32 e, pathedges[i]) {
			y2 *= xmeasured[e];
		}
		Q_UNUSED(y1);
		Q_UNUSED(y2);
//		if (!qFuzzyCompare(y1, y2))
//			qDebug() << y1 << y2;
//		Q_ASSERT_FORCE(qFuzzyCompare(y1, y2));
	}

	// check path trans rates for optimized
	for (int i = 0; i < data2->m; i++) {
		qreal y1 = data2->y[i];
		qreal y2 = 1.0;
		foreach (qint32 e, data2->pathedges[i]) {
			y2 *= data2->xmeasured[e];
		}
		Q_UNUSED(y1);
		Q_UNUSED(y2);
//		if (!qFuzzyCompare(y1, y2))
//			qDebug() << y1 << y2;
//		Q_ASSERT_FORCE(qFuzzyCompare(y1, y2));
	}

	data2->tsMin = tsMin;
	data2->tsMax = tsMax;

	return *data2;
}

QList<TomoData> &TomoData::separate()
{
	if (!dataSeparated.isEmpty())
		return dataSeparated;

	// list of sets of paths
	QList<QSet<qint32> > pathGroups;
	// set of paths that belong to a group
	QSet<qint32> pathsTaken;

	// maps paths to group indices
	QVector<qint32> pathGroupIndex;
	pathGroupIndex.resize(m);
	pathGroupIndex.fill(-1);

	// list of set of edges
	QList<QSet<qint32> > pathEdgeSets;
	for (int i = 0; i < m; i++) {
		pathEdgeSets << pathedges[i].toSet();
	}

	for (int i1 = 0; i1 < m; i1++) {
		if (pathsTaken.contains(i1))
			continue;
		if (pathedges[i1].isEmpty())
			continue;
		// start a new group with path i1
		pathGroups << QSet<qint32>();
		pathGroupIndex[i1] = pathGroups.count() - 1;
		pathGroups[pathGroupIndex[i1]].insert(i1);
		pathsTaken.insert(i1);
		// see if we can add more paths to the group
		// a group is defined by the union of all the edges in the paths of the group
		QSet<qint32> groupEdges = pathEdgeSets[i1];
		for (;;) {
			// we need several passes due to transitivity
			bool changed = false;
			for (int i2 = i1 + 1; i2 < m; i2++) {
				if (pathsTaken.contains(i2))
					continue;
				QSet<qint32> edges1 = groupEdges;
				if (edges1.intersect(pathEdgeSets[i2]).isEmpty())
					continue;
				pathGroupIndex[i2] = pathGroupIndex[i1];
				pathGroups[pathGroupIndex[i2]].insert(i2);
				pathsTaken.insert(i2);
				groupEdges.unite(pathEdgeSets[i2]);
				changed = true;
			}
			if (!changed)
				break;
		}
	}

	{
		mapPaths1toRegions2.clear();
		mapRegions2toPaths1.clear();

		mapPaths1toRegions2.fill(-1, m);
		mapRegions2toPaths1.fill(QSet<qint32>(), pathGroups.count());
		for (int i2 = 0; i2 < pathGroups.count(); i2++) {
			foreach (int i1, pathGroups[i2]) {
				mapPaths1toRegions2[i1] = i2;
				mapRegions2toPaths1[i2].insert(i1);
			}
		}

		mapLinks1toRegions2.clear();
		mapRegions2toLinks1.clear();
		mapLinks1toRegions2.fill(-1, n);
		mapRegions2toLinks1.fill(QSet<qint32>(), pathGroups.count());
		for (int iGroup = 0; iGroup < pathGroups.count(); iGroup++) {
			foreach (int i1, pathGroups[iGroup]) {
				foreach (int j1, pathedges[i1]) {
					Q_ASSERT_FORCE(mapLinks1toRegions2[j1] == iGroup || mapLinks1toRegions2[j1] == -1);
					mapLinks1toRegions2[j1] = iGroup;
					mapRegions2toLinks1[iGroup].insert(j1);
				}
			}
		}

		mapPaths1toRegionPaths2.clear();
		mapRegionPaths2toPaths1.clear();
		mapPaths1toRegionPaths2.fill(-1, m);
		mapRegionPaths2toPaths1.fill(QVector<int>(), pathGroups.count());

		mapLinks1toRegionLinks2.clear();
		mapRegionLinks2toLinks1.clear();
		mapLinks1toRegionLinks2.fill(-1, n);
		mapRegionLinks2toLinks1.fill(QVector<int>(), pathGroups.count());
	}

	//
	dataSeparated.clear();
	for (int iGroup = 0; iGroup < pathGroups.count(); iGroup++) {
		TomoData data2;
		data2.m = pathGroups[iGroup].count();
		mapRegionPaths2toPaths1[iGroup].fill(-1, data2.m);
		QSet<qint32> edgeSet;
		foreach (int i, pathGroups[iGroup]) {
			edgeSet.unite(pathEdgeSets[i]);
		}
		data2.n = edgeSet.count();
		mapRegionLinks2toLinks1[iGroup].fill(-1, data2.n);

		// maps an edge index to a data2 edge index or -1
		QVector<int> edgeMap;
		edgeMap.resize(n);
		{
			int currentIndex = 0;
			for (int j = 0; j < n; j++) {
				if (edgeSet.contains(j)) {
					edgeMap[j] = currentIndex;
					currentIndex++;
					Q_ASSERT_FORCE(mapLinks1toRegionLinks2[j] == -1);
					mapLinks1toRegionLinks2[j] = edgeMap[j];
					mapRegionLinks2toLinks1[iGroup][mapLinks1toRegionLinks2[j]] = j;
				} else {
					edgeMap[j] = -1;
				}
			}
		}

		// maps a path index to a data2 path index or -1
		QVector<int> pathMap;
		pathMap.resize(m);
		{
			int currentIndex = 0;
			for (int i = 0; i < m; i++) {
				if (pathGroups[iGroup].contains(i)) {
					pathMap[i] = currentIndex;
					currentIndex++;
					Q_ASSERT_FORCE(mapPaths1toRegionPaths2[i] == -1);
					mapPaths1toRegionPaths2[i] = pathMap[i];
					mapRegionPaths2toPaths1[iGroup][mapPaths1toRegionPaths2[i]] = i;
				} else {
					pathMap[i] = -1;
				}
			}
		}

		//
		data2.pathstarts.resize(data2.m);
		data2.pathends.resize(data2.m);
		data2.pathedges.resize(data2.m);
		data2.y.resize(data2.m);
		if (!pathlengths.isEmpty()) {
			data2.pathlengths.resize(data2.m);
		}
		foreach (int i, pathGroups[iGroup]) {
			data2.pathstarts[pathMap[i]] = pathstarts[i];
			data2.pathends[pathMap[i]] = pathends[i];
			foreach (qint32 e, pathedges[i]) {
				data2.pathedges[pathMap[i]] << edgeMap[e];
			}
			data2.y[pathMap[i]] = y[i];
			if (!pathlengths.isEmpty()) {
				data2.pathlengths[pathMap[i]] = pathlengths[i];
			}
		}

		data2.A.resize(data2.m);
		for (int i2 = 0; i2 < data2.m; i2++) {
			data2.A[i2].resize(data2.n);
		}
		for (int i2 = 0; i2 < data2.m; i2++) {
			foreach (qint32 e2, data2.pathedges[i2]) {
				data2.A[i2][e2] = 1;
			}
		}

		if (!T.isEmpty()) {
			data2.T.resize(data2.m);
			for (int i2 = 0; i2 < data2.m; i2++) {
				data2.T[i2].resize(data2.n);
			}

			for (int i2 = 0; i2 < data2.m; i2++) {
				for (int j2 = 0; j2 < data2.n; j2++) {
					data2.T[i2][j2] = T[mapRegionPaths2toPaths1[iGroup][i2]][mapRegionLinks2toLinks1[iGroup][j2]];
				}
			}
		}

		if (!packetCounters.isEmpty()) {
			data2.packetCounters.resize(data2.m);
			for (int i2 = 0; i2 < data2.m; i2++) {
				data2.packetCounters[i2].resize(data2.n);
			}

			for (int i2 = 0; i2 < data2.m; i2++) {
				for (int j2 = 0; j2 < data2.n; j2++) {
					data2.packetCounters[i2][j2] = packetCounters[mapRegionPaths2toPaths1[iGroup][i2]][mapRegionLinks2toLinks1[iGroup][j2]];
				}
			}
		}

		data2.xmeasured.resize(data2.n);
		if (!edgelengths.isEmpty()) {
			data2.edgelengths.resize(data2.n);
		}
		foreach (int j, edgeSet) {
			data2.xmeasured[edgeMap[j]] = xmeasured[j];
			if (!edgelengths.isEmpty()) {
				data2.edgelengths[edgeMap[j]] = edgelengths[j];
			}
		}

		data2.tsMin = tsMin;
		data2.tsMax = tsMax;

		dataSeparated << data2;
	}

	// consistency checks
	Q_ASSERT_FORCE(mapRegions2toPaths1.count() == dataSeparated.count());
	for (int i = 0; i < m; i++) {
		if (pathedges[i].isEmpty()) {
			Q_ASSERT_FORCE(mapPaths1toRegions2[i] < 0);
			for (int iRegion = 0; iRegion < mapRegions2toPaths1.count(); iRegion++) {
				Q_ASSERT_FORCE(!mapRegions2toPaths1[iRegion].contains(i));
			}
		} else {
			Q_ASSERT_FORCE(mapPaths1toRegions2[i] >= 0 && mapPaths1toRegions2[i] < dataSeparated.count());
			Q_ASSERT_FORCE(mapRegions2toPaths1[mapPaths1toRegions2[i]].contains(i));
			for (int iRegion = 0; iRegion < mapRegions2toPaths1.count(); iRegion++) {
				if (iRegion != mapPaths1toRegions2[i]) {
					Q_ASSERT_FORCE(!mapRegions2toPaths1[iRegion].contains(i));
				}
			}

			// check links
			foreach (int j, pathedges[i]) {
				Q_ASSERT_FORCE(mapLinks1toRegions2[j] == mapPaths1toRegions2[i]);
				Q_ASSERT_FORCE(mapRegions2toLinks1[mapPaths1toRegions2[i]].contains(j));
			}
		}
	}

	for (int iRegion = 0; iRegion < dataSeparated.count(); iRegion++) {
		QSet<qint32> regionPathIndices;
		foreach (int i, mapRegions2toPaths1[iRegion]) {
			Q_ASSERT_FORCE(mapRegionPaths2toPaths1[iRegion][mapPaths1toRegionPaths2[i]] == i);
			regionPathIndices.insert(mapPaths1toRegionPaths2[i]);
		}
		Q_ASSERT_FORCE(regionPathIndices.count() == dataSeparated[iRegion].m);

		QSet<qint32> regionLinkIndices;
		foreach (int j, mapRegions2toLinks1[iRegion]) {
			Q_ASSERT_FORCE(mapRegionLinks2toLinks1[iRegion][mapLinks1toRegionLinks2[j]] == j);
			regionLinkIndices.insert(mapLinks1toRegionLinks2[j]);
		}
		Q_ASSERT_FORCE(regionLinkIndices.count() == dataSeparated[iRegion].n);
	}

	return dataSeparated;
}

QVector<bool> TomoData::nonRedundantPaths() const
{
	QVector<bool> result;
	result.resize(m);

	if (!result.isEmpty())
		result[0] = true;

	for (int i2 = 1; i2 < m; i2++) {
		result[i2] = true;
		for (int i1 = 0; i1 < i2 && result[i2]; i1++) {
			if (result[i1]) {
				if (A[i1] == A[i2]) {
					result[i2] = false;
				}
			}
		}
	}
	return result;
}

#if 0
QList<int> TomoData::unoptimize(QList<int> lossyLinksOptimized)
{
	QList<int> result;
	foreach (int ieo, lossyLinksOptimized) {
		result << mapLinks2to1[ieo];
	}
	return result;
}
#endif

QList<bool> TomoData::unoptimizeLossynessGood(QList<bool> lossyness)
{
	TomoData &optimized = optimizedGood();
	Q_ASSERT_FORCE(lossyness.count() == optimized.n);

	QList<bool> result;
	for (int i = 0; i < n; i++) {
		if (mapLinks1to2[i] < 0) {
			// pruned, so good
			result << false;
		} else {
			result << lossyness[mapLinks1to2[i]];
		}
	}
	return result;
}

QList<bool> TomoData::unoptimizeLossynessUndistinguishable(QList<bool> lossyness)
{
	TomoData &optimized = optimizedUndistinguishable();
	Q_ASSERT_FORCE(lossyness.count() == optimized.n);

	QList<bool> result;
	for (int i = 0; i < n; i++) {
		result << lossyness[mapLinks1toGroups2[i]];
	}
	return result;
}

QList<bool> TomoData::unoptimizeLossynessSeparated(QList<QList<bool> > lossyness)
{
	QList<bool> result;
	for (int i = 0; i < n; i++) {
		result << false;
	}

	Q_ASSERT_FORCE(mapRegionLinks2toLinks1.count() == lossyness.count());
	for (int iRegion = 0; iRegion < mapRegionLinks2toLinks1.count(); iRegion++) {
		Q_ASSERT_FORCE(mapRegionLinks2toLinks1[iRegion].count() == lossyness[iRegion].count());
		for (int j2 = 0; j2 < lossyness[iRegion].count(); j2++) {
			result[mapRegionLinks2toLinks1[iRegion][j2]] = lossyness[iRegion][j2];
		}
	}
	return result;
}

QList<qreal> TomoData::unoptimizeRatesGood(QList<qreal> rates)
{
	TomoData &optimized = optimizedGood();
	Q_ASSERT_FORCE(rates.count() == optimized.n);

	QList<qreal> result;
	for (int j = 0; j < n; j++) {
		if (mapLinks1to2[j] >= 0) {
			result << rates.takeFirst();
		} else {
			result << 1.0;
		}
	}
	return result;
}

QList<qreal> TomoData::unoptimizeRatesUndistinguishable(QList<qreal> rates)
{
	TomoData &optimized = optimizedUndistinguishable();
	Q_ASSERT_FORCE(rates.count() == optimized.n);

	QList<qreal> result;
	for (int i = 0; i < n; i++) {
		qreal groupSize = mapGroups2toLinks1[mapLinks1toGroups2[i]].count();
		result << pow(rates[mapLinks1toGroups2[i]], 1/groupSize);
	}
	return result;
}

QList<qreal> TomoData::unoptimizeRatesSeparated(QList<QList<qreal> > rates)
{
	QList<qreal> result;
	for (int i = 0; i < n; i++) {
		result << 1.0;
	}

	Q_ASSERT_FORCE(mapRegionLinks2toLinks1.count() == rates.count());
	for (int iRegion = 0; iRegion < mapRegionLinks2toLinks1.count(); iRegion++) {
		Q_ASSERT_FORCE(mapRegionLinks2toLinks1[iRegion].count() == rates[iRegion].count());
		for (int j2 = 0; j2 < rates[iRegion].count(); j2++) {
			result[mapRegionLinks2toLinks1[iRegion][j2]] = rates[iRegion][j2];
		}
	}
	return result;
}

QList<int> TomoData::unoptimizeLossyEdgesGood(QList<int> lossyEdges)
{
	QList<int> result;
	foreach (int e, lossyEdges) {
		Q_ASSERT_FORCE(0 <= e && e < mapLinks2to1.count());
		Q_ASSERT_FORCE(mapLinks2to1[e] >= 0);
		result.append(mapLinks2to1[e]);
	}
	return result.toSet().toList();
}

QList<int> TomoData::unoptimizeLossyEdgesUndistinguishable(QList<int> lossyEdges)
{
	QList<int> result;
	foreach (int e, lossyEdges) {
		Q_ASSERT_FORCE(0 <= e && e < mapGroups2toLinks1.count());
		Q_ASSERT_FORCE(!mapGroups2toLinks1[e].isEmpty());
		result.append(mapGroups2toLinks1[e]);
	}
	return result.toSet().toList();
}

QList<int> TomoData::unoptimizeLossyEdgesSeparated(QList<QList<int> > lossyEdges)
{
	QList<int> result;

	Q_ASSERT_FORCE(mapRegionLinks2toLinks1.count() == lossyEdges.count());
	for (int iRegion = 0; iRegion < mapRegionLinks2toLinks1.count(); iRegion++) {
		foreach (int j2, lossyEdges[iRegion]) {
			result << mapRegionLinks2toLinks1[iRegion][j2];
		}
	}
	return result;
}

QList<bool> TomoData::idealLossyness()
{
	QList<bool> result;
	for (int i = 0; i < n; i++) {
		result << isEdgeLossy(i);
	}
	return result;
}

void TomoData::analogMetrics(QList<qreal> xcomputed, qreal &sensitivity, qreal &specificity, qreal &accuracy, QList<bool> &linkResult, QList<qreal> &absError, qreal &relDev, QList<qreal> &errorFactor, QList<bool> &linkLossyness)
{
	absError.clear();
	errorFactor.clear();
	linkResult.clear();
	linkLossyness.clear();

	for (int j = 0; j < n; j++) {
		absError << qAbs(xmeasured[j] - xcomputed[j]);
		errorFactor << qMax(xmeasured[j]/xcomputed[j], xcomputed[j]/xmeasured[j]);
	}

	relDev = 0;
	qreal s1 = 0;
	qreal s2 = 0;
	for (int j = 0; j < n; j++) {
		s1 += (xmeasured[j] - xcomputed[j]) * (xmeasured[j] - xcomputed[j]);
		s2 += xmeasured[j] * xmeasured[j];
	}
	if (s1 == 0) {
		relDev = 0;
	} else {
		relDev = sqrt(s1/s2);
	}

//	- sensitivity = ratio of correctly_detected_lossy_links / total_lossy_links, between 0 and 1;
//  - specificity = ratio of correctly_detected_good_links / total_good_links, between 0 and 1;
//  - accuracy = ratio of correctly_detected_links / total_links, between 0 and 1.

	sensitivity = 0;
	specificity = 0;
	accuracy = 0;
	qreal actualFailed = 0;
	for (int j = 0; j < n; j++) {
		if (isEdgeLossy(j)) {
			// lossy link
			actualFailed += 1;
			if (isEdgeLossy(j, xcomputed[j])) {
				sensitivity += 1;
				linkResult << true;
				accuracy += 1;
			} else {
				linkResult << false;
			}
		} else {
			// good link
			if (!isEdgeLossy(j, xcomputed[j])) {
				specificity += 1;
				linkResult << true;
				accuracy += 1;
			} else {
				linkResult << false;
			}
		}
		linkLossyness << isEdgeLossy(j, xcomputed[j]);
	}
	if (actualFailed > 0) {
		sensitivity /= actualFailed;
	} else {
		sensitivity = 0;
	}
	if (n - actualFailed > 0) {
		specificity /= n - actualFailed;
	} else {
		specificity = 0;
	}
	accuracy /= n;
}

void TomoData::binaryMetrics(QList<int> lossyX, qreal &sensitivity, qreal &specificity, qreal &accuracy, QList<bool> &linkResult, QList<bool> &linkLossyness, qreal &relDev)
{
	linkResult.clear();
	linkLossyness.clear();

	relDev = 0;
	qreal s1 = 0;
	qreal s2 = 0;
	for (int j = 0; j < n; j++) {
#if 0
		// turn measured rate into 0/1
		qreal xm = isEdgeLossy(j) ? 0 : 1;
#else
		// keep measured rate analog
		qreal xm = xmeasured[j];
#endif
		qreal xc = lossyX.contains(j) ? 0 : 1;
		s1 += (xm - xc) * (xm - xc);
		s2 += xm * xm;
	}
	if (s1 == 0) {
		relDev = 0;
	} else {
		relDev = sqrt(s1/s2);
	}

	sensitivity = 0;
	specificity = 0;
	accuracy = 0;
	qreal actualFailed = 0;
	for (int j = 0; j < n; j++) {
		if (isEdgeLossy(j)) {
			actualFailed += 1;
			if (lossyX.contains(j)) {
				sensitivity += 1;
				linkResult << true;
				accuracy += 1;
			} else {
				linkResult << false;
			}
		} else {
			if (!lossyX.contains(j)) {
				specificity += 1;
				linkResult << true;
				accuracy += 1;
			} else {
				linkResult << false;
			}
		}
		linkLossyness << lossyX.contains(j);
	}
	if (actualFailed > 0) {
		sensitivity /= actualFailed;
	} else {
		sensitivity = 0;
	}
	if (n - actualFailed > 0) {
		specificity /= n - actualFailed;
	} else {
		specificity = 0;
	}
	accuracy /= n;
}

void TomoData::metricsFromLossyness(QList<bool> lossyness, qreal &sensitivity, qreal &specificity, qreal &accuracy, QList<bool> &correct)
{
	Q_ASSERT_FORCE(lossyness.count() == n);
	correct.clear();

	sensitivity = specificity = accuracy = 0;
	qreal actualFailed = 0;
	for (int i = 0; i < n; i++) {
		if (isEdgeLossy(i)) {
			actualFailed += 1;
			if (lossyness[i]) {
				sensitivity += 1;
				accuracy += 1;
				correct << true;
			} else {
				correct << false;
			}
		} else {
			if (!lossyness[i]) {
				specificity += 1;
				accuracy += 1;
				correct << true;
			} else {
				correct << false;
			}
		}
	}
	if (actualFailed > 0) {
		sensitivity /= actualFailed;
	} else {
		sensitivity = 0;
	}
	if (n - actualFailed > 0) {
		specificity /= n - actualFailed;
	} else {
		specificity = 0;
	}
	accuracy /= n;
}

void TomoData::pruningMetrics(qreal &sensitivity, qreal &specificity, qreal &accuracy)
{
	Q_ASSERT_FORCE(mapLinks1to2.count() == n);
	// - pruningSensitivity = 0 (by pruning we cannot detect any lossy links);
	// - pruningSpecificity = ratio of correctly_detected_good_links / total_good_links_in_original_topology, between 0 and 1;
	// - pruningAccuracy = ratio of correctly_detected_good_links / total_links_in_original_topology, between 0 and 1.
	sensitivity = 0;
	qreal goodCorrect = 0;
	qreal good = 0;
	for (int i = 0; i < n; i++) {
		if (!isEdgeLossy(i)) {
			good += 1;
			if (mapLinks1to2[i] < 0) {
				goodCorrect += 1;
			}
		}
	}
	specificity = good > 0 ? goodCorrect / good : 0;
	accuracy = goodCorrect / n;
}

bool TomoData::isSlowRankCNSVD(int m, int n)
{
	// according to the lapack svdsdd benchmark with a timeout of 60s
	const int k = 1000;
	if (m <= 1*k) {
		return n > 60*k;
	}
	if (m <= 2*k) {
		return n > 16*k;
	}
	if (m <= 3*k) {
		return n > 4*k;
	}
	if (m <= 10*k) {
		return n > 2*k;
	}
	if (m <= 20*k) {
		return n > 1*k;
	}
	return true;
}

void TomoData::rankCNSVD(int &rank, qreal &conditionNumber, QList<qreal> &svd)
{
	// This is what we return in case of error
	rank = -1;
	conditionNumber = -1;
	svd.clear();
	// We abort any computation that exceeds this time
	const int timeout_s = 60;

	// Remove empty lines from matrix
	QVector<QVector<quint8> > mat = A;
	for (int i = 0; i < mat.count(); i++) {
		bool emptyLine = true;
		for (int j = 0; j < n; j++) {
			if (mat[i][j]) {
				emptyLine = false;
				break;
			}
		}
		if (emptyLine) {
			mat.remove(i);
			i--;
		}
	}
	if (mat.isEmpty()) {
		qDebug() << __FILE__ << __LINE__ << "Empty matrix";
		return;
	}

	// If we know this is going to be slow, don't even try to compute it
	if (isSlowRankCNSVD(mat.count(), mat.first().count())) {
		qDebug() << "Skipping slow SVD:" << __FILE__ << __LINE__ << QString("Size %1 x %2").arg(m).arg(n);
		return;
	}

	// Do it
	Chronometer chrono(QString("SVD %1 %2 x %3").arg(__FUNCTION__).arg(mat.count()).arg(mat.first().count()), false); Q_UNUSED(chrono);
	QProcess p;
	p.setProcessChannelMode(QProcess::MergedChannels);
	p.start("fastrank", QStringList() << "svdlsdd");
	if (!p.waitForStarted()) {
		qDebug() << "error" << __FILE__ << __LINE__;
		return;
	}

	// shuffle lines and columns
	QVector<int> lineMaps;
	for (int i = 0; i < mat.count(); i++)
		lineMaps.append(i);
	qShuffle(lineMaps);
	QVector<int> colMaps;
	for (int j = 0; j < mat.first().count(); j++)
		colMaps.append(j);
	qShuffle(colMaps);

	p.write(QString("%1 %2\n").arg(mat.count()).arg(mat.first().count()).toAscii());
	for (int i = 0; i < mat.count(); i++) {
		for (int j = 0; j < mat.first().count(); j++) {
			char s[2];
			s[0] = '0' + mat[lineMaps[i]][colMaps[j]];
			s[1] = j == n-1 ? '\n' : ' ';
			p.write(s, 2);
		}
	}
	p.closeWriteChannel();

	// It should start to compute now, wait
	if (!p.waitForFinished(timeout_s * 1000)) {
		p.kill();
		p.waitForFinished(5000);
		// too slow
		return;
	}

	QString result = p.readAll();
	bool ok;
	QStringList tokens = result.split(QRegExp("\\n|\\r|\\s"), QString::SkipEmptyParts);
	if (tokens.isEmpty()) {
		qDebug() << "error" << __FILE__ << __LINE__ << result;
		// this is usually an OOM error
//        QString dump;
//        dump += QString("fastrank svdlsdd\n");
//        dump += QString("%1 %2\n").arg(m).arg(n);
//        for (int i = 0; i < m; i++) {
//            for (int j = 0; j < n; j++) {
//                char s[3];
//                s[0] = '0' + A[i][j];
//                s[1] = j == n-1 ? '\n' : ' ';
//                s[2] = '\0';
//                dump += s;
//            }
//        }
//        dump += "\n";
//        qDebug() << dump;
		return;
	}
	// Parse results
	rank = tokens.takeFirst().toInt(&ok);
	if (!ok) {
		qDebug() << "error" << __FILE__ << __LINE__;
		rank = -1;
		return;
	}
	if (tokens.count() != rank) {
		qDebug() << "error" << __FILE__ << __LINE__;
		rank = -1;
		return;
	}
	while (!tokens.isEmpty()) {
		svd << tokens.takeFirst().toDouble();
	}
    if (!svd.isEmpty()) {
		conditionNumber = svd.first() / svd.last();
	} else {
		conditionNumber = 0;
	}
}

QDataStream& operator>>(QDataStream& s, TomoData& data)
{
	qint32 ver;
	s >> ver;
	s >> data.m;
	s >> data.n;
	s >> data.pathstarts;
	s >> data.pathends;
	s >> data.pathedges;
	QVector<QBitArray> a;
	s >> a;
	data.A = bitArrayToArray(a);
	s >> data.y;
	s >> data.xmeasured;
	s >> data.tsMin;
	s >> data.tsMax;
	if (ver >= 2) {
		s >> data.T;
	} else {
		data.T.clear();
	}
	if (ver >= 3) {
		s >> data.packetCounters;
	} else {
		data.packetCounters.clear();
	}
	if (ver >= 4) {
		s >> data.traffic;
		s >> data.qdelay;
	} else {
		data.traffic.clear();
		data.qdelay.clear();
	}

	return s;
}

QDataStream& operator<<(QDataStream& s, const TomoData& data)
{
	const qint32 ver = 4;
	s << ver;

	s << data.m;
	s << data.n;
	s << data.pathstarts;
	s << data.pathends;
	s << data.pathedges;
	QVector<QBitArray> a = arrayToBitArray(data.A);
	s << a;
	s << data.y;
	s << data.xmeasured;
	s << data.tsMin;
	s << data.tsMax;
	s << data.T;
	s << data.packetCounters;
	s << data.traffic;
	s << data.qdelay;

	return s;
}

void dumpTomoData(TomoData &data, QString outputFile)
{
	QFile file(outputFile);
	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
		qDebug() << "Could not open" << file.fileName();
		return;
	}
	QTextStream out(&file);
	out << "pathcount = " << data.m << "\n";
    out << "linkcount = " << data.n << "\n";

	out << "pathstarts = ";
	for (int p = 0; p < data.m; p++) {
		out << data.pathstarts[p] << " ";
	}
	out << "\n";

	out << "pathends = ";
	for (int p = 0; p < data.m; p++) {
		out << data.pathends[p] << " ";
	}
	out << "\n";

	out << "\n";
    out << "pathlinks" << "\n";
	for (int p = 0; p < data.m; p++) {
		foreach (int e, data.pathedges[p]) {
			out << e << " ";
		}
		out << "\n";
	}
	out << "\n";

	out << "\n";
	out << "routingmatrix" << "\n";
	for (int p = 0; p < data.m; p++) {
		for (int e = 0; e < data.n; e++) {
			out << data.A[p][e] << " ";
		}
		out << "\n";
	}
	out << "\n";

	out << "y = ";
	for (int p = 0; p < data.m; p++) {
		out << data.y[p] << " ";
	}
	out << "\n";

	out << "x = ";
	for (int e = 0; e < data.n; e++) {
		out << data.xmeasured[e] << " ";
	}
	out << "\n";

	out << "duration = " << data.tsMax - data.tsMin << "\n";

	out << "A*x = ";
	for (int p = 0; p < data.m; p++) {
		qreal t = 1.0;
		for (int e = 0; e < data.n; e++) {
			if (data.A[p][e]) {
				t *= data.xmeasured[e];
			}
		}
		out << t << " ";
	}
	out << "\n";

	out << "A*x - y = ";
	for (int p = 0; p < data.m; p++) {
		qreal t = 1.0;
		for (int e = 0; e < data.n; e++) {
			if (data.A[p][e]) {
				t *= data.xmeasured[e];
			}
		}
		t -= data.y[p];
		out << t << " ";
	}
	out << "\n";

	out.flush();
}
