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

#include "run_experiment.h"

#include "run_experiment_params.h"
#include "netgraph.h"
#include "deploy.h"
#include "chronometer.h"
#include "remoteprocessssh.h"
#include "simulate_experiment.h"
#include "tomodata.h"
#include "export_matlab.h"
#include "result_processing.h"
#include "intervalmeasurements.h"
#include "util.h"

bool doGenericSimulation(NetGraph &g, const RunParams &runParams);

static volatile bool mustStop;
static void sigIntMustStop(int )
{
	mustStop = true;
}

bool runExperiment(QString paramsFileName) {
	RunParams runParams;
	if (!loadRunParams(runParams, paramsFileName)) {
		return false;
	}

	qDebug() << QString("Running experiment for graph %1, working dir %2")
				.arg(runParams.graphName)
				.arg(runParams.workingDir);

	QDir dir;
	dir.mkpath(runParams.workingDir);
	dir.cd(runParams.workingDir);

	QFile::copy(paramsFileName, runParams.workingDir + "/run-params.data");
	dumpRunParams(runParams, runParams.workingDir + "/run-params.txt");

	NetGraph g;
	if (!g.loadFromByteArray(qUncompress(runParams.graphSerializedCompressed))) {
		g.setFileName(runParams.graphInputPath);
		if (!g.loadFromFile()) {
			return false;
		}
	}

	if (runParams.realRouting) {
		QList<QString> customIPs = QList<QString>()
								   << "192.168.20.2"
								   << "192.168.40.2"
								   << "192.168.21.2"
								   << "192.168.41.2"
								   << "192.168.22.2"
								   << "192.168.42.2"
								   << "192.168.23.2"
								   << "192.168.43.2";
		int index = 0;
		for (int i = 0; i < g.nodes.count(); i++) {
			if (g.nodes[i].nodeType == NETGRAPH_NODE_HOST) {
				g.nodes[i].customIp = customIPs[index % customIPs.count()];
				g.nodes[i].customIpForeign = g.nodes[i].customIp;
				index++;
			}
		}
	} else {
		for (int i = 0; i < g.nodes.count(); i++) {
			if (g.nodes[i].nodeType == NETGRAPH_NODE_HOST) {
				g.nodes[i].customIp.clear();
				g.nodes[i].customIpForeign.clear();
			}
		}
	}

	g.setFileName(dir.absolutePath() + "/" + runParams.graphName + ".graph");
	if (!g.saveToFile()) {
		return false;
	}

	mustStop = false;
	signal(SIGINT, sigIntMustStop);
	if (!doGenericSimulation(g, runParams)) {
		signal(SIGINT, SIG_DFL);
		return false;
	}

	if (!exportMatlab(runParams.workingDir, runParams.graphName)) {
		return false;
	}

    if (!processResults(QStringList() << runParams.workingDir)) {
		return false;
	}

	signal(SIGINT, SIG_DFL);
	return true;
}

