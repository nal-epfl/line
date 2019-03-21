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

#include "intervalmeasurements.h"

#include "../util/util.h"
#include "../util/json.h"

void GraphIntervalMeasurements::initialize(int numLinks,
										   int numPaths,
										   QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed)
{
    linkMeasurements.resize(numLinks);
    pathMeasurements.resize(numPaths);
	foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
		perPathLinkMeasurements[ep] = LinkIntervalMeasurement();
	}
}

void GraphIntervalMeasurements::clear()
{
	for (int i = 0; i < linkMeasurements.count(); i++) {
		linkMeasurements[i].clear();
	}
	for (int i = 0; i < pathMeasurements.count(); i++) {
		pathMeasurements[i].clear();
	}
	foreach (LinkPath ep, perPathLinkMeasurements.uniqueKeys()) {
		perPathLinkMeasurements[ep].clear();
	}
}

GraphIntervalMeasurements& GraphIntervalMeasurements::operator+=(GraphIntervalMeasurements other)
{
	for (int i = 0; i < qMin(this->linkMeasurements.count(), other.linkMeasurements.count()); i++) {
		this->linkMeasurements[i] += other.linkMeasurements[i];
	}
	for (int i = 0; i < qMin(this->pathMeasurements.count(), other.pathMeasurements.count()); i++) {
		this->pathMeasurements[i] += other.pathMeasurements[i];
	}
	foreach (LinkPath ep, this->perPathLinkMeasurements.uniqueKeys()) {
		if (other.perPathLinkMeasurements.contains(ep)) {
			this->perPathLinkMeasurements[ep] += other.perPathLinkMeasurements[ep];
		}
	}
    return *this;
}

QDataStream& operator<<(QDataStream& s, const GraphIntervalMeasurements& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.linkMeasurements;
    s << d.pathMeasurements;
    s << d.perPathLinkMeasurements;

    return s;
}

void GraphIntervalMeasurements::dump(QString indent)
{
    printf("%slinkMeasurements: %d items\n", indent.toLatin1().constData(), linkMeasurements.count());
    for (int i = 0; i < linkMeasurements.count(); i++) {
        printf("%s link: %d\n", indent.toLatin1().constData(), i);
        linkMeasurements[i].dump(indent + "  ");
    }
    printf("%spathMeasurements: %d items\n", indent.toLatin1().constData(), pathMeasurements.count());
    for (int i = 0; i < pathMeasurements.count(); i++) {
        printf("%s path: %d\n", indent.toLatin1().constData(), i);
        pathMeasurements[i].dump(indent + "  ");
    }
    printf("%sperPathLinkMeasurements: %d items\n", indent.toLatin1().constData(), perPathLinkMeasurements.count());
    foreach (LinkPath ep, perPathLinkMeasurements.keys()) {
        printf("%s link: %d\n", indent.toLatin1().constData(), ep.first);
        printf("%s path: %d\n", indent.toLatin1().constData(), ep.second);
        perPathLinkMeasurements[ep].dump(indent + "  ");
    }
}

QDataStream& operator>>(QDataStream& s, GraphIntervalMeasurements& d)
{
    qint32 ver;

    s >> ver;

    s >> d.linkMeasurements;
    s >> d.pathMeasurements;
    s >> d.perPathLinkMeasurements;

    return s;
}

void ExperimentIntervalMeasurements::initialize(quint64 tsStart,
												quint64 expectedDuration,
												quint64 intervalSize,
												int numLinks,
												int numPaths,
                                                QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed,
												int packetSizeThreshold)
{
	EndToEndMeasurements::initialize(tsStart,
									 expectedDuration,
									 intervalSize,
									 numLinks,
									 numPaths,
									 sparseRoutingMatrixTransposed,
									 packetSizeThreshold);

	globalMeasurements.initialize(numLinks, numPaths, sparseRoutingMatrixTransposed);
    intervalMeasurements.resize(numIntervals);
    for (int i = 0; i < numIntervals; i++) {
		intervalMeasurements[i].initialize(numLinks, numPaths, sparseRoutingMatrixTransposed);
	}
}

bool ExperimentIntervalMeasurements::recordPacketLink(PathPair pp, LinkPath ep, Timestamp tsIn, Timestamp tsOut, int size, bool forwarded, Timestamp delay)
{
	Q_UNUSED(pp);
	if (size < packetSizeThreshold)
		return false;
	int iIn = timestampToOpenInterval(tsIn);
	if (iIn < 0)
		return false;
	int iOut = timestampToOpenInterval(tsOut);
	if (iOut < 0)
		return false;

	tsLast = qMax(tsLast, tsIn);
	tsLast = qMax(tsLast, tsOut);
	globalMeasurements.linkMeasurements[ep.first].recordPacket(forwarded, delay);
	globalMeasurements.perPathLinkMeasurements[ep].recordPacket(forwarded, delay);

	for (int i = iIn; i <= iOut; i++) {
        intervalMeasurements[i].linkMeasurements[ep.first].recordPacket(forwarded, delay);
		intervalMeasurements[i].perPathLinkMeasurements[ep].recordPacket(forwarded,	delay);
	}
	return true;
}

bool ExperimentIntervalMeasurements::recordPacketPath(PathPair pp, LinkPath ep, Timestamp tsIn, Timestamp tsOut, int size, bool forwarded, Timestamp delay)
{
	Q_UNUSED(pp);
    if (size < packetSizeThreshold)
        return false;

    tsLast = qMax(tsLast, tsIn);
	tsLast = qMax(tsLast, tsOut);
	globalMeasurements.pathMeasurements[ep.second].recordPacket(forwarded, delay);

	int iIn = timestampToOpenInterval(tsIn);
	if (iIn < 0)
		return false;
	int iOut = timestampToOpenInterval(tsOut);
	if (iOut < 0)
		return false;

	for (int i = iIn; i <= iOut; i++) {
		intervalMeasurements[i].pathMeasurements[ep.second].recordPacket(forwarded, delay);
	}
	return true;
}

