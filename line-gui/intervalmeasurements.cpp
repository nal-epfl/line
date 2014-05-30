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

LinkIntervalMeasurement::LinkIntervalMeasurement()
{
	clear();
}

qreal LinkIntervalMeasurement::successRate(bool *ok) const
{
	if (numPacketsInFlight == 0) {
		if (ok) {
			*ok = false;
		}
		return 0.0;
	}
	if (ok) {
		*ok = true;
	}
	return 1.0 - qreal(numPacketsDropped) / qreal(numPacketsInFlight);
}

void LinkIntervalMeasurement::clear()
{
	numPacketsInFlight = 0;
	numPacketsDropped = 0;
    events.clear();
}

void LinkIntervalMeasurement::sample(int packetCount)
{
    if (packetCount > numPacketsInFlight)
        return;

    QVector<quint8> packetEvents = events.toVector();

    // Generate random events if they are missing
    if (packetEvents.size() != numPacketsInFlight) {
        packetEvents.resize(numPacketsInFlight);
        for (int i = 0; i < packetEvents.count(); i++) {
            packetEvents[i] = i < numPacketsDropped ? 0 : 1;
        }
        qShuffle(packetEvents);
    }

    // Sample the events
    QVector<quint8> sampledEvents = packetEvents;

    qShuffle(sampledEvents);
    sampledEvents.resize(packetCount);

    // Reconstruct measurement
    numPacketsInFlight = sampledEvents.count();
    numPacketsDropped = sampledEvents.count(0);
    events.clear();
    foreach (quint8 event, sampledEvents) {
        events.append(event);
    }
}

LinkIntervalMeasurement& LinkIntervalMeasurement::operator+=(LinkIntervalMeasurement other)
{
	this->numPacketsInFlight += other.numPacketsInFlight;
	this->numPacketsDropped += other.numPacketsDropped;
	return *this;
}

bool operator ==(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() == b.successRate();
}

bool operator !=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() != b.successRate();
}

bool operator <(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() < b.successRate();
}

bool operator <=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() <= b.successRate();
}

bool operator >(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() > b.successRate();
}

bool operator >=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() >= b.successRate();
}

QDataStream& operator<<(QDataStream& s, const LinkIntervalMeasurement& d) {
    qint32 ver = 4;
	s << ver;

	if (ver == 1) {
		quint64 v = d.numPacketsInFlight;
		s << v;
		v = d.numPacketsDropped;
		s << v;
	} else if (ver == 2) {
		quint16 v;
		v = d.numPacketsInFlight;
		s << v;
		v = d.numPacketsDropped;
		s << v;
	} else if (ver == 3) {
		writeNumberCompressed(s, d.numPacketsInFlight);
		writeNumberCompressed(s, d.numPacketsDropped);
    } else if (ver == 4) {
        writeNumberCompressed(s, d.numPacketsInFlight);
        writeNumberCompressed(s, d.numPacketsDropped);
        s << d.events;
    }

	return s;
}

QDataStream& operator>>(QDataStream& s, LinkIntervalMeasurement& d) {
    qint32 ver;
    s >> ver;

    d.clear();
	if (ver == 1) {
		quint64 v;
		s >> v;
		d.numPacketsInFlight = v;
		s >> v;
		d.numPacketsDropped = v;
	} else if (ver == 2) {
		quint16 v;
		s >> v;
		d.numPacketsInFlight = v;
		s >> v;
		d.numPacketsDropped = v;
	} else if (ver == 3) {
		readNumberCompressed(s, d.numPacketsInFlight);
		readNumberCompressed(s, d.numPacketsDropped);
    } else if (ver == 4) {
        readNumberCompressed(s, d.numPacketsInFlight);
        readNumberCompressed(s, d.numPacketsDropped);
        s >> d.events;
    } else {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		exit(-1);
	}

	return s;
}

void GraphIntervalMeasurements::initialize(int numEdges, int numPaths,
										   QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed)
{
    edgeMeasurements.resize(numEdges);
    pathMeasurements.resize(numPaths);
	foreach (QInt32Pair ep, sparseRoutingMatrixTransposed) {
		perPathEdgeMeasurements[ep] = LinkIntervalMeasurement();
	}
    // 833 packets/100ms corresponds to 100 Mbps
    // 1024 corresponds to the amount of weakly-bursty traffic over a 100Mbps link
    // 1024*4 for good measure
    // TODO allocate this based on the fastest link in the network
    const int bitmaskSize = 1024 * 4;
    for (qint32 e = 0; e < numEdges; e++) {
        edgeMeasurements[e].events.reserve(bitmaskSize);
    }
    for (qint32 p = 0; p < numPaths; p++) {
        pathMeasurements[p].events.reserve(bitmaskSize);
    }
    foreach (QInt32Pair ep, sparseRoutingMatrixTransposed) {
        perPathEdgeMeasurements[ep].events.reserve(bitmaskSize);
    }
}

void GraphIntervalMeasurements::clear()
{
	for (int i = 0; i < edgeMeasurements.count(); i++) {
		edgeMeasurements[i].clear();
	}
	for (int i = 0; i < pathMeasurements.count(); i++) {
		pathMeasurements[i].clear();
	}
	foreach (QInt32Pair edgePath, perPathEdgeMeasurements.uniqueKeys()) {
		perPathEdgeMeasurements[edgePath].clear();
	}
}