bool doGenericSimulation(NetGraph &g, const RunParams &runParams)
{
	// Experiment duration in seconds
	int timeout = runParams.estimatedDuration / 1000000000ULL;

	qDebug() << "Redeploying...";
	if (!deploy(g, runParams)) {
		qError() << "Deployment failed";
		return false;
	}
	if (mustStop) {
		qError() << "User abort";
		return false;
	}

	if (runParams.fakeEmulation) {
		return runSimulation(g, runParams);
	}

	QList<QSharedPointer<RemoteProcessSsh> > sshHosts;
	if (!runParams.realRouting) {
		sshHosts << QSharedPointer<RemoteProcessSsh>(new RemoteProcessSsh("traffic-generator"));
		if (!sshHosts.last()->connect("root", runParams.clientHostName, runParams.clientPort)) {
			qError() << "Connect to client failed";
			return false;
		}
	} else {
		foreach (QString host, runParams.clientHostNames) {
			sshHosts << QSharedPointer<RemoteProcessSsh>(new RemoteProcessSsh(QString("traffic-generator-%1").arg(host)));
			if (!sshHosts.last()->connect("root", host, runParams.clientPort)) {
				qError() << "Connect to client failed";
				return false;
			}
		}
	}

	QList<QSharedPointer<RemoteProcessSsh> > sshCores;
	if (!runParams.realRouting) {
		sshCores << QSharedPointer<RemoteProcessSsh>(new RemoteProcessSsh("network-emulator"));
		if (!sshCores.last()->connect("root", runParams.coreHostName, runParams.corePort)) {
			qError() << "Connect to core failed";
			return false;
		}
	} else {
		foreach (QString host, runParams.routerHostNames) {
			sshCores << QSharedPointer<RemoteProcessSsh>(new RemoteProcessSsh(QString("network-emulator-%1").arg(host)));
			if (!sshCores.last()->connect("root", host, runParams.corePort)) {
				qError() << "Connect to router failed";
				return false;
			}
		}
	}

	QList<QSharedPointer<RemoteProcessSsh> > sshAll;
	foreach (PointerSsh ssh, sshCores) {
		sshAll << ssh;
	}
	foreach (PointerSsh ssh, sshHosts) {
		sshAll << ssh;
	}

	QHash<RemoteProcessSsh*, QString> emulatorKeys;
	QHash<RemoteProcessSsh*, QString> multiplexerKeys;
	QHash<RemoteProcessSsh*, QString> recorderKeys;
	QHash<RemoteProcessSsh*, QString> allKeys;

	// Check for iptables
	{
		foreach (PointerSsh ssh, sshAll) {
			ssh->runCommand("limit coredumpsize unlimited");
			ssh->runCommand("limit coredumpsize unlimited");

			QString checkKey = ssh->startProcess("sh", QStringList() << "-c" << "lsmod | grep ip_tables");
			while (!mustStop && ssh->isProcessRunning(checkKey)) {}
			QString output = ssh->readAllStdout(checkKey) + "\n" + ssh->readAllStderr(checkKey);
			if (output.contains("ip_tables")) {
				qError() << QString("Module ip_tables is loaded on the system %1. "
									"This can interfere with the emulation. "
									"You should either rmmod it or run 'service iptables stop'.")
							.arg(ssh->getHostname());
			}
		}
	}

	// set the number of maximum open files/sockets
	{
		int maxOpenFiles = 1000000;
		foreach (PointerSsh ssh, sshAll) {
			ssh->runRawCommand(QString("ulimit -n %1").arg(maxOpenFiles));
			ssh->runCommand(QString("sysctl -w fs.file-max=%1").arg(maxOpenFiles));
		}
	}

	// clear leftover apps
	qDebug() << QString("Clearing zombie apps...");
	foreach (PointerSsh ssh, sshHosts) {
        QString clearKey = ssh->startProcess("killall -9 line-traffic; killall -9 bash");
		while (!mustStop && ssh->isProcessRunning(clearKey)) {}
		QString output = ssh->readAllStdout(clearKey) + "\n" + ssh->readAllStderr(clearKey);
		qDebug() << output;
	}

	foreach (PointerSsh ssh, sshHosts) {
		while (!mustStop) {
			qDebug() << "Checking for open connections...";
			QString netstatKey = ssh->startProcess("netstat", QStringList() << "-ntp");
			while (!mustStop && ssh->isProcessRunning(netstatKey)) {}
			QString output = ssh->readAllStdout(netstatKey) + "\n" + ssh->readAllStderr(netstatKey);
			if (!output.contains(" 10.128."))
				break;
			qDebug() << QString("Found %1 leftover connections. Waiting...").arg(output.count(" 10.128."));
			sleep(5);
		}
	}

	// clear leftover emulator
#ifndef DEGUB_EMULATOR
	foreach (PointerSsh ssh, sshCores) {
		qDebug() << QString("Clearing zombie emulator...");
		QString clearKey = ssh->startProcess("killall -9 line-router; killall -9 bash");
		while (!mustStop && ssh->isProcessRunning(clearKey)) {}
		QString output = ssh->readAllStdout(clearKey) + "\n" + ssh->readAllStderr(clearKey);
		qDebug() << output;
	}
#endif

	QString testId = runParams.workingDir.split('/', QString::SkipEmptyParts).last();

	if (mustStop) {
		qError() << "User abort";
		return false;
	}

	if (!mustStop) {
		foreach (PointerSsh ssh, sshAll) {
			QString key = ssh->startProcess(QString("mkdir -p %1").arg(testId));
			if (!ssh->waitForFinished(key, -1))
				qError() << QString("Error: could create remote dir");
		}
	}

	if (!mustStop) {
		// Start the emulator
		foreach (PointerSsh ssh, sshCores) {
			QString emulatorCmd;
			if (!runParams.realRouting) {
                emulatorCmd = QString("LD_PRELOAD=/usr/lib/malloc_profile.so line-router %1.graph %2 %3 %4 %5 %6 %7 %8")
							  .arg(runParams.graphName)
							  .arg(testId)
							  .arg(runParams.capture ?
									   QString("--record %1 %2")
									   .arg(runParams.capturePacketLimit)
									   .arg(runParams.captureEventLimit) :
									   QString(""))
							  .arg(QString("--interval_size %1 --estimated_duration %2")
								   .arg(runParams.intervalSize)
								   .arg(runParams.estimatedDuration))
							  .arg(QString("--scale_buffers %1")
								   .arg(runParams.bufferBloatFactor))
							  .arg(QString("--qos_scale_buffers %1")
								   .arg(runParams.qosBufferScaling))
							  .arg(QString("--queuing_discipline %1")
                                   .arg(runParams.queuingDiscipline))
                              .arg(runParams.flowTracking ?
                                       QString("--track_flows") :
                                       QString(""));
			} else {
				emulatorCmd = QString("bash -c 'echo real; while true ; do sleep 5; done'");
			}
			qDebug() << QString("Emulator command: %1").arg(emulatorCmd);
			allKeys[ssh.data()] = emulatorKeys[ssh.data()] = ssh->startProcess(emulatorCmd);
		}
	}

	if (!mustStop) {
		// Start the recorders
		foreach (PointerSsh ssh, sshAll) {
			QString recorderCmd = QString("bash -c '"
										  "cat /proc/net/line-reset; "
										  "x=0; "
										  "ttl=64; "
										  "while true; "
										  "do "
										  "  (( x-- )) || { "
										  "    x=4; "
										  "	   echo \"$ttl\" > /proc/sys/net/ipv4/ip_default_ttl; "
										  "    (( ttl++ )); "
										  "	   (( ttl > 255 )) && ttl=64; "
										  "	 }; "
										  "  date +%s; "
										  "  cat /proc/sys/net/ipv4/ip_default_ttl; "
										  "  sleep 1; "
										  "done"
										  "'");
			qDebug() << QString("Recorder command: %1").arg(recorderCmd);
			allKeys[ssh.data()] = recorderKeys[ssh.data()] = ssh->startProcess(recorderCmd);
		}
	}

	// Wait
	qDebug() << "Sleeping a bit so that the recorders can be calibrated...";
	sleep(5);

	if (!mustStop) {
		// Start the clients
		foreach (PointerSsh ssh, sshHosts) {
            QString multiplexerCmd = QString("line-traffic --master %1.graph").
									 arg(runParams.graphName);
			qDebug() << QString("Multiplexer command: %1").arg(multiplexerCmd);
			allKeys[ssh.data()] = multiplexerKeys[ssh.data()] = ssh->startProcess(multiplexerCmd);
		}
	}

	if (timeout) {
		qDebug() << QString("Letting it run for %1...").arg(Chronometer::durationToString(1000ULL * (timeout), "s"));
		qreal tStart = getCurrentTimeSec();
		int i = 0;
		bool allGood = true;
		while (allGood && getCurrentTimeSec() - tStart < timeout) {
			sleep(1);
			if (mustStop)
				break;
			if ((i >= 5) && ((timeout - i) % 5 == 0)) {
				qDebug() << QString("ETA: %1...").arg(Chronometer::durationToString(1000ULL * (timeout - i), "s"));
			}
			if (i >= 10 && i % 10 == 0) {
				foreach (PointerSsh ssh, sshAll) {
					if (!ssh->isProcessRunning(allKeys[ssh.data()])) {
						qError() << "Early abort: machine down";
						allGood = false;
						break;
					}
				}
			}
			i++;
		}
	} else {
		// wait for clients
		qDebug() << QString("Waiting for client apps to finish...");
		foreach (PointerSsh ssh, sshHosts) {
			while (1) {
				for (int i = 0; i < 5; i++) {
					sleep(1);
					if (mustStop)
						break;
				}
				if (mustStop)
					break;
				if (ssh->waitForFinished(multiplexerKeys[ssh.data()], 1))
					break;
			}
		}
	}

	// stopping the apps should be as simple as this:
	foreach (PointerSsh ssh, sshHosts) {
		if (!ssh->signalProcess(multiplexerKeys[ssh.data()], SIGINT)) {
			qError() << QString("Error: %1 signal failed").arg(multiplexerKeys[ssh.data()]);
		}
	}

	// Stop the recorders
	foreach (PointerSsh ssh, sshAll) {
		if (!ssh->signalProcess(recorderKeys[ssh.data()], SIGUSR1)) {
			qError() << QString("Error: %1 signal failed").arg(recorderKeys[ssh.data()]);
		}
	}

	// stop emulator
	bool emulatorExitOk = false;
	QString emulatorExitCode;
	foreach (PointerSsh ssh, sshCores) {
		QString emulatorKey = emulatorKeys[ssh.data()];
		qDebug() << QString("Stopping emulator (%1)").arg(emulatorKey);
		if (!ssh->signalProcess(emulatorKey, !runParams.realRouting ? SIGINT : SIGUSR1)) {
			qError() << QString("Error: %1 signal failed").arg(emulatorKey);
		}
		if (!ssh->waitForFinished(emulatorKey, -1)) {
			qError() << QString("Error: %1 wait failed").arg(emulatorKey);
			emulatorExitOk = false;
		} else {
			emulatorExitCode = ssh->getExitCodeStr(emulatorKey);
			qDebug() << QString("Emulator exited with code %1").arg(emulatorExitCode);
			emulatorExitOk = emulatorExitCode == "Success" || emulatorExitCode == "SIGUSR1";
		}
		qDebug() << QString("Emulator output:");
		qDebug() << ssh->readAllStdout(emulatorKey);
		qDebug() << ssh->readAllStderr(emulatorKey);
	}

	// we need to kill client apps if (1) we are running with a timeout or (2) if the stop button was pressed
	// we need to kill server apps in any case
	// but there is no big problem if we try to kill them all anyways so...
	bool multiplexerExitOk = false;
	QString multiplexerExitCode;
	foreach (PointerSsh ssh, sshHosts) {
		QString multiplexerKey = multiplexerKeys[ssh.data()];
		if (!ssh->waitForFinished(multiplexerKey, 5)) {
			qError() << QString("Error: %1 wait failed after SIGINT; trying SIGKILL").arg(multiplexerKey);
		} else {
			qDebug() << QString("Multiplexer exited with code %1").arg(ssh->getExitCodeStr(multiplexerKey));
		}

		if (!ssh->signalProcess(multiplexerKey, SIGKILL)) {
			qError() << QString("Error: %1 signal failed").arg(multiplexerKey);
		}

		if (!ssh->waitForFinished(multiplexerKey, 5)) {
			multiplexerExitOk = false;
			qError() << QString("Error: %1 wait failed").arg(multiplexerKey);
		} else {
			multiplexerExitCode = ssh->getExitCodeStr(multiplexerKey);
			qDebug() << QString("Multiplexer exited with code %1").arg(multiplexerExitCode);
			multiplexerExitOk = multiplexerExitCode == "Success";
		}
	}

	// show output from apps
	qDebug() << QString("App output");
	{
		QDir dir(".");
		dir.mkpath(runParams.workingDir);
	}
	foreach (PointerSsh ssh, sshHosts) {
		QString fileNamePrefix = QString("%1/").arg(runParams.workingDir);
		QString fileName = fileNamePrefix + QString("multiplexer");
		QString result;
		qDebug() << QString("STDOUT multiplexer %1:").arg(multiplexerKeys[ssh.data()]);
		result = ssh->readAllStdout(multiplexerKeys[ssh.data()]);
		qDebug() << result;
		saveFile(fileName + ".out", result);
		qDebug() << QString("STDERR multiplexer %1:").arg(multiplexerKeys[ssh.data()]);
		result = ssh->readAllStderr(multiplexerKeys[ssh.data()]);
		qDebug() << result;
		saveFile(fileName + ".err", result);
	}

	// Save emulator output
	foreach (PointerSsh ssh, sshCores) {
		QString emulatorKey = emulatorKeys[ssh.data()];
		QString suffix = runParams.realRouting ? QString("-%1").arg(ssh->getHostname()) : QString("");
		QString key = ssh->startProcess("cp", QStringList() <<
										QString("%1.out").arg(emulatorKey) <<
										QString("%1/emulator%2.out").arg(testId).arg(suffix));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not copy emulator stdout");
		key = ssh->startProcess("cp", QStringList() <<
								QString("%1.err").arg(emulatorKey) <<
								QString("%1/emulator%2.err").arg(testId).arg(suffix));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not copy emulator stderr");

		// Zip everything
		key = ssh->startProcess("zip", QStringList() <<
								QString("-r") <<
								QString("%1.zip").arg(testId) <<
								QString("%1").arg(testId));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not zip emulator output folder");

		// Download
		if (!ssh->downloadFromRemote(QString("%1.zip").arg(testId), parentDirFromDir(runParams.workingDir))) {
			qError() << "Aborted.";
		}

		// Unzip
		QString command = "sh";
		QStringList args = QStringList() << "-c" << QString("cd %1 ; unzip -o %2.zip")
											.arg(parentDirFromDir(runParams.workingDir))
											.arg(testId);
		if (QProcess::execute(command, args) != 0) {
			qError() << "Aborted.";
		} else {
			TomoData tomoData;
			tomoData.load(runParams.workingDir + "/" + "tomo-records.dat");
			dumpTomoData(tomoData, runParams.workingDir + "/" + "tomo-records.txt");
		}
	}

	// Save recorder output
	qDebug() << "Saving recorder output";
	foreach (PointerSsh ssh, sshAll) {
		QString recorderKey = recorderKeys[ssh.data()];
		QString suffix = ssh->getHostname();
		QString key = ssh->startProcess("cp", QStringList() <<
										QString("%1.out").arg(recorderKey) <<
										QString("%1/recorder-%2.out").arg(testId).arg(suffix));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not copy recorder stdout");
		key = ssh->startProcess("cp", QStringList() <<
								QString("%1.err").arg(recorderKey) <<
								QString("%1/recorder-%2.err").arg(testId).arg(suffix));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not copy recorder stderr");

		// Zip everything
		key = ssh->startProcess("zip", QStringList() <<
								QString("-r") <<
								QString("%1.zip").arg(testId) <<
								QString("%1").arg(testId));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not zip recorder output folder");

		// Download
		if (!ssh->downloadFromRemote(QString("%1.zip").arg(testId), parentDirFromDir(runParams.workingDir))) {
			qError() << "Aborted.";
		}

		// Unzip
		QString command = "sh";
		QStringList args = QStringList() << "-c" << QString("cd %1 ; unzip -o %2.zip")
											.arg(parentDirFromDir(runParams.workingDir))
											.arg(testId);
		if (QProcess::execute(command, args) != 0) {
			qError() << "Aborted.";
		}
	}

	// Save line-rec data
	foreach (PointerSsh ssh, sshAll) {
		QString suffix = ssh->getHostname();

		QString key = ssh->startProcess(QString("cat /proc/net/line-in > %1/line-in-%2").arg(testId).arg(suffix));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not copy line-in");

		key = ssh->startProcess(QString("cat /proc/net/line-out > %1/line-out-%2").arg(testId).arg(suffix));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not copy line-out");

		// Download
		if (!ssh->downloadFromRemote(QString("%1/line-in-%2").arg(testId).arg(suffix), runParams.workingDir)) {
			qError() << "Aborted.";
		}
		if (!ssh->downloadFromRemote(QString("%1/line-out-%2").arg(testId).arg(suffix), runParams.workingDir)) {
			qError() << "Aborted.";
		}
	}

	// Cleanup
	foreach (PointerSsh ssh, sshAll) {
		QString key = ssh->startProcess("rm", QStringList() << QString("-rf") <<
										QString("%1").arg(testId) <<
										QString("%1.zip").arg(testId));
		if (!ssh->waitForFinished(key, -1))
			qError() << QString("Error: could not delete remote files");
	}

	saveFile(runParams.workingDir + "/graph.txt",  g.toText());

	if (runParams.realRouting) {
		saveFile(runParams.workingDir + "/simulation.txt",
				 QString("graph=%1")
				 .arg(QString(g.fileName).replace(".graph", "").split('/', QString::SkipEmptyParts).last()));
	}

	// Export tomodata
	{
		TomoData tomoData;
		tomoData.load(runParams.workingDir + "/" + "tomo-records.dat");
		dumpTomoData(tomoData, runParams.workingDir + "/" + "tomo-records.txt");
	}

	if (runParams.realRouting) {
		lineKernelRec2IntervalMeasurements(runParams.workingDir,
										   runParams.clientHostNames);
		saveFile(runParams.workingDir + "/emulator.out", QString("real"));
		saveFile(runParams.workingDir + "/emulator.err", QString("real"));
	}

	qDebug() << QString("Test done.");

	if (emulatorExitOk) {
		qDebug() << QString("Emulator exit code: %1").arg(emulatorExitCode);
	} else {
		qError() << QString("Emulator exit code: %1").arg(emulatorExitCode);
	}
	if (multiplexerExitOk) {
		qDebug() << QString("Multiplexer exit code: %1").arg(multiplexerExitCode);
	} else {
		qError() << QString("Multiplexer exit code: %1").arg(multiplexerExitCode);
	}

	foreach (PointerSsh ssh, sshAll) {
		ssh->disconnect(emulatorExitOk && multiplexerExitOk);
	}

	return true;
}

