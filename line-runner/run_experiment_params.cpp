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

#include "run_experiment_params.h"
#include "util.h"

bool saveRunParams(const RunParams &runParams,
						 QString fileName) {
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qError() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	QByteArray buffer;
	{
		QDataStream raw(&buffer, QIODevice::WriteOnly);
		raw << runParams;
	}
	buffer = qCompress(buffer);
	out << buffer;
	return true;
}

bool loadRunParams(RunParams &runParams,
				   QString fileName) {
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qError() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	QByteArray buffer;
	in >> buffer;
	if (in.status() != QDataStream::Ok) {
		qError() << __FILE__ << __LINE__ << "Read error:" << file.fileName();
		Q_ASSERT_FORCE(false);
	}
	buffer = qUncompress(buffer);
	{
		QDataStream raw(&buffer, QIODevice::ReadOnly);
		raw >> runParams;
	}
	return true;
}

bool dumpRunParams(const RunParams &runParams,
						 QString fileName) {
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qError() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		return false;
	}
	QTextStream out(&file);

	out << runParams;
	return true;
}

QDataStream& operator<<(QDataStream& s, const RunParams& d) {
    qint32 ver = 4;
	s << ver;

	if (ver >= 1) {
		s << d.experimentSuffix;
		s << d.capture;
		s << d.capturePacketLimit;
		s << d.captureEventLimit;
		s << d.intervalSize;
		s << d.estimatedDuration;
		s << d.bufferBloatFactor;
		s << d.readjustQueues;
		s << d.setTcpReceiveWindowSize;
		s << d.setFixedTcpReceiveWindowSize;
		s << d.fixedTcpReceiveWindowSize;
		s << d.setAutoTcpReceiveWindowSize;
		s << d.scaleAutoTcpReceiveWindowSize;
		s << d.graphSerializedCompressed;
		s << d.graphInputPath;
		s << d.graphName;
		s << d.workingDir;
		s << d.repeats;
		s << d.fakeEmulation;
		s << d.clientHostName;
		s << d.clientPort;
		s << d.coreHostName;
		s << d.corePort;
		s << d.congestedLinkFraction;
		s << d.minProbCongestedLinkIsCongested;
		s << d.maxProbCongestedLinkIsCongested;
		s << d.minProbGoodLinkIsCongested;
		s << d.maxProbGoodLinkIsCongested;
		s << d.minLossRateOfCongestedLink;
		s << d.maxLossRateOfCongestedLink;
		s << d.minLossRateOfGoodLink;
		s << d.maxLossRateOfGoodLink;
		s << d.congestedRateNoise;
		s << d.goodRateNoise;
	}
	if (ver >= 2) {
		s << d.realRouting;
		s << d.clientHostNames;
		s << d.routerHostNames;
	}
	if (ver >= 3) {
		s << d.bufferSize;
		s << d.qosBufferScaling;
		s << d.queuingDiscipline;
	}
    if (ver >= 4) {
        s << d.sampleAllTimelines;
        s << d.flowTracking;
    }

	return s;
}

QDataStream& operator>>(QDataStream& s, RunParams& d) {
	qint32 ver;
	s >> ver;

	if (ver >= 1) {
		s >> d.experimentSuffix;
		s >> d.capture;
		s >> d.capturePacketLimit;
		s >> d.captureEventLimit;
		s >> d.intervalSize;
		s >> d.estimatedDuration;
		s >> d.bufferBloatFactor;
		s >> d.readjustQueues;
		s >> d.setTcpReceiveWindowSize;
		s >> d.setFixedTcpReceiveWindowSize;
		s >> d.fixedTcpReceiveWindowSize;
		s >> d.setAutoTcpReceiveWindowSize;
		s >> d.scaleAutoTcpReceiveWindowSize;
		s >> d.graphSerializedCompressed;
		s >> d.graphInputPath;
		s >> d.graphName;
		s >> d.workingDir;
		s >> d.repeats;
		s >> d.fakeEmulation;
		s >> d.clientHostName;
		s >> d.clientPort;
		s >> d.coreHostName;
		s >> d.corePort;
		s >> d.congestedLinkFraction;
		s >> d.minProbCongestedLinkIsCongested;
		s >> d.maxProbCongestedLinkIsCongested;
		s >> d.minProbGoodLinkIsCongested;
		s >> d.maxProbGoodLinkIsCongested;
		s >> d.minLossRateOfCongestedLink;
		s >> d.maxLossRateOfCongestedLink;
		s >> d.minLossRateOfGoodLink;
		s >> d.maxLossRateOfGoodLink;
		s >> d.congestedRateNoise;
		s >> d.goodRateNoise;
	}
	if (ver >= 2) {
		s >> d.realRouting;
		s >> d.clientHostNames;
		s >> d.routerHostNames;
	} else {
		d.realRouting = false;
	}
	if (ver >= 3) {
		s >> d.bufferSize;
		s >> d.qosBufferScaling;
		s >> d.queuingDiscipline;
	}
    if (ver >= 4) {
        s >> d.sampleAllTimelines;
        s >> d.flowTracking;
    }
    if (ver < 1 || ver > 4) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		exit(-1);
	}

	return s;
}