GraphIntervalMeasurements& GraphIntervalMeasurements::operator+=(GraphIntervalMeasurements other)
{
	for (int i = 0; i < qMin(this->edgeMeasurements.count(), other.edgeMeasurements.count()); i++) {
		this->edgeMeasurements[i] += other.edgeMeasurements[i];
	}
	for (int i = 0; i < qMin(this->pathMeasurements.count(), other.pathMeasurements.count()); i++) {
		this->pathMeasurements[i] += other.pathMeasurements[i];
	}
	foreach (QInt32Pair edgePath, this->perPathEdgeMeasurements.uniqueKeys()) {
		if (other.perPathEdgeMeasurements.contains(edgePath)) {
			this->perPathEdgeMeasurements[edgePath] += other.perPathEdgeMeasurements[edgePath];
		}
	}
	return *this;
}

QDataStream& operator<<(QDataStream& s, const GraphIntervalMeasurements& d)
{
    qint32 ver = 1;

    s << ver;

    s << d.edgeMeasurements;
    s << d.pathMeasurements;
    s << d.perPathEdgeMeasurements;

    return s;
}

QDataStream& operator>>(QDataStream& s, GraphIntervalMeasurements& d)
{
    qint32 ver;

    s >> ver;

    s >> d.edgeMeasurements;
    s >> d.pathMeasurements;
    s >> d.perPathEdgeMeasurements;

    return s;
}

void ExperimentIntervalMeasurements::initialize(quint64 tsStart,
												quint64 expectedDuration,
												quint64 intervalSize,
												int numEdges,
												int numPaths,
                                                QList<QPair<qint32, qint32> > sparseRoutingMatrixTransposed,
                                                int packetSizeThreshold)
{
    this->tsStart = tsStart;
	this->tsLast = tsStart;
    this->intervalSize = intervalSize;
    this->numEdges = numEdges;
    this->numPaths = numPaths;
    this->packetSizeThreshold = packetSizeThreshold;

	globalMeasurements.initialize(numEdges, numPaths, sparseRoutingMatrixTransposed);
	int numIntervals = qMin(10000, qMax(100, (int)((2ULL * expectedDuration) / intervalSize) + 10));
    intervalMeasurements.resize(numIntervals);
    for (int i = 0; i < numIntervals; i++) {
		intervalMeasurements[i].initialize(numEdges, numPaths, sparseRoutingMatrixTransposed);
    }
}

void ExperimentIntervalMeasurements::countPacketInFLightEdge(int edge, int path, quint64 tsIn, quint64 tsOut, int size,
                                                             int multiplier)
{
    if (size < packetSizeThreshold)
        return;

    tsLast = qMax(tsLast, tsIn);
	tsLast = qMax(tsLast, tsOut);
    if (edge >= 0) {
        globalMeasurements.edgeMeasurements[edge].numPacketsInFlight += multiplier;
        if (path >= 0) {
            QInt32Pair ep = QInt32Pair(edge, path);
            globalMeasurements.perPathEdgeMeasurements[ep].numPacketsInFlight += multiplier;
        }
    }

    int intervalIn = timestampToInterval(tsIn);
	if (intervalIn < 0)
		return;
    int intervalOut = timestampToOpenInterval(tsOut);
	if (intervalOut < 0)
		return;
    for (int interval = intervalIn; interval <= intervalOut; interval++) {
        if (edge >= 0) {
            intervalMeasurements[interval].edgeMeasurements[edge].numPacketsInFlight += multiplier;
            if (path >= 0) {
                QInt32Pair ep = QInt32Pair(edge, path);
                intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsInFlight += multiplier;
            }
        }
    }
}

void ExperimentIntervalMeasurements::countPacketInFLightPath(int path, quint64 tsIn, quint64 tsOut, int size, int multiplier)
{
    if (path < 0)
        return;
    if (size < packetSizeThreshold)
        return;

    tsLast = qMax(tsLast, tsIn);
	tsLast = qMax(tsLast, tsOut);
    globalMeasurements.pathMeasurements[path].numPacketsInFlight += multiplier;

    int intervalIn = timestampToInterval(tsIn);
	if (intervalIn < 0)
		return;
    int intervalOut = timestampToOpenInterval(tsOut);
	if (intervalOut < 0)
		return;
    for (int interval = intervalIn; interval <= intervalOut; interval++) {
        intervalMeasurements[interval].pathMeasurements[path].numPacketsInFlight += multiplier;
    }
}

void ExperimentIntervalMeasurements::countPacketDropped(int edge, int path, quint64 tsIn, quint64 tsDrop, int size, int multiplier)
{
    if (path < 0 || edge < 0)
        return;
    if (size < packetSizeThreshold)
        return;

	QInt32Pair ep = QInt32Pair(edge, path);
	tsLast = qMax(tsLast, tsDrop);
    globalMeasurements.edgeMeasurements[edge].numPacketsDropped += multiplier;
	globalMeasurements.perPathEdgeMeasurements[ep].numPacketsDropped += multiplier;
    globalMeasurements.pathMeasurements[path].numPacketsDropped += multiplier;

    int intervalIn = timestampToInterval(tsIn);
    if (intervalIn < 0)
        return;
    int intervalOut = timestampToOpenInterval(tsDrop);
    if (intervalOut < 0)
        return;
    for (int interval = intervalIn; interval <= intervalOut; interval++) {
        intervalMeasurements[interval].edgeMeasurements[edge].numPacketsDropped += multiplier;
        intervalMeasurements[interval].perPathEdgeMeasurements[ep].numPacketsDropped += multiplier;
        intervalMeasurements[interval].pathMeasurements[path].numPacketsDropped += multiplier;
    }
}

