/*
 *	Copyright (C) 2015 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license under which this program may be distributed.
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

#include "erroranalysisdata.h"

#include "intervalmeasurements.h"
#include "netgraph.h"
#include "run_experiment_params.h"
#include "tomodata.h"
#include "json.h"

QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathIntervalData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.used;
	s << d.goodLocally;
	s << d.goodEnd2End;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathIntervalData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.used;
	s >> d.goodLocally;
	s >> d.goodEnd2End;

	return s;
}

QString toJson(const ErrorAnalysisPathIntervalData &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.used);
	jsonObjectPrinterAddMember(p, d.goodLocally);
	jsonObjectPrinterAddMember(p, d.goodEnd2End);
	return p.json();
}


QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathPairIntervalData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.used;
	s << d.p1Data;
	s << d.p2Data;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathPairIntervalData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.used;
	s >> d.p1Data;
	s >> d.p2Data;

	return s;
}

QString toJson(const ErrorAnalysisPathPairIntervalData &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.used);
	jsonObjectPrinterAddMember(p, d.p1Data);
	jsonObjectPrinterAddMember(p, d.p2Data);
	return p.json();
}


QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathPairData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.pathPair;
	s << d.intervals;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathPairData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.pathPair;
	s >> d.intervals;

	return s;
}

QString toJson(const ErrorAnalysisPathPairData &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.pathPair);
	jsonObjectPrinterAddMember(p, d.intervals);
	return p.json();
}


QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.path;
	s << d.intervals;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.path;
	s >> d.intervals;

	return s;
}

QString toJson(const ErrorAnalysisPathData &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.path);
	jsonObjectPrinterAddMember(p, d.intervals);
	return p.json();
}


QDataStream& operator<<(QDataStream& s, const ErrorAnalysisLinkData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.link;
	s << d.ppData;
	s << d.pathData;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisLinkData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.link;
	s >> d.ppData;
	s >> d.pathData;

	return s;
}

QString toJson(const ErrorAnalysisLinkData &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.link);
	jsonObjectPrinterAddMember(p, d.ppData);
	jsonObjectPrinterAddMember(p, d.pathData);
	return p.json();
}


QDataStream& operator<<(QDataStream& s, const ErrorAnalysisSequenceData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.sequence;
	s << d.ppData;
	s << d.pathData;
	s << d.linkData;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisSequenceData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.sequence;
	s >> d.ppData;
	s >> d.pathData;
	s >> d.linkData;

	return s;
}

QString toJson(const ErrorAnalysisSequenceData &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.sequence);
	jsonObjectPrinterAddMember(p, d.ppData);
	jsonObjectPrinterAddMember(p, d.pathData);
	//jsonObjectPrinterAddMember(p, d.linkData);
	return p.json();
}


QDataStream& operator<<(QDataStream& s, const ErrorAnalysisData& d)
{
	qint32 ver = 1;

	s << ver;

	s << d.tsStart;
	s << d.tsEnd;
	s << d.intervalSize;
	s << d.numIntervals;
	s << d.samplingPeriod;
	s << d.linkNeutrality;
	s << d.seqNeutrality;
	s << d.pathClass;
	s << d.seq2paths;
	s << d.seq2pathPairs;
	s << d.link2paths;
	s << d.path2Endpoints;
	s << d.seqDataV1;
	s << d.seqDataV2;

	return s;
}

QDataStream& operator>>(QDataStream& s, ErrorAnalysisData& d)
{
	qint32 ver;

	s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 1);

	s >> d.tsStart;
	s >> d.tsEnd;
	s >> d.intervalSize;
	s >> d.numIntervals;
	s >> d.samplingPeriod;
	s >> d.linkNeutrality;
	s >> d.seqNeutrality;
	s >> d.pathClass;
	s >> d.seq2paths;
	s >> d.seq2pathPairs;
	s >> d.link2paths;
	s >> d.path2Endpoints;
	s >> d.seqDataV1;
	s >> d.seqDataV2;

	return s;
}

ErrorAnalysisPathIntervalData::ErrorAnalysisPathIntervalData()
{
	used = false;
	goodLocally = true;
	goodEnd2End = true;
}

ErrorAnalysisPathPairIntervalData::ErrorAnalysisPathPairIntervalData()
{
	used = false;
}

ErrorAnalysisData::ErrorAnalysisData()
{
	tsStart = 0;
	intervalSize = 0;
	numIntervals = 0;
	samplingPeriod = 0;
}

qint64 ErrorAnalysisData::time2interval(Timestamp time)
{
	if (time < tsStart || time > tsEnd)
		return -1;
	return (time - tsStart) / intervalSize;
}

QPair<Timestamp, Timestamp> ErrorAnalysisData::interval2time(int interval)
{
	QPair<Timestamp, Timestamp> result;
	result.first = tsStart + interval * intervalSize;
	result.second = tsStart + (interval + 1) * intervalSize - 1;
	return result;
}

qint64 ErrorAnalysisData::time2sample(Timestamp time)
{
	return (time - tsStart) / samplingPeriod;
}

QPair<Timestamp, Timestamp> ErrorAnalysisData::sample2time(qint64 sample)
{
	QPair<Timestamp, Timestamp> result;
	result.first = tsStart + sample * samplingPeriod;
	result.second = tsStart + (sample + 1) * samplingPeriod - 1;
	return result;
}

qint64 ErrorAnalysisData::sample2interval(qint64 sample)
{
	//return sample / (intervalSize / samplingPeriod);
	return sample * samplingPeriod / intervalSize;
}

QPair<qint64, qint64> ErrorAnalysisData::interval2sample(qint64 interval)
{
	QPair<qint64, qint64> result;
	result.first = interval * intervalSize / samplingPeriod;
	result.second = (interval + 1) * intervalSize / samplingPeriod - 1;
	return result;
}

bool ErrorAnalysisData::saveToFile(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << *this;

	return out.status() == QDataStream::Ok;
}

bool ErrorAnalysisData::loadFromFile(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;
	return in.status() == QDataStream::Ok;
}

bool ErrorAnalysisData::saveToJson(QString expPath, QString sMeasurements)
{
	{
		QDir dir;
		dir.cd(expPath);
		dir.mkpath(QString("error-analysis-json-%1").arg(sMeasurements));
	}

	QFile file(expPath + QString("/error-analysis-json-%1/error-analysis-data.json").arg(sMeasurements));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file";
		return false;
	}
	QTextStream out(&file);

	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, tsStart);
	jsonObjectPrinterAddMember(p, tsEnd);
	jsonObjectPrinterAddMember(p, intervalSize);
	jsonObjectPrinterAddMember(p, numIntervals);
	jsonObjectPrinterAddMember(p, samplingPeriod);
	jsonObjectPrinterAddMember(p, linkNeutrality);
	jsonObjectPrinterAddMember(p, seqNeutrality);
	jsonObjectPrinterAddMember(p, pathClass);
	jsonObjectPrinterAddMember(p, seq2paths);
	jsonObjectPrinterAddMember(p, seq2pathPairs);
	jsonObjectPrinterAddMember(p, link2paths);
	jsonObjectPrinterAddMember(p, path2Endpoints);
	out << p.json() << "\n";

	if (out.status() != QTextStream::Ok)
		return false;

	for (int version = 1; version <= 2; version++) {
		foreach (LinkSequence seq, seqNeutrality.uniqueKeys()) {
			ErrorAnalysisSequenceData &seqData = version == 1 ?
													 seqDataV1[seq] :
													 seqDataV2[seq];
			QString seqString("s");
			foreach (Link e, seq) {
				seqString += QString("%1%2").arg(seqString.endsWith("s") ? "" : "-").arg(e);
			}
			seqString += QString("-v%1").arg(version);

			QFile fileSeq(expPath + QString("/error-analysis-json-%1/%2.json").arg(sMeasurements).arg(seqString));
			if (!fileSeq.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				qDebug() << __FILE__ << __LINE__ << "Failed to open file";
				return false;
			}
			QTextStream outSeq(&fileSeq);
			outSeq << toJson(seqData) << "\n";
			if (outSeq.status() != QTextStream::Ok)
				return false;
		}
	}

	return true;
}