QTextStream& operator<<(QTextStream& s, const RunParams& d) {
	s << "RunParams = {" << endl;
	s << "experimentSuffix = " << d.experimentSuffix << endl;
	s << "capture = " << d.capture << endl;
	s << "capturePacketLimit = " << d.capturePacketLimit << endl;
	s << "captureEventLimit = " << d.captureEventLimit << endl;
	s << "intervalSize = " << d.intervalSize << endl;
	s << "estimatedDuration = " << d.estimatedDuration << endl;
	s << "bufferBloatFactor = " << d.bufferBloatFactor << endl;
	s << "bufferSize = " << d.bufferSize << endl;
	s << "QoSBufferScaling = " << d.qosBufferScaling << endl;
	s << "queuingDiscipline = " << d.queuingDiscipline << endl;
	s << "readjustQueues = " << d.readjustQueues << endl;
	s << "setTcpReceiveWindowSize = " << d.setTcpReceiveWindowSize << endl;
	s << "setFixedTcpReceiveWindowSize = " << d.setFixedTcpReceiveWindowSize << endl;
	s << "fixedTcpReceiveWindowSize = " << d.fixedTcpReceiveWindowSize << endl;
	s << "setAutoTcpReceiveWindowSize = " << d.setAutoTcpReceiveWindowSize << endl;
	s << "scaleAutoTcpReceiveWindowSize = " << d.scaleAutoTcpReceiveWindowSize << endl;
	s << "graphSerializedCompressed = " << "<not available>" << endl;
	s << "graphInputPath = " << d.graphInputPath << endl;
	s << "graphName = " << d.graphName << endl;
	s << "workingDir = " << d.workingDir << endl;
	s << "repeats = " << d.repeats << endl;
    s << "sampleAllTimelines = " << d.sampleAllTimelines << endl;
    s << "flowTracking = " << d.flowTracking << endl;
	s << "fakeEmulation = " << d.fakeEmulation << endl;
	s << "realRouting = " << d.realRouting << endl;
	s << "clientHostName = " << d.clientHostName << endl;
	s << "clientPort = " << d.clientPort << endl;
	s << "coreHostName = " << d.coreHostName << endl;
	s << "corePort = " << d.corePort << endl;
	s << "clientHostNames = " << QStringList(d.clientHostNames).join(" ") << endl;
	s << "routerHostNames = " << QStringList(d.routerHostNames).join(" ") << endl;
	s << "congestedLinkFraction = " << d.congestedLinkFraction << endl;
	s << "minProbCongestedLinkIsCongested = " << d.minProbCongestedLinkIsCongested << endl;
	s << "maxProbCongestedLinkIsCongested = " << d.maxProbCongestedLinkIsCongested << endl;
	s << "minProbGoodLinkIsCongested = " << d.minProbGoodLinkIsCongested << endl;
	s << "maxProbGoodLinkIsCongested = " << d.maxProbGoodLinkIsCongested << endl;
	s << "minLossRateOfCongestedLink = " << d.minLossRateOfCongestedLink << endl;
	s << "maxLossRateOfCongestedLink = " << d.maxLossRateOfCongestedLink << endl;
	s << "minLossRateOfGoodLink = " << d.minLossRateOfGoodLink << endl;
	s << "maxLossRateOfGoodLink = " << d.maxLossRateOfGoodLink << endl;
	s << "congestedRateNoise = " << d.congestedRateNoise << endl;
	s << "goodRateNoise = " << d.goodRateNoise << endl;
	s << "}" << endl;

	return s;
}
