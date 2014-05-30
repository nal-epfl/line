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
	}

	// Cleanup
	foreach (QString fileName, processedParams) {
	  QFile::remove(fileName);
	}
	processedParams.clear();

	return 0;
}
