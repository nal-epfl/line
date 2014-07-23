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

#include <QtCore>
#include <QDebug>

#include "deploy.h"
#include "export_matlab.h"
#include "run_experiment.h"
#include "result_processing.h"
#include "tinyhistogram.h"
#include "util.h"

QString shiftCmdLineArg(int &argc, char **&argv, QString preceding = QString()) {
	if (argc <= 0) {
		qDebug() << "Missing parameter" <<
					(preceding.isEmpty() ? QString() : QString("after %1").arg(preceding));
		exit(-1);
	}
	QString arg = argv[0];
	argc--, argv++;
	return arg;
}

QString peekCmdLineArg(int argc, char **argv) {
	if (argc <= 0) {
		return QString();
	}
	return QString(argv[0]);
}

void flushLogs() {
	OutputStream::flushCopies();
}

void setupLogging(QString logDir) {
	QDir(".").mkpath(logDir);
	QFile *file = new QFile(logDir + "/" + "line-runner.log");
	file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
	OutputStream::addCopy(QSharedPointer<QTextStream>(new QTextStream(file)));
	atexit(flushLogs);
}

void checkTrace(QString recordedFileName, QStringList pcapFileNames)
{
	TrafficTraceRecord traceRecord;
	if (traceRecord.load(recordedFileName)) {
		if (0) {
			TrafficTraceRecord traceRecordRaw;
			if (traceRecordRaw.rawLoad(recordedFileName.toLatin1().constData())) {
				if (traceRecordRaw.tsStart != traceRecord.tsStart) {
					qDebug() << "different data!!!";
					Q_ASSERT_FORCE(false);
				}
				if (traceRecordRaw.events.count() != traceRecord.events.count()) {
					qDebug() << "different data!!!";
					Q_ASSERT_FORCE(false);
				}
				for (int iEvent = 0; iEvent < traceRecord.events.count(); iEvent++) {
					if (traceRecordRaw.events[iEvent].traceIndex != traceRecord.events[iEvent].traceIndex ||
						traceRecordRaw.events[iEvent].packetIndex != traceRecord.events[iEvent].packetIndex ||
						traceRecordRaw.events[iEvent].injectionTime != traceRecord.events[iEvent].injectionTime ||
						traceRecordRaw.events[iEvent].exitTime != traceRecord.events[iEvent].exitTime ||
						traceRecordRaw.events[iEvent].theoreticalDelay != traceRecord.events[iEvent].theoreticalDelay) {
						qDebug() << "different data!!!";
						Q_ASSERT_FORCE(false);
					}
				}
			} else {
				qDebug() << "raw load error!!!";
				Q_ASSERT_FORCE(false);
			}
		}
		QVector<TrafficTrace> traces;
		foreach (QString pcapFileName, pcapFileNames) {
			TrafficTrace trace;
			trace.setPcapFilePath(pcapFileName);
			trace.loadFromPcap();
			traces.append(trace);
		}

		qint64 splitInterval = 60ULL * 1000ULL * 1000ULL * 1000ULL;
		QVector<TinyHistogram> injectionDelays;
		QVector<TinyHistogram> theoreticalDelays;
		QVector<TinyHistogram> processingDelays;
		QVector<TinyHistogram> relativeDelayErrors;
		QVector<qint32> packets;
		QVector<qint32> drops;

		qint64 tsStart = (qint64)traceRecord.tsStart;
		// debug
		if (1) {
			qDebug() << "Simulation starts at" << tsStart;
		}
		for (int iEvent = 0; iEvent < traceRecord.events.count(); iEvent++) {
			qint32 iTrace = traceRecord.events[iEvent].traceIndex;
			qint32 iPacket = traceRecord.events[iEvent].packetIndex;
			if (0 <= iTrace && iTrace < traces.count() &&
				0 <= iPacket && iPacket < traces[iTrace].packets.count()) {
				// Origin: 0
				qint64 idealInjectionTime = (qint64)traces[iTrace].packets[iPacket].timestamp;
				int iSplit = idealInjectionTime / splitInterval;
				while (iSplit >= injectionDelays.count()) {
					injectionDelays << TinyHistogram(32);
					theoreticalDelays << TinyHistogram(32);
					processingDelays << TinyHistogram(32);
					relativeDelayErrors << TinyHistogram(32);
					packets << 0;
					drops << 0;
				}

				qint64 realInjectionTime = (qint64)traceRecord.events[iEvent].injectionTime - tsStart;

				qint64 delta = realInjectionTime - idealInjectionTime;

				if (delta < 0) {
					qDebug() << __FILE__ << __LINE__ << "negative delay";
					Q_ASSERT_FORCE(false);
				}

				injectionDelays[iSplit].recordEvent(delta);
				packets[iSplit]++;
				if (traceRecord.events[iEvent].exitTime == 0) {
					drops[iSplit]++;
				} else {
					qint64 absoluteDelayError = traceRecord.events[iEvent].exitTime - traceRecord.events[iEvent].injectionTime
												- traceRecord.events[iEvent].theoreticalDelay;
					processingDelays[iSplit].recordEvent(absoluteDelayError);
					qint64 relativeDelayError = (absoluteDelayError * 100) / qMax(1ULL, traceRecord.events[iEvent].theoreticalDelay);
					relativeDelayErrors[iSplit].recordEvent(relativeDelayError);
					theoreticalDelays[iSplit].recordEvent(traceRecord.events[iEvent].theoreticalDelay);
				}

				// debug
				if (1) {
					if (iPacket < 100) {
						qDebug() << "Packet in trace" << iTrace << "at index" << iPacket;
						qDebug() << "Ideal injection time (relative, from .pcap):" << idealInjectionTime;
						qDebug() << "Real injection time (relative, from .data):" << realInjectionTime;
						qDebug() << "Delay:" << delta;
						qDebug() << "Real injection time (absolute, from .data):" << traceRecord.events[iEvent].injectionTime;
						qDebug() << "Real exit time (absolute, from .data):" << traceRecord.events[iEvent].exitTime;
						qDebug() << "Theoretical delay (from .data):" << traceRecord.events[iEvent].theoreticalDelay;
						qDebug() << "";
					}
				}
			} else {
				qDebug() << __FILE__ << __LINE__ << "bad index";
				Q_ASSERT_FORCE(false);
			}
		}

		qDebug() << __FILE__ << __LINE__ << "===============================================";
		for (int iSplit = 0; iSplit < injectionDelays.count(); iSplit++) {
			qDebug() << __FILE__ << __LINE__ << "-----------------------------------------------";
			qDebug() << __FILE__ << __LINE__ << "Interval:" << time2String(iSplit * splitInterval) << "to" << time2String((iSplit + 1) * splitInterval);
			qDebug() << __FILE__ << __LINE__ << "Packets:" << intWithCommas2String(packets[iSplit]);
			qDebug() << __FILE__ << __LINE__ << "Drops:" << intWithCommas2String(drops[iSplit]);
			qDebug() << __FILE__ << __LINE__ << "Injection delays:" << injectionDelays[iSplit].toString(time2String);
			qDebug() << __FILE__ << __LINE__ << "Theoretical delays:" << theoreticalDelays[iSplit].toString(time2String);
			qDebug() << __FILE__ << __LINE__ << "Processing delays:" << processingDelays[iSplit].toString(time2String);
			qDebug() << __FILE__ << __LINE__ << "Relative delay errors (%):" << relativeDelayErrors[iSplit].toString(intWithCommas2String);
		}
		qDebug() << __FILE__ << __LINE__ << "===============================================";
	} else {
		qDebug() << "Could not load injection.data";
	}
}

