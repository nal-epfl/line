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

#ifndef RUN_EXPERIMENT_PARAMS_H
#define RUN_EXPERIMENT_PARAMS_H

#include <QtCore>

#include "chronometer.h"

class RunParams {
public:
	inline RunParams(QString experimentSuffix = QString(),
					 bool capture = false,
					 int capturePacketLimit = 0,
					 int captureEventLimit = 0,
					 quint64 intervalSize = 5000000000ULL,
					 quint64 estimatedDuration = 10000000000ULL,
					 qreal bufferBloatFactor = 1.0,
					 QString bufferSize = "large",
					 QString qosBufferScaling = "none",
					 QString queuingDiscipline = "drop-tail",
					 bool readjustQueues = false,
					 bool setTcpReceiveWindowSize = false,
					 bool setFixedTcpReceiveWindowSize = false,
					 int fixedTcpReceiveWindowSize = 0,
					 bool setAutoTcpReceiveWindowSize = false,
					 qreal scaleAutoTcpReceiveWindowSize = 0,
					 QByteArray graphSerializedCompressed = QByteArray(),
					 QString graphName = QString(),
					 QString workingDir = QString(),
					 int repeats = 1,
                     bool sampleAllTimelines = true,
                     bool flowTracking = false,
					 bool fakeEmulation = false,
					 bool realRouting = false,
					 QString clientHostName = QString(),
					 QString clientPort = QString(),
					 QString coreHostName = QString(),
					 QString corePort = QString(),
					 QList<QString> clientHostNames = QList<QString>(),
					 QList<QString> routerHostNames = QList<QString>(),
					 qreal congestedLinkFraction = 0.10,
					 qreal minProbCongestedLinkIsCongested = 0.200,
					 qreal maxProbCongestedLinkIsCongested = 0.500,
					 qreal minProbGoodLinkIsCongested = 0.000,
					 qreal maxProbGoodLinkIsCongested = 0.050,
					 qreal minLossRateOfCongestedLink = 0.025,
					 qreal maxLossRateOfCongestedLink = 0.075,
					 qreal minLossRateOfGoodLink = 0.000,
					 qreal maxLossRateOfGoodLink = 0.010,
					 qreal congestedRateNoise = 0.020,
					 qreal goodRateNoise = 0.007)
		: experimentSuffix(experimentSuffix),
		  capture(capture),
		  capturePacketLimit(capturePacketLimit),
		  captureEventLimit(captureEventLimit),
		  intervalSize(intervalSize),
		  estimatedDuration (estimatedDuration),
		  bufferBloatFactor(bufferBloatFactor),
		  bufferSize(bufferSize),
		  qosBufferScaling(qosBufferScaling),
		  queuingDiscipline(queuingDiscipline),
		  readjustQueues(readjustQueues),
		  setTcpReceiveWindowSize(setTcpReceiveWindowSize),
		  setFixedTcpReceiveWindowSize(setFixedTcpReceiveWindowSize),
		  fixedTcpReceiveWindowSize(fixedTcpReceiveWindowSize),
		  setAutoTcpReceiveWindowSize(setAutoTcpReceiveWindowSize),
		  scaleAutoTcpReceiveWindowSize(scaleAutoTcpReceiveWindowSize),
		  graphSerializedCompressed(graphSerializedCompressed),
		  graphName(graphName),
		  workingDir(workingDir),
		  repeats(repeats),
          sampleAllTimelines(sampleAllTimelines),
          flowTracking(flowTracking),
		  fakeEmulation(fakeEmulation),
		  realRouting(realRouting),
		  clientHostName(clientHostName),
		  clientPort(clientPort),
		  coreHostName(coreHostName),
		  corePort(corePort),
		  clientHostNames(clientHostNames),
		  routerHostNames(routerHostNames),
		  congestedLinkFraction(congestedLinkFraction),
		  minProbCongestedLinkIsCongested(minProbCongestedLinkIsCongested),
		  maxProbCongestedLinkIsCongested(maxProbCongestedLinkIsCongested),
		  minProbGoodLinkIsCongested(minProbGoodLinkIsCongested),
		  maxProbGoodLinkIsCongested(maxProbGoodLinkIsCongested),
		  minLossRateOfCongestedLink(minLossRateOfCongestedLink),
		  maxLossRateOfCongestedLink(maxLossRateOfCongestedLink),
		  minLossRateOfGoodLink(minLossRateOfGoodLink),
		  maxLossRateOfGoodLink(maxLossRateOfGoodLink),
		  congestedRateNoise(congestedRateNoise),
		  goodRateNoise(goodRateNoise) {}