void ExperimentIntervalMeasurements::recordPacketEventPath(int path,
                                                           quint64 tsIn, quint64 tsOut,
                                                           int size, int multiplier,
                                                           bool forwarded)
{
    if (path < 0)
        return;
    if (size < packetSizeThreshold)
        return;

    tsLast = qMax(tsLast, tsIn);
    tsLast = qMax(tsLast, tsOut);

    int intervalIn = timestampToInterval(tsIn);
    if (intervalIn < 0)
        return;
    int intervalOut = timestampToOpenInterval(tsOut);
    if (intervalOut < 0)
        return;
    for (int interval = intervalIn; interval <= intervalOut; interval++) {
        for (int m = 0; m < multiplier; m++) {
            intervalMeasurements[interval].pathMeasurements[path].events.append(forwarded);
        }
    }
}

void ExperimentIntervalMeasurements::recordPacketEventEdge(int edge, int path,
                                                           quint64 tsIn, quint64 tsOut,
                                                           int size, int multiplier,
                                                           bool forwarded)
{
    if (size < packetSizeThreshold)
        return;

    tsLast = qMax(tsLast, tsIn);
    tsLast = qMax(tsLast, tsOut);

    int intervalIn = timestampToInterval(tsIn);
    if (intervalIn < 0)
        return;
    int intervalOut = timestampToOpenInterval(tsOut);
    if (intervalOut < 0)
        return;
    for (int interval = intervalIn; interval <= intervalOut; interval++) {
        if (edge >= 0) {
            for (int m = 0; m < multiplier; m++) {
                intervalMeasurements[interval].edgeMeasurements[edge].events.append(forwarded);
            }
            if (path >= 0) {
                QInt32Pair ep = QInt32Pair(edge, path);
                for (int m = 0; m < multiplier; m++) {
                    intervalMeasurements[interval].perPathEdgeMeasurements[ep].events.append(forwarded);
                }
            }
        }
    }
}

int ExperimentIntervalMeasurements::timestampToInterval(quint64 ts)
{
    int interval = (int)((ts - tsStart) / intervalSize);
    if (interval >= intervalMeasurements.count()) {
		interval = -1;
    }
    return interval;
}

int ExperimentIntervalMeasurements::timestampToOpenInterval(quint64 ts)
{
	int result;
    if (ts == tsStart) {
		result = 0;
    } else if ((ts - tsStart) % intervalSize != 0) {
		result = timestampToInterval(ts);
    } else {
		result = timestampToInterval(ts) - 1;
    }
	return result;
}

int ExperimentIntervalMeasurements::numIntervals()
{
	return intervalMeasurements.count();
}

bool ExperimentIntervalMeasurements::save(QString fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_4_0);

    out << *this;

    if (out.status() != QDataStream::Ok) {
        qDebug() << __FILE__ << __LINE__ << "Error writing file:" << file.fileName();
        return false;
    }
    return true;
}

bool ExperimentIntervalMeasurements::load(QString fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
        return false;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_4_0);

    in >> *this;

    if (in.status() != QDataStream::Ok) {
        qDebug() << __FILE__ << __LINE__ << "Error reading file:" << file.fileName();
        return false;
    }

    trim();
    return true;
}

void ExperimentIntervalMeasurements::trim()
{
    int lastIntervalWithData = -1;
    for (int i = 0; i < numIntervals(); i++) {
        bool hasData = false;
        for (int e = 0; e < numEdges; e++) {
            if (intervalMeasurements[i].edgeMeasurements[e].numPacketsInFlight > 0) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            lastIntervalWithData = i;
            continue;
        }
        for (int p = 0; p < numPaths; p++) {
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
    if (lastIntervalWithData < numIntervals() - 1) {
        intervalMeasurements = intervalMeasurements.mid(0, lastIntervalWithData + 1);
    }
}

bool ExperimentIntervalMeasurements::exportText(QString fileName,
												QVector<bool> trueEdgeNeutrality,
												QVector<int> pathTrafficClass)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}

	exportText(&file, trueEdgeNeutrality, pathTrafficClass);

	bool ok = file.flush();
	file.close();
	return ok;
}

void printLinkMeasurementValues(QTextStream &out,
								QVector<LinkIntervalMeasurement> data)
{
	out << "<table>";
	out << "<tr>";
	for (int i = 0; i < data.count(); i++) {
		out << "<th>" << (i + 1) << "</th>";
	}
	out << "</tr>";
	out << "<tr>";
	for (int i = 0; i < data.count(); i++) {
		out << "<td>";
		if (data[i].numPacketsInFlight > 0) {
			out << QString("%1 ").arg(data[i].successRate());
			out << QString("(%1/%2)")
				   .arg(data[i].numPacketsDropped)
				   .arg(data[i].numPacketsInFlight);
		} else {
			out << QString(".");
		}
		out << "</td>";
	}
	out << "</tr>";
	out << "</table>";
	out << endl;
}