int main(int argc, char **argv) {
    unsigned int seed = clock() ^ time(NULL) ^ getpid();
	qDebug() << "Seed:" << seed;
	srand(seed);

	shiftCmdLineArg(argc, argv);

	QString command;
	QString paramsFileName;
	QStringList extraParams;
	QStringList processedParams;

	while (argc > 0) {
		while (argc > 0) {
			QString arg = shiftCmdLineArg(argc, argv);

			if (arg == "--deploy") {
				command = arg;
				paramsFileName = shiftCmdLineArg(argc, argv, command);
			} else if (arg == "--run") {
				command = arg;
				paramsFileName = shiftCmdLineArg(argc, argv, command);
			} else if (arg == "--path-pairs") {
				command = arg;
				paramsFileName = shiftCmdLineArg(argc, argv, command);
			} else if (arg == "--export-matlab") {
				command = arg;
				paramsFileName = shiftCmdLineArg(argc, argv, command);
            } else if (arg == "--process-results") {
				command = arg;
				extraParams.clear();
				while (true) {
					QString param = peekCmdLineArg(argc, argv);
					if (param.isEmpty() || param.startsWith("--"))
						break;
					param = shiftCmdLineArg(argc, argv);
					extraParams << param;
				}
			} else if (arg == "--path-pairs-coverage-master") {
				command = arg;
			} else if (arg == "--path-pairs-coverage") {
				command = arg;
				paramsFileName = shiftCmdLineArg(argc, argv, command);
			} else if (arg == "--check-trace") {
				command = arg;
				extraParams.clear();
				while (true) {
					QString param = peekCmdLineArg(argc, argv);
					if (param.isEmpty() || param.startsWith("--"))
						break;
					param = shiftCmdLineArg(argc, argv);
					extraParams << param;
				}
			} else if (arg == "--log-dir") {
				QString logDir = shiftCmdLineArg(argc, argv, arg);
				setupLogging(logDir);
			} else if (arg == ";") {
				break;
			} else {
				qDebug() << "Unrecognized command line argument:" << arg;
				qDebug() << "Unrecognized command line argument:" << arg;
				qDebug() << "Unrecognized command line argument:" << arg;
				qDebug() << "Unrecognized command line argument:" << arg;
				exit(-1);
			}
		}

		if (command == "--deploy") {
			if (!deploy(paramsFileName)) {
				exit(-1);
			}
			processedParams << paramsFileName;
		}

		if (command == "--export-matlab") {
			if (!exportMatlab(paramsFileName)) {
				exit(-1);
			}
			processedParams << paramsFileName;
		}

        if (command == "--process-results") {
            if (!processResults(extraParams)) {
				exit(-1);
			}
		}

		if (command == "--run") {
			if (!runExperiment(paramsFileName)) {
				exit(-1);
			}
			processedParams << paramsFileName;
		}

		if (command == "--check-trace") {
			QString recordedFileName = extraParams.takeFirst();
			checkTrace(recordedFileName, extraParams);
		}
	}

	// Cleanup
	foreach (QString fileName, processedParams) {
	  QFile::remove(fileName);
	}
	processedParams.clear();

	return 0;
}