	// All fractions stored in absolute values (0-1, not 0-100)
	QString experimentSuffix;
	bool capture;
	qint64 capturePacketLimit;
	qint64 captureEventLimit;
	quint64 intervalSize;
	quint64 estimatedDuration;
	qreal bufferBloatFactor;
	QString bufferSize;
	QString qosBufferScaling;
	QString queuingDiscipline;
	bool readjustQueues;
	bool setTcpReceiveWindowSize;
	bool setFixedTcpReceiveWindowSize;
	qint64 fixedTcpReceiveWindowSize;
	bool setAutoTcpReceiveWindowSize;
	qreal scaleAutoTcpReceiveWindowSize;
	// If graphSerialized is empty, graphInputPath is used
	QByteArray graphSerializedCompressed;
	// Full path and extension
	QString graphInputPath;
	// no path, no extension!
	QString graphName;
	QString workingDir;
	// only used in GUI
	qint64 repeats;
    bool sampleAllTimelines;
    bool flowTracking;
	bool fakeEmulation;
	bool realRouting;
	QString clientHostName;
	QString clientPort;
	QString coreHostName;
	QString corePort;
	// Used only for realRouting
	QList<QString> clientHostNames;
	QList<QString> routerHostNames;
	qreal congestedLinkFraction;
	// The probability that the links from the congested pool are congested, per interval, is sampled from a uniform
	// distribution bounded by these two values.
	qreal minProbCongestedLinkIsCongested;
	qreal maxProbCongestedLinkIsCongested;
	// The probability that the links from outside the congested pool are congested, per interval, is sampled from a
	// uniform distribution bounded by these two values.
	qreal minProbGoodLinkIsCongested;
	qreal maxProbGoodLinkIsCongested;
	// For each interval we flip a coin to decide if a link is congested or not, for each traffic class (taking into
	// account neutrality). After this decision, we must generate the loss rate.
	// The loss rate of the congested links, per interval, per class, is sampled from a uniform distribution bounded by
	// these two values.
	qreal minLossRateOfCongestedLink;
	qreal maxLossRateOfCongestedLink;
	// The loss rate of the good links, per interval, per class, is sampled from a uniform distribution bounded by
	// these two values.
	qreal minLossRateOfGoodLink;
	qreal maxLossRateOfGoodLink;
	// Uniform noise is added to obtain transmission rates per link, per interval, per flow.
	// This noise introduces additional non-neutrality, and will make neutral links behave slightly non-neutrally.
	// The noise is specified in additive units. It always increases the loss rate, never decreases it.
	qreal congestedRateNoise;
	qreal goodRateNoise;

	inline bool canRunInParallelWith(const RunParams &other) {
		if (!fakeEmulation && !other.fakeEmulation)
			return false;
		return true;
	}

	inline QString generateTestId(QString suffix = QString()) {
		return graphName +
				QDateTime::currentDateTime().toString(".yyyy.MM.dd.hh.mm.ss") +
				QString(".%1.%2").arg(getCurrentTimeNanosec()).arg(rand()) +
				QString("%1%2").arg(suffix.isEmpty() ? "" : "-").arg(suffix);
	}
};

bool saveRunParams(const RunParams &runParams,
				   QString fileName);
bool loadRunParams(RunParams &runParams,
				   QString fileName);
bool dumpRunParams(const RunParams &runParams,
				   QString fileName);

QDataStream& operator<<(QDataStream& s, const RunParams& d);
QDataStream& operator>>(QDataStream& s, RunParams& d);
QTextStream& operator<<(QTextStream& s, const RunParams& d);

#endif // RUN_EXPERIMENT_PARAMS_H