void printLinkPathMeasurementValues(QTextStream &out,
									int nEdges,
									int nPaths,
									QHash<QPair<qint32, qint32>, LinkIntervalMeasurement> data)
{
	if (data.isEmpty())
		return;

	out << "<table>";
	out << "<tr>";
    out << "<th>Path\\Link</th>";
	for (int iEdge = 0; iEdge < nEdges; iEdge++) {
		out << "<th>" << (iEdge + 1) << "</th>";
	}
	out << "</tr>";
	for (int iPath = 0; iPath < nPaths; iPath++) {
		out << "<tr>";
		out << "<td>" << (iPath + 1) << "</td>";
		for (int iEdge = 0; iEdge < nEdges; iEdge++) {
			out << "<td>";
			QInt32Pair ep = QInt32Pair(iEdge, iPath);
			if (data[ep].numPacketsInFlight > 0) {
				out << QString("%1 ").arg(data[ep].successRate());
				out << QString("(%1/%2)")
					   .arg(data[ep].numPacketsDropped)
					   .arg(data[ep].numPacketsInFlight);
			} else {
				out << QString(".");
			}
			out << "</td>";
		}
		out << "</tr>";
		out << endl;
	}
	out << "</table>";
	out << endl;
}

bool ExperimentIntervalMeasurements::exportText(QIODevice *device,
												QVector<bool> trueEdgeNeutrality,
												QVector<int> pathTrafficClass)
{
	Q_UNUSED(trueEdgeNeutrality);
	QTextStream out(device);

	out << "<h1>Overall measurements</h1>" << endl;
	out << "<p>" << "Timestamp start (ns): " << QString::number(tsStart) << "</p>" << endl;
	out << "<p>" << "Timestamp end (ns): " << QString::number(tsLast) << "</p>" << endl;
	out << "<h2> Path transmission rates </h2>" << endl;
	printLinkMeasurementValues(out, globalMeasurements.pathMeasurements);
	out << endl;

	out << "<h2> Path transmission rates -- sorted </h2>" << endl;
	{
		QVector<LinkIntervalMeasurement> measurements = globalMeasurements.pathMeasurements;
		qSort(measurements);
		printLinkMeasurementValues(out, measurements);
	}
	out << endl;

    out << "<h2> Link transmission rates </h2>" << endl;
	printLinkMeasurementValues(out, globalMeasurements.edgeMeasurements);
	out << endl;

    out << "<h2> Link transmission rates -- sorted </h2>" << endl;
	{
		QVector<LinkIntervalMeasurement> measurements = globalMeasurements.edgeMeasurements;
		qSort(measurements);
		printLinkMeasurementValues(out, measurements);
	}
	out << endl;

    out << "<h2> Per path link transmission rates (rows = paths) </h2>" << endl;
	printLinkPathMeasurementValues(out, numEdges, numPaths, globalMeasurements.perPathEdgeMeasurements);
	out << endl;

	out << "<h1> Interval measurements </h1>" << endl;
	out << "<h2> Number of intervals </h2>" << endl;
	out << QString("%1").arg(numIntervals()) << endl;
	out << endl;
    for (int iInterval = 0; iInterval < numIntervals(); iInterval++) {
		out << "<h2 id=\"" << (iInterval + 1) << "\"> Interval " << (iInterval + 1) << "</h2>" << endl;
		out << "<h2> Path transmission rates </h2>" << endl;
		printLinkMeasurementValues(out, intervalMeasurements[iInterval].pathMeasurements);
		out << endl;

		out << "<h2> Path transmission rates -- sorted </h2>" << endl;
		{
			QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].pathMeasurements;
			qSort(measurements);
			printLinkMeasurementValues(out, measurements);
		}
		out << endl;

        out << "<h2> Link transmission rates </h2>" << endl;
		printLinkMeasurementValues(out, intervalMeasurements[iInterval].edgeMeasurements);
		out << endl;

        out << "<h2> Link transmission rates -- sorted </h2>" << endl;
		{
			QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].edgeMeasurements;
			qSort(measurements);
			printLinkMeasurementValues(out, measurements);
		}
		out << endl;

        out << "<h2> Per path link transmission rates (rows = paths) </h2>" << endl;
		printLinkPathMeasurementValues(out, numEdges, numPaths, intervalMeasurements[iInterval].perPathEdgeMeasurements);
		out << endl;
	}
	out << endl;

	// Listing
	{
		QSet<int> trafficClasses = QSet<int>::fromList(pathTrafficClass.toList());
		foreach (int trafficClass, trafficClasses) {
			out << "<p>" << QString("Traffic class %1:").arg(trafficClass) << "</p>" << endl;
			out << "<p>" << QString("Paths: ");
			for (int p = 0; p < numPaths; p++) {
				if (pathTrafficClass[p] != trafficClass)
					continue;
				out << QString("%1 ").arg(p + 1);
			}
			out << "</p>" << endl;
			out << endl;
		}
        out << "<p>" << QString("Neutral links: ");
		for (int e = 0; e < trueEdgeNeutrality.count(); e++) {
			if (trueEdgeNeutrality[e]) {
				out << QString("%1 ").arg(e + 1);
			}
		}
		out << "</p>" << endl;
        out << "<p>" << QString("Non-neutral links: ");
		for (int e = 0; e < trueEdgeNeutrality.count(); e++) {
			if (!trueEdgeNeutrality[e]) {
				out << QString("%1 ").arg(e + 1);
			}
		}
		out << "</p>" << endl;
		out << endl;
	}
	out << endl;

    // Statistics
    out << QString("Found %1 intervals of %2 seconds each (total experiment duration: %3 seconds)")
           .arg(numIntervals())
           .arg(intervalSize / 1000000000ULL)
           .arg((tsLast - tsStart) / 1000000000ULL) << endl;
    out << endl;
    {
        out << "Per interval path transmission statistics:" << endl;
        {
            QList<qreal> transmissionThresholds = QList<qreal>() << 0.0 << 0.85 << 0.90 << 0.95 << 0.97 << 0.98 << 0.99 << 1.0;
            // first index: interval, second index: histogram bin
            QList<QVector<qreal> > pathDistribution;
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].pathMeasurements;

                // histogram
                QVector<qreal> intervalDistribution(transmissionThresholds.count() - 1);
                // fill bins with counters
                for (int i = 0; i < measurements.count(); i++) {
                    if (measurements[i].numPacketsInFlight == 0)
                        continue;
                    qreal rate = measurements[i].successRate();
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        if (rate >= transmissionThresholds[bin] &&
                                rate <= transmissionThresholds[bin + 1]) {
                            intervalDistribution[bin] += 1.0;
                            break;
                        }
                    }
                }
                // normalize
                qreal sum = 0;
                for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                    sum += intervalDistribution[bin];
                }
                if (sum > 0) {
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        intervalDistribution[bin] /= sum;
                    }
                }
                pathDistribution << intervalDistribution;
            }
            out << "Rate         ";
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                out << QString("Interval %1    ").arg(iInterval + 1, 3);
            }
            out << endl;
            for (int bin = 0; bin < transmissionThresholds.count() - 1; bin++) {
                out << QString("%1-%2:   ").arg(transmissionThresholds[bin], 4, 'f', 2).arg(transmissionThresholds[bin + 1], 4, 'f', 2);
                for (int i = 0; i < numIntervals() - 2; i++) {
                    if (pathDistribution[i][bin] != 0) {
                        out << QString("%1%   ").arg(pathDistribution[i][bin] * 100, 12, 'f', 1);
                    } else {
                        out << QString("                ");
                    }
                }
                out << endl;
            }
        }
        out << endl;

		if (pathTrafficClass.count() == numPaths) {
			QSet<int> trafficClasses = QSet<int>::fromList(pathTrafficClass.toList());
			foreach (int trafficClass, trafficClasses) {
				out << QString("Traffic class %1: Per interval path transmission statistics:").arg(trafficClass) << endl;
				{
					QList<qreal> transmissionThresholds = QList<qreal>() << 0.0 << 0.85 << 0.90 << 0.95 << 0.97 << 0.98 << 0.99 << 1.0;
					// first index: interval, second index: histogram bin
					QList<QVector<qreal> > pathDistribution;
					for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
						QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].pathMeasurements;

						// histogram
						QVector<qreal> intervalDistribution(transmissionThresholds.count() - 1);
						// fill bins with counters
						for (int i = 0; i < measurements.count(); i++) {
							if (pathTrafficClass[i] != trafficClass)
								continue;
							if (measurements[i].numPacketsInFlight == 0)
								continue;
							qreal rate = measurements[i].successRate();
							for (int bin = 0; bin < intervalDistribution.count(); bin++) {
								if (rate >= transmissionThresholds[bin] &&
										rate <= transmissionThresholds[bin + 1]) {
									intervalDistribution[bin] += 1.0;
									break;
								}
							}
						}
						// normalize
						qreal sum = 0;
						for (int bin = 0; bin < intervalDistribution.count(); bin++) {
							sum += intervalDistribution[bin];
						}
						if (sum > 0) {
							for (int bin = 0; bin < intervalDistribution.count(); bin++) {
								intervalDistribution[bin] /= sum;
							}
						}
						pathDistribution << intervalDistribution;
					}
					out << "Rate         ";
					for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
						out << QString("Interval %1    ").arg(iInterval + 1, 3);
					}
					out << endl;
					for (int bin = 0; bin < transmissionThresholds.count() - 1; bin++) {
						out << QString("%1-%2:   ").arg(transmissionThresholds[bin], 4, 'f', 2).arg(transmissionThresholds[bin + 1], 4, 'f', 2);
						for (int i = 0; i < numIntervals() - 2; i++) {
							if (pathDistribution[i][bin] != 0) {
								out << QString("%1%   ").arg(pathDistribution[i][bin] * 100, 12, 'f', 1);
							} else {
								out << QString("                ");
							}
						}
						out << endl;
					}
				}
				out << endl;
			}
		}
        out << endl;
        out << endl;
        out << endl;

        out << "Per interval link transmission statistics:" << endl;
        {
            QList<qreal> transmissionThresholds = QList<qreal>() << 0.0 << 0.85 << 0.90 << 0.95 << 0.97 << 0.98 << 0.99 << 1.0;
            // first index: interval, second index: histogram bin
            QList<QVector<qreal> > edgeDistribution;
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].edgeMeasurements;

                // histogram
                QVector<qreal> intervalDistribution(transmissionThresholds.count() - 1);
                // fill bins with counters
                for (int i = 0; i < measurements.count(); i++) {
                    if (measurements[i].numPacketsInFlight == 0)
                        continue;
                    qreal rate = measurements[i].successRate();
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        if (rate >= transmissionThresholds[bin] &&
                                rate <= transmissionThresholds[bin + 1]) {
                            intervalDistribution[bin] += 1.0;
                            break;
                        }
                    }
                }
                // normalize
                qreal sum = 0;
                for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                    sum += intervalDistribution[bin];
                }
                if (sum > 0) {
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        intervalDistribution[bin] /= sum;
                    }
                }
                edgeDistribution << intervalDistribution;
            }
            out << "Rate         ";
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                out << QString("Interval %1    ").arg(iInterval + 1, 3);
            }
            out << endl;
            for (int bin = 0; bin < transmissionThresholds.count() - 1; bin++) {
                out << QString("%1-%2:   ").arg(transmissionThresholds[bin], 4, 'f', 2).arg(transmissionThresholds[bin + 1], 4, 'f', 2);
                for (int i = 0; i < numIntervals() - 2; i++) {
                    if (edgeDistribution[i][bin] != 0) {
                        out << QString("%1%   ").arg(edgeDistribution[i][bin] * 100, 12, 'f', 1);
                    } else {
                        out << QString("                ");
                    }
                }
                out << endl;
            }
        }
        out << endl;

        out << "Neutral links (by policy): Per interval edge transmission statistics:" << endl;
        {
            QList<qreal> transmissionThresholds = QList<qreal>() << 0.0 << 0.85 << 0.90 << 0.95 << 0.97 << 0.98 << 0.99 << 1.0;
            // first index: interval, second index: histogram bin
            QList<QVector<qreal> > edgeDistribution;
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].edgeMeasurements;

                // histogram
                QVector<qreal> intervalDistribution(transmissionThresholds.count() - 1);
                // fill bins with counters
                for (int e = 0; e < measurements.count(); e++) {
                    if (!trueEdgeNeutrality[e])
                        continue;
                    if (measurements[e].numPacketsInFlight == 0)
                        continue;
                    qreal rate = measurements[e].successRate();
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        if (rate >= transmissionThresholds[bin] &&
                                rate <= transmissionThresholds[bin + 1]) {
                            intervalDistribution[bin] += 1.0;
                            break;
                        }
                    }
                }
                // normalize
                qreal sum = 0;
                for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                    sum += intervalDistribution[bin];
                }
                if (sum > 0) {
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        intervalDistribution[bin] /= sum;
                    }
                }
                edgeDistribution << intervalDistribution;
            }
            out << "Rate         ";
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                out << QString("Interval %1    ").arg(iInterval + 1, 3);
            }
            out << endl;
            for (int bin = 0; bin < transmissionThresholds.count() - 1; bin++) {
                out << QString("%1-%2:   ").arg(transmissionThresholds[bin], 4, 'f', 2).arg(transmissionThresholds[bin + 1], 4, 'f', 2);
                for (int i = 0; i < numIntervals() - 2; i++) {
                    if (edgeDistribution[i][bin] != 0) {
                        out << QString("%1%   ").arg(edgeDistribution[i][bin] * 100, 12, 'f', 1);
                    } else {
                        out << QString("                ");
                    }
                }
                out << endl;
            }
        }
        out << endl;

        out << "Non-neutral links (by policy): Per interval edge transmission statistics:" << endl;
        {
            QList<qreal> transmissionThresholds = QList<qreal>() << 0.0 << 0.85 << 0.90 << 0.95 << 0.97 << 0.98 << 0.99 << 1.0;
            // first index: interval, second index: histogram bin
            QList<QVector<qreal> > edgeDistribution;
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                QVector<LinkIntervalMeasurement> measurements = intervalMeasurements[iInterval].edgeMeasurements;

                // histogram
                QVector<qreal> intervalDistribution(transmissionThresholds.count() - 1);
                // fill bins with counters
                for (int e = 0; e < measurements.count(); e++) {
                    if (trueEdgeNeutrality[e])
                        continue;
                    if (measurements[e].numPacketsInFlight == 0)
                        continue;
                    qreal rate = measurements[e].successRate();
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        if (rate >= transmissionThresholds[bin] &&
                                rate <= transmissionThresholds[bin + 1]) {
                            intervalDistribution[bin] += 1.0;
                            break;
                        }
                    }
                }
                // normalize
                qreal sum = 0;
                for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                    sum += intervalDistribution[bin];
                }
                if (sum > 0) {
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        intervalDistribution[bin] /= sum;
                    }
                }
                edgeDistribution << intervalDistribution;
            }
            out << "Rate         ";
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                out << QString("Interval %1    ").arg(iInterval + 1, 3);
            }
            out << endl;
            for (int bin = 0; bin < transmissionThresholds.count() - 1; bin++) {
                out << QString("%1-%2:   ").arg(transmissionThresholds[bin], 4, 'f', 2).arg(transmissionThresholds[bin + 1], 4, 'f', 2);
                for (int i = 0; i < numIntervals() - 2; i++) {
                    if (edgeDistribution[i][bin] != 0) {
                        out << QString("%1%   ").arg(edgeDistribution[i][bin] * 100, 12, 'f', 1);
                    } else {
                        out << QString("                ");
                    }
                }
                out << endl;
            }
        }
        out << endl;
        out << endl;
        out << endl;

        out << "Per interval link non-neutrality statistics (rates are absolute deltas):" << endl;
        {
            QList<qreal> nonNeutralityThresholds = QList<qreal>() << 0.0 << 0.01 << 0.02 << 0.03 << 0.05 << 0.10 << 0.15 << 1.0;
            // first index: interval, second index: histogram bin
            QList<QVector<qreal> > edgeNonNeutralityDistribution;
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                // first index: edge; second index: path
				QHash<QPair<qint32, qint32>, LinkIntervalMeasurement> pathEdgeMeasurements =
						intervalMeasurements[iInterval].perPathEdgeMeasurements;

                // histogram
                QVector<qreal> intervalDistribution(nonNeutralityThresholds.count() - 1);
                // fill bins with counters
				for (int e = 0; e < numEdges; e++) {
                    QList<qreal> rates;
					for (int p = 0; p < numPaths; p++) {
						QInt32Pair ep = QInt32Pair(e, p);
						if (pathEdgeMeasurements[ep].numPacketsInFlight == 0)
                            continue;
						rates << pathEdgeMeasurements[ep].successRate();
                    }
                    qreal nonNeutrality;
                    if (rates.count() < 2) {
                        nonNeutrality = 0.0;
                    } else {
                        qreal rateMin = rates.first();
                        foreach (qreal rate, rates) {
                            rateMin = qMin(rateMin, rate);
                        }
                        qreal rateMax = rates.first();
                        foreach (qreal rate, rates) {
                            rateMax = qMax(rateMax, rate);
                        }
                        // between 0 and 1, 0 means neutral, 1 means insanely non-neutral
                        nonNeutrality = rateMax - rateMin;
                    }
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        if (nonNeutrality >= nonNeutralityThresholds[bin] &&
                                nonNeutrality <= nonNeutralityThresholds[bin + 1]) {
                            intervalDistribution[bin] += 1.0;
                            break;
                        }
                    }
                }
                // normalize
                qreal sum = 0;
                for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                    sum += intervalDistribution[bin];
                }
                if (sum > 0) {
                    for (int bin = 0; bin < intervalDistribution.count(); bin++) {
                        intervalDistribution[bin] /= sum;
                    }
                }
                edgeNonNeutralityDistribution << intervalDistribution;
            }
            out << "Non-neutrality ";
            for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
                out << QString("Interval %1    ").arg(iInterval + 1, 3);
            }
            out << endl;
            for (int bin = 0; bin < nonNeutralityThresholds.count() - 1; bin++) {
                out << QString("%1-%2:     ").arg(nonNeutralityThresholds[bin], 4, 'f', 2).arg(nonNeutralityThresholds[bin + 1], 4, 'f', 2);
                for (int i = 0; i < numIntervals() - 2; i++) {
                    if (edgeNonNeutralityDistribution[i][bin] != 0) {
                        out << QString("%1%   ").arg(edgeNonNeutralityDistribution[i][bin] * 100, 12, 'f', 1);
                    } else {
                        out << QString("                ");
                    }
                }
                out << endl;
            }
        }
        out << endl;

		if (trueEdgeNeutrality.count() == numEdges) {
            out << "Neutral links (by policy): per interval link non-neutrality statistics (rates are absolute deltas):" << endl;
			{
				QList<qreal> nonNeutralityThresholds = QList<qreal>() << 0.0 << 0.01 << 0.02 << 0.03 << 0.05 << 0.10 << 0.15 << 1.0;
				// first index: interval, second index: histogram bin
				QList<QVector<qreal> > edgeNonNeutralityDistribution;
				for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
					// first index: edge; second index: path
					QHash<QPair<qint32, qint32>, LinkIntervalMeasurement> pathEdgeMeasurements =
							intervalMeasurements[iInterval].perPathEdgeMeasurements;

					// histogram
					QVector<qreal> intervalDistribution(nonNeutralityThresholds.count() - 1);
					// fill bins with counters
					for (int e = 0; e < numEdges; e++) {
						if (!trueEdgeNeutrality[e])
							continue;
						QList<qreal> rates;
						for (int p = 0; p < numPaths; p++) {
							QInt32Pair ep = QInt32Pair(e, p);
							if (pathEdgeMeasurements[ep].numPacketsInFlight == 0)
								continue;
							rates << pathEdgeMeasurements[ep].successRate();
						}
						qreal nonNeutrality;
						if (rates.count() < 2) {
							nonNeutrality = 0.0;
						} else {
							qreal rateMin = rates.first();
							foreach (qreal rate, rates) {
								rateMin = qMin(rateMin, rate);
							}
							qreal rateMax = rates.first();
							foreach (qreal rate, rates) {
								rateMax = qMax(rateMax, rate);
							}
							// between 0 and 1, 0 means neutral, 1 means insanely non-neutral
							nonNeutrality = rateMax - rateMin;
						}
						for (int bin = 0; bin < intervalDistribution.count(); bin++) {
							if (nonNeutrality >= nonNeutralityThresholds[bin] &&
									nonNeutrality <= nonNeutralityThresholds[bin + 1]) {
								intervalDistribution[bin] += 1.0;
								break;
							}
						}
					}
					// normalize
					qreal sum = 0;
					for (int bin = 0; bin < intervalDistribution.count(); bin++) {
						sum += intervalDistribution[bin];
					}
					if (sum > 0) {
						for (int bin = 0; bin < intervalDistribution.count(); bin++) {
							intervalDistribution[bin] /= sum;
						}
					}
					edgeNonNeutralityDistribution << intervalDistribution;
				}
				out << "Non-neutrality ";
				for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
					out << QString("Interval %1    ").arg(iInterval + 1, 3);
				}
				out << endl;
				for (int bin = 0; bin < nonNeutralityThresholds.count() - 1; bin++) {
					out << QString("%1-%2:     ").arg(nonNeutralityThresholds[bin], 4, 'f', 2).arg(nonNeutralityThresholds[bin + 1], 4, 'f', 2);
					for (int i = 0; i < numIntervals() - 2; i++) {
						if (edgeNonNeutralityDistribution[i][bin] != 0) {
							out << QString("%1%   ").arg(edgeNonNeutralityDistribution[i][bin] * 100, 12, 'f', 1);
						} else {
							out << QString("                ");
						}
					}
					out << endl;
				}
			}
			out << endl;

            out << "Non-neutral links (by policy): per interval link non-neutrality statistics (rates are absolute deltas):" << endl;
			{
				QList<qreal> nonNeutralityThresholds = QList<qreal>() << 0.0 << 0.01 << 0.02 << 0.03 << 0.05 << 0.10 << 0.15 << 1.0;
				// first index: interval, second index: histogram bin
				QList<QVector<qreal> > edgeNonNeutralityDistribution;
				for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
					// first index: edge; second index: path
					QHash<QPair<qint32, qint32>, LinkIntervalMeasurement> pathEdgeMeasurements =
							intervalMeasurements[iInterval].perPathEdgeMeasurements;

					// histogram
					QVector<qreal> intervalDistribution(nonNeutralityThresholds.count() - 1);
					// fill bins with counters
					for (int e = 0; e < numEdges; e++) {
						if (trueEdgeNeutrality[e])
							continue;
						QList<qreal> rates;
						for (int p = 0; p < numPaths; p++) {
							QInt32Pair ep = QInt32Pair(e, p);
							if (pathEdgeMeasurements[ep].numPacketsInFlight == 0)
								continue;
							rates << pathEdgeMeasurements[ep].successRate();
						}
						qreal nonNeutrality;
						if (rates.count() < 2) {
							nonNeutrality = 0.0;
						} else {
							qreal rateMin = rates.first();
							foreach (qreal rate, rates) {
								rateMin = qMin(rateMin, rate);
							}
							qreal rateMax = rates.first();
							foreach (qreal rate, rates) {
								rateMax = qMax(rateMax, rate);
							}
							// between 0 and 1, 0 means neutral, 1 means insanely non-neutral
							nonNeutrality = rateMax - rateMin;
						}
						for (int bin = 0; bin < intervalDistribution.count(); bin++) {
							if (nonNeutrality >= nonNeutralityThresholds[bin] &&
									nonNeutrality <= nonNeutralityThresholds[bin + 1]) {
								intervalDistribution[bin] += 1.0;
								break;
							}
						}
					}
					// normalize
					qreal sum = 0;
					for (int bin = 0; bin < intervalDistribution.count(); bin++) {
						sum += intervalDistribution[bin];
					}
					if (sum > 0) {
						for (int bin = 0; bin < intervalDistribution.count(); bin++) {
							intervalDistribution[bin] /= sum;
						}
					}
					edgeNonNeutralityDistribution << intervalDistribution;
				}
				out << "Non-neutrality ";
				for (int iInterval = 1; iInterval < numIntervals() - 1; iInterval++) {
					out << QString("Interval %1    ").arg(iInterval + 1, 3);
				}
				out << endl;
				for (int bin = 0; bin < nonNeutralityThresholds.count() - 1; bin++) {
					out << QString("%1-%2:     ").arg(nonNeutralityThresholds[bin], 4, 'f', 2).arg(nonNeutralityThresholds[bin + 1], 4, 'f', 2);
					for (int i = 0; i < numIntervals() - 2; i++) {
						if (edgeNonNeutralityDistribution[i][bin] != 0) {
							out << QString("%1%   ").arg(edgeNonNeutralityDistribution[i][bin] * 100, 12, 'f', 1);
						} else {
							out << QString("                ");
						}
					}
					out << endl;
				}
			}
			out << endl;
		}
    }

	out.flush();
	return true;
}