LinkIntervalMeasurement ExperimentIntervalMeasurements::readLink(PathPair pp, Link e, int i) const
{
	Q_UNUSED(pp);
	return this->intervalMeasurements[i].linkMeasurements[e];
}

LinkIntervalMeasurement ExperimentIntervalMeasurements::readLinkPath(PathPair pp, LinkPath ep, int i) const
{
	Q_UNUSED(pp);
	return this->intervalMeasurements[i].perPathLinkMeasurements[ep];
}

LinkIntervalMeasurement ExperimentIntervalMeasurements::readPath(PathPair pp, LinkPath ep, int i) const
{
	Q_UNUSED(pp);
	return this->intervalMeasurements[i].pathMeasurements[ep.second];
}

void ExperimentIntervalMeasurements::saveToStream(QDataStream &s)
{
	EndToEndMeasurements::saveToStream(s);
	s << *this;
}

void ExperimentIntervalMeasurements::loadFromStream(QDataStream &s)
{
	EndToEndMeasurements::loadFromStream(s);
    s >> *this;
}

void ExperimentIntervalMeasurements::trim()
{
    int lastIntervalWithData = -1;
    for (int i = 0; i < numIntervals; i++) {
        bool hasData = false;
        for (Link e = 0; e < numLinks; e++) {
            if (intervalMeasurements[i].linkMeasurements[e].numPacketsInFlight > 0) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            lastIntervalWithData = i;
            continue;
        }
        for (Path p = 0; p < numPaths; p++) {
            if (intervalMeasurements[i].pathMeasurements[p].numPacketsInFlight > 0) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            lastIntervalWithData = i;
            continue;
        }
    }
    if (lastIntervalWithData < numIntervals - 1) {
		numIntervals = lastIntervalWithData + 1;
        intervalMeasurements = intervalMeasurements.mid(0, numIntervals);
    }
}

QDataStream& operator<<(QDataStream& s, const ExperimentIntervalMeasurements& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.intervalMeasurements;
    s << d.globalMeasurements;

    return s;
}

void ExperimentIntervalMeasurements::dump(QString indent)
{
    EndToEndMeasurements::dump(indent);
    printf("%sintervalMeasurements: %d items\n", indent.toLatin1().constData(), intervalMeasurements.count());
    for (int i = 0; i < intervalMeasurements.count(); i++) {
        printf("%s interval: %d\n", indent.toLatin1().constData(), i);
        intervalMeasurements[i].dump(indent + "  ");
    }
    printf("%sglobalMeasurements:\n", indent.toLatin1().constData());
    globalMeasurements.dump(indent + " ");
}

QDataStream& operator>>(QDataStream& s, ExperimentIntervalMeasurements& d)
{
    qint32 ver;

    s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

    s >> d.intervalMeasurements;
    s >> d.globalMeasurements;

    return s;
}

ExperimentIntervalMeasurements ExperimentIntervalMeasurements::resample(int factor, int firstActiveInterval) const
{
	Q_ASSERT_FORCE(factor >= 1);

	ExperimentIntervalMeasurements result = *this;
	int oldNumIntervals = this->intervalMeasurements.count();

	result.intervalSize = this->intervalSize * factor;
	// Resample from the first active interval
	result.intervalMeasurements.resize((oldNumIntervals - firstActiveInterval + 1) / factor);
	result.numIntervals = result.intervalMeasurements.size();
//	qDebug() << result.numIntervals;

	for (int i = 0; i < result.intervalMeasurements.count(); i++) {
		result.intervalMeasurements[i].clear();
		for (int j = i * factor + firstActiveInterval;
			 j < qMin(i * factor + firstActiveInterval + factor, this->intervalMeasurements.count()); j++) {
			result.intervalMeasurements[i] += this->intervalMeasurements[j];
		}
	}
	return result;
}

ExperimentIntervalMeasurements ExperimentIntervalMeasurements::extractPathTuple(QList<Path> paths,
																				bool (*intervalFilter)(QList<LinkIntervalMeasurement>)) const
{
	ExperimentIntervalMeasurements result = *this;
	result.intervalMeasurements.clear();
	for (int i = 0; i < intervalMeasurements.count(); i++) {
		QList<LinkIntervalMeasurement> pathMeasurements;
		foreach (Path p, paths) {
			pathMeasurements.append(intervalMeasurements[i].pathMeasurements[p]);
		}
		if (intervalFilter(pathMeasurements)) {
			result.intervalMeasurements.append(intervalMeasurements[i]);
			GraphIntervalMeasurements &m = result.intervalMeasurements.last();
			for (Path p = 0; p < numPaths; p++) {
				if (!paths.contains(p)) {
					m.pathMeasurements[p].clear();
					for (Link e = 0; e < numLinks; e++) {
						m.perPathLinkMeasurements[LinkPath(e, p)].clear();
					}
				}
			}
		}
	}
	return result;
}

bool intervalFilterMin1Packet(QList<LinkIntervalMeasurement> measurements)
{
	bool keep = true;
	foreach (LinkIntervalMeasurement m, measurements) {
		if (m.numPacketsInFlight < 1) {
			keep = false;
			break;
		}
	}
	return keep;
}
