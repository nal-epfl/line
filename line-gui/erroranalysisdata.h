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

#ifndef ERRORANALYSISDATA_H
#define ERRORANALYSISDATA_H

#include <QtCore>

#include "graph_types.h"

class ErrorAnalysisPathIntervalData {
public:
	ErrorAnalysisPathIntervalData();

	bool used;
	// Congestion detection results
	// Locally = per link or per link sequence
	bool goodLocally;
	// End2End = per path
	bool goodEnd2End;
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathIntervalData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathIntervalData& d);
QString toJson(const ErrorAnalysisPathIntervalData &d);

class ErrorAnalysisPathPairIntervalData {
public:
	ErrorAnalysisPathPairIntervalData();

	bool used;

	ErrorAnalysisPathIntervalData p1Data;
	ErrorAnalysisPathIntervalData p2Data;
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathPairIntervalData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathPairIntervalData& d);
QString toJson(const ErrorAnalysisPathPairIntervalData &d);

class ErrorAnalysisPathPairData {
public:
	PathPair pathPair;
	// Same index as ExperimentIntervalMeasurements
	QVector<ErrorAnalysisPathPairIntervalData> intervals;
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathPairData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathPairData& d);
QString toJson(const ErrorAnalysisPathPairData &d);

class ErrorAnalysisPathData {
public:
	Path path;
	// Same index as ExperimentIntervalMeasurements
	QVector<ErrorAnalysisPathIntervalData> intervals;
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisPathPairData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisPathPairData& d);
QString toJson(const ErrorAnalysisPathData &d);

class ErrorAnalysisLinkData {
public:
	Link link;
	QHash<PathPair, ErrorAnalysisPathPairData> ppData;
	QHash<Path, ErrorAnalysisPathData> pathData;
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisLinkData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisLinkData& d);
QString toJson(const ErrorAnalysisLinkData &d);

class ErrorAnalysisSequenceData {
public:
	// Each element is a link index, starting from zero.
	LinkSequence sequence;
	QHash<PathPair, ErrorAnalysisPathPairData> ppData;
	QHash<Path, ErrorAnalysisPathData> pathData;
	// TODO move this to parent
	QHash<Link, ErrorAnalysisLinkData> linkData;
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisSequenceData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisSequenceData& d);
QString toJson(const ErrorAnalysisSequenceData &d);

class ErrorAnalysisData {
public:
	ErrorAnalysisData();

	// Timestamp in nanoseconds
	Timestamp tsStart;
	// Timestamp in nanoseconds
	Timestamp tsEnd;
	// Interval size in nanoseconds
	Timestamp intervalSize;
	// Number of intervals
	qint64 numIntervals;
	// Sampling period in nanoseconds. Note: this is not filled in, nor used in result processing.
	// It is only set and used in result post-processing.
	Timestamp samplingPeriod;

	qint64 time2interval(Timestamp time);
	QPair<Timestamp, Timestamp> interval2time(int interval);

	qint64 time2sample(Timestamp time);
	QPair<Timestamp, Timestamp> sample2time(qint64 sample);

	qint64 sample2interval(qint64 sample);
	QPair<qint64, qint64> interval2sample(qint64 interval);

	QHash<Link, bool> linkNeutrality;
	QHash<LinkSequence, bool> seqNeutrality;
	QHash<Path, Class> pathClass;
	QHash<LinkSequence, QSet<Path> > seq2paths;
	QHash<LinkSequence, QSet<PathPair> > seq2pathPairs;
	QHash<Link, QSet<Path> > link2paths;
	QHash<Path, NodePair> path2Endpoints;

	// Computes the probability of congestion of the sequence discarding intervals with small PPI ONLY for p1, p2.
	// NO resampling.
	QHash<LinkSequence, ErrorAnalysisSequenceData> seqDataV1;
	// Computes the probability of congestion of the sequence discarding intervals with small PPI for ANY path
	// in the sequence. NO resampling.
	QHash<LinkSequence, ErrorAnalysisSequenceData> seqDataV2;

	bool saveToFile(QString fileName);
	bool loadFromFile(QString fileName);
	bool saveToJson(QString expPath, QString sMeasurements);
};
QDataStream& operator<<(QDataStream& s, const ErrorAnalysisData& d);
QDataStream& operator>>(QDataStream& s, ErrorAnalysisData& d);


#endif // ERRORANALYSISDATA_H