ExperimentIntervalMeasurements ExperimentIntervalMeasurements::resample(quint64 resamplePeriod) const
{
	// Nothing to do
	if (resamplePeriod <= intervalSize) {
		return *this;
	}

	// Round it up
	if (resamplePeriod % intervalSize != 0) {
		resamplePeriod = (resamplePeriod / intervalSize + 1) * intervalSize;
	}
	int factor = resamplePeriod / this->intervalSize;
	// factor > 1

	ExperimentIntervalMeasurements result = *this;
	result.intervalSize = resamplePeriod;
	result.intervalMeasurements.resize(this->intervalMeasurements.count() / factor + ((this->intervalMeasurements.count() % factor) ? 1 : 0));

	for (int i = 0; i < result.intervalMeasurements.count(); i++) {
		result.intervalMeasurements[i].clear();
		for (int j = i * factor; j < qMin(i * factor + factor, this->intervalMeasurements.count()); j++) {
			result.intervalMeasurements[i] += this->intervalMeasurements[j];
		}
	}
	return result;
}

QDataStream& operator<<(QDataStream& s, const ExperimentIntervalMeasurements& d)
{
    qint32 ver = 2;

    s << ver;

    s << d.intervalMeasurements;
    s << d.globalMeasurements;
    s << d.tsStart;
	s << d.tsLast;
    s << d.intervalSize;
    s << d.numEdges;
    s << d.numPaths;

    if (ver >= 2) {
        s << d.packetSizeThreshold;
    }

    return s;
}

QDataStream& operator>>(QDataStream& s, ExperimentIntervalMeasurements& d)
{
    qint32 ver;

    s >> ver;

    s >> d.intervalMeasurements;
    s >> d.globalMeasurements;
    s >> d.tsStart;
	s >> d.tsLast;
    s >> d.intervalSize;
    s >> d.numEdges;
    s >> d.numPaths;

    if (ver >= 2) {
        s >> d.packetSizeThreshold;
    } else {
        d.packetSizeThreshold = 0;
    }

    return s;
}