bool readMachineData(QString fileName, QList<QList<qint64> > &data1s) {
	data1s.clear();
	QString text;
	if (!readFile(fileName, text)) {
		qErr() << "Could not read file" << fileName;
		return false;
	}
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	foreach (QString line, lines) {
		QStringList tokens = line.split(' ', QString::SkipEmptyParts);
		if (tokens.isEmpty())
			continue;
		QList<qint64> row;
		foreach (QString token, tokens) {
			bool ok;
			row << token.toLongLong(&ok);
			if (!ok) {
				qErr() << "Bad integer in" << fileName << "token" << token << "line" << line;
				return false;
			}
		}
		data1s << row;
	}
	return true;
}

bool lineKernelRec2IntervalMeasurements(QString testId,
										QList<QString> hosts) {
	// Network: A <-> B <-> C <-> D
	// A hosts nodes (id from 0) 2, 4, 6, 8.
	// D hosts nodes (id from 0) 3, 5, 7, 9.
	QList<QList<qint64> > dataAOut;
	if (!readMachineData(QString("%1/line-out-%2").arg(testId).arg(hosts[0]), dataAOut)) {
		qError() << "readMachineData";
		return false;
	}

	QList<QList<qint64> > dataAIn;
	if (!readMachineData(QString("%1/line-in-%2").arg(testId).arg(hosts[0]), dataAIn)) {
		qError() << "readMachineData";
		return false;
	}

	QList<QList<qint64> > dataDIn;
	if (!readMachineData(QString("%1/line-in-%2").arg(testId).arg(hosts[1]), dataDIn)) {
		qError() << "readMachineData";
		return false;
	}

	QList<QList<qint64> > dataDOut;
	if (!readMachineData(QString("%1/line-out-%2").arg(testId).arg(hosts[1]), dataDOut)) {
		qError() << "readMachineData";
		return false;
	}

	if (dataAOut.length() != dataDIn.length() ||
		dataAIn.length() != dataDOut.length() ||
		dataAIn.length() != dataAOut.length()) {
		qError() << "data length mismatch";
		return false;
	}

	int iStart = 0;
	for (int i = 0; i < dataAOut.length(); i++) {
		if (dataAIn[i].count(0) != dataAIn[i].count())
			break;
		if (dataAOut[i].count(0) != dataAOut[i].count())
			break;
		if (dataDIn[i].count(0) != dataDIn[i].count())
			break;
		if (dataDOut[i].count(0) != dataDOut[i].count())
			break;
		iStart++;
	}

	int iLast = dataAOut.length() - 1;
	for (int i = dataAOut.length() - 1; i >= 0; i--) {
		if (dataAIn[i].count(0) != dataAIn[i].count())
			break;
		if (dataAOut[i].count(0) != dataAOut[i].count())
			break;
		if (dataDIn[i].count(0) != dataDIn[i].count())
			break;
		if (dataDOut[i].count(0) != dataDOut[i].count())
			break;
		iLast--;
	}

	if (iStart > iLast) {
		qError() << "empty dataset";
		return false;
	}

	const int period = 5;
	ExperimentIntervalMeasurements intervalMeasurements;
	intervalMeasurements.initialize(777,
									quint64(iLast - iStart + 1) * 1000ULL * 1000ULL * 1000ULL,
									quint64(period) * 1000ULL * 1000ULL * 1000ULL,
									18,
									8,
                                    QList<QInt32Pair>(),
                                    1400);

	const int edge = 0;
	for (int i = iStart; i <= iLast; i++) {
		quint64 time = intervalMeasurements.tsStart + quint64((i - iStart) * period) * 1000ULL * 1000ULL * 1000ULL + period / 2;
        const int packetSize = 1500;
		for (int p = 0; p < dataAOut[i].count(); p++) {
			if (dataAOut[i][p] >= dataDIn[i][p]) {
                intervalMeasurements.countPacketInFLightEdge(edge, p, time, time, packetSize, dataAOut[i][p]);
                intervalMeasurements.countPacketInFLightPath(p, time, time, packetSize, dataAOut[i][p]);
                intervalMeasurements.countPacketDropped(edge, p, time, time, packetSize, dataAOut[i][p] - dataDIn[i][p]);
			} else {
                intervalMeasurements.countPacketInFLightEdge(edge, p, time, time, packetSize, dataDOut[i][p]);
                intervalMeasurements.countPacketInFLightPath(p, time, time, packetSize, dataDOut[i][p]);
                intervalMeasurements.countPacketDropped(edge, p, time, time, packetSize, dataDOut[i][p] - dataAIn[i][p]);
			}
		}
	}
	if (!intervalMeasurements.save(testId + "/interval-measurements.data")) {
		qError() << "save";
	}

	return true;
}
