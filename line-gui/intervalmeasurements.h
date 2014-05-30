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

#ifndef INTERVALMEASUREMENTS_H
#define INTERVALMEASUREMENTS_H

#include <QtCore>
#include "../util/bitarray.h"

class LinkIntervalMeasurement
{
public:
    LinkIntervalMeasurement();
	qint64 numPacketsInFlight;
	qint64 numPacketsDropped;
    // 1: forward, 0: drop
    BitArray events;
	qreal successRate(bool *ok = NULL) const;
	void clear();

    void sample(int packetCount);

	friend bool operator ==(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator !=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator <(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator <=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator >(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator >=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);

	LinkIntervalMeasurement& operator+=(LinkIntervalMeasurement other);
};

bool operator ==(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator !=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator <(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator <=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator >(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator >=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);

QDataStream& operator>>(QDataStream& s, LinkIntervalMeasurement& d);
QDataStream& operator<<(QDataStream& s, const LinkIntervalMeasurement& d);

class GraphIntervalMeasurements
{
public:
	void initialize(int numEdges, int numPaths, QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed);
	// Sets all the counters to zero
	void clear();
	// Index: edge
    QVector<LinkIntervalMeasurement> edgeMeasurements;
    // Index: path
    QVector<LinkIntervalMeasurement> pathMeasurements;
    // first index: edge; second index: path
	// QVector<QVector<LinkIntervalMeasurement> > perPathEdgeMeasurements;
	QHash<QPair<qint32, qint32>, LinkIntervalMeasurement> perPathEdgeMeasurements;
	// The object other must have been initialized with the same routing matrix.
	GraphIntervalMeasurements& operator+=(GraphIntervalMeasurements other);
};

QDataStream& operator>>(QDataStream& s, GraphIntervalMeasurements& d);
QDataStream& operator<<(QDataStream& s, const GraphIntervalMeasurements& d);

class ExperimentIntervalMeasurements
{
public:
    void initialize(quint64 tsStart,
                    quint64 expectedDuration,
                    quint64 intervalSize,
                    int numEdges,
					int numPaths,
                    QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed,
                    int packetSizeThreshold);

    // These functions update the counters
    void countPacketInFLightEdge(int edge, int path, quint64 tsIn, quint64 tsOut, int size, int multiplier);
    void countPacketInFLightPath(int path, quint64 tsIn, quint64 tsOut, int size, int multiplier);
    void countPacketDropped(int edge, int path, quint64 tsIn, quint64 tsDrop, int size, int multiplier);

    // Updates the events bitmask
    void recordPacketEventPath(int path, quint64 tsIn, quint64 tsOut, int size, int multiplier, bool forwarded);
    void recordPacketEventEdge(int edge, int path, quint64 tsIn, quint64 tsOut, int size, int multiplier, bool forwarded);

    // Returns the index of the interval that includes a timestamp.
    // Also resizes the vector of intervals if the index is outside of the current range.
    int timestampToInterval(quint64 ts);

    int timestampToOpenInterval(quint64 ts);

	int numIntervals();

    bool save(QString fileName);
    bool load(QString fileName);

    void trim();

	bool exportText(QString fileName,
					QVector<bool> trueEdgeNeutrality,
					QVector<int> pathTrafficClass);
	bool exportText(QIODevice *device,
					QVector<bool> trueEdgeNeutrality,
					QVector<int> pathTrafficClass);

	// Returns a new ExperimentIntervalMeasurements object resampled at this period.
	// If resamplePeriod is smaller than intervalSize (including zero), no resampling is performed
	// and a copy of this object is returned.
	// If resamplePeriod is higher than intervalSize, but not an exact multiple, it is adjusted by
	// rounding up to the next multiple.
	ExperimentIntervalMeasurements resample(quint64 resamplePeriod) const;

    // Index: interval
    QVector<GraphIntervalMeasurements> intervalMeasurements;
    GraphIntervalMeasurements globalMeasurements;

    quint64 tsStart;
	quint64 tsLast;
    quint64 intervalSize;
    int numEdges;
    int numPaths;
    int packetSizeThreshold;
};
QDataStream& operator>>(QDataStream& s, ExperimentIntervalMeasurements& d);
QDataStream& operator<<(QDataStream& s, const ExperimentIntervalMeasurements& d);

#endif // INTERVALMEASUREMENTS_H
