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

#include "end_to_end_measurements.h"

class GraphIntervalMeasurements
{
public:
	void initialize(int numEdges, int numPaths, QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed);
	// Sets all the counters to zero
	void clear();
	// Index: edge
    QVector<LinkIntervalMeasurement> linkMeasurements;
    // Index: path
    QVector<LinkIntervalMeasurement> pathMeasurements;
    // first index: link; second index: path
	QHash<LinkPath, LinkIntervalMeasurement> perPathLinkMeasurements;
	// The object other must have been initialized with the same routing matrix.
	GraphIntervalMeasurements& operator+=(GraphIntervalMeasurements other);
};
QDataStream& operator>>(QDataStream& s, GraphIntervalMeasurements& d);
QDataStream& operator<<(QDataStream& s, const GraphIntervalMeasurements& d);

class ExperimentIntervalMeasurements : public EndToEndMeasurements
{
public:
    void initialize(quint64 tsStart,
                    quint64 expectedDuration,
                    quint64 intervalSize,
                    int numLinks,
					int numPaths,
                    QList<LinkPath> sparseRoutingMatrixTransposed,
					int packetSizeThreshold);

	bool recordPacketLink(PathPair pp, LinkPath ep, Timestamp tsIn, Timestamp tsOut, int size, bool forwarded, Timestamp delay);
	bool recordPacketPath(PathPair pp, LinkPath ep, Timestamp tsIn, Timestamp tsOut, int size, bool forwarded, Timestamp delay);

	LinkIntervalMeasurement readLink(PathPair pp, Link e, int i) const;
	LinkIntervalMeasurement readLinkPath(PathPair pp, LinkPath ep, int i) const;
	LinkIntervalMeasurement readPath(PathPair pp, LinkPath ep, int i) const;

    void trim();

	void saveToStream(QDataStream &s);
	void loadFromStream(QDataStream &s);

	// Returns a new ExperimentIntervalMeasurements object resampled at an exact multiple of intervalSize.
    ExperimentIntervalMeasurements resample(int factor, int firstActiveInterval = 0) const;

	// Returns a new ExperimentIntervalMeasurements object containing traffic only for the specified paths, with
	// intervals filtered by the given function (returns true if the interval should be kept)
	// See intervalFilterMin1Packet as an example filtering function.
	ExperimentIntervalMeasurements extractPathTuple(QList<Path> paths,
													bool (*intervalFilter)(QList<LinkIntervalMeasurement> measurements)) const;

    // Index: interval
    QVector<GraphIntervalMeasurements> intervalMeasurements;
	GraphIntervalMeasurements globalMeasurements;
};
QDataStream& operator>>(QDataStream& s, ExperimentIntervalMeasurements& d);
QDataStream& operator<<(QDataStream& s, const ExperimentIntervalMeasurements& d);

bool intervalFilterMin1Packet(QList<LinkIntervalMeasurement> measurements);

#endif // INTERVALMEASUREMENTS_H
