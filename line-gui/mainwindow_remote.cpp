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

#include "mainwindow.h"
#include "ui_mainwindow.h"

bool MainWindow::runCommand(QPlainTextEdit *log, QString command, QStringList args, QString description)
{
	QString output;
	return runCommand(log, command, args, description, true, output);
}

bool MainWindow::runCommand(QPlainTextEdit *log, QString command, QStringList args, QString description, bool showOutput, QString &output)
{
	int exitCode;
	bool runOk;

	if (!description.isEmpty()) {
		emit logInformation(log, QString("Running: %1...").arg(description));
	}

	emit logText(log, command + " " + args.join(" "));
	runOk = runProcess(command, args, output, -1, false, &exitCode);
	if (output.endsWith('\n'))
		output = output.mid(0, output.length() - 1);
	if (showOutput && !output.isEmpty()) {
		emit logOutput(log, output);
	}
	if (!runOk || exitCode) {
		emit logError(log, "FAIL.");
		return false;
	}

	emit logSuccess(log, "OK.");
	return true;
}

bool MainWindow::startProcess(QPlainTextEdit *log, QProcess &process, QString command, QStringList args, QString description,
						QString &output)
{
	Q_UNUSED(output);
	emit logInformation(log, QString("Starting %1...").arg(description));
	emit logText(log, QString("%1 %2").arg(command, args.join(" ")));

	process.setProcessChannelMode(QProcess::MergedChannels);
	process.start(command, args);
	if (!process.waitForStarted(5 * 1000)) {
		emit logError(log, QString("Could not start %1").arg(description));
		return false;
	}
	/*sleep(1);
	if (process.state() == QProcess::NotRunning) {
		output = process.readAllStandardOutput();
		emit logOutput(log, output);
		emit logError(log, QString("Could not start %1").arg(description));
		return false;
	} else {
		output = process.readAllStandardOutput();
		return true;
	}*/
	return true;
}

bool MainWindow::stopProcess(QPlainTextEdit *log, QProcess &process, QString description, QString stopCommand,
						QString &output)
{
	int ret;
	emit logInformation(log, QString("Stopping %1...").arg(description));
	if (!stopCommand.isEmpty()) {
		process.write(stopCommand.toLatin1());
		if (!process.waitForBytesWritten(5 * 1000)) {
			emit logError(log, QString("Could not write to %1").arg(description));
			output = process.readAllStandardOutput();
			emit logOutput(log, output);
		}
		if (!process.waitForFinished(5 * 1000)) {
			emit logError(log, QString("Could not stop %1 with command %2").arg(description, stopCommand));
			output = process.readAllStandardOutput();
			emit logOutput(log, output);
		} else {
			goto stopProcess_allfine;
		}
	}

	ret = kill(process.pid(), SIGINT);
	if (ret < 0) {
		emit logError(log, QString("Could not SIGINT %1: %2").arg(description, strerror(errno)));
	}
	if (!process.waitForFinished(5 * 1000)) {
		emit logError(log, QString("Could not stop %1 with SIGINT").arg(description));
		output = process.readAllStandardOutput();
		emit logOutput(log, output);
	} else {
		goto stopProcess_allfine;
	}

	process.terminate();
	if (!process.waitForFinished(5 * 1000)) {
		emit logError(log, QString("Could not stop %1 with SIGTERM").arg(description));
		output = process.readAllStandardOutput();
		emit logOutput(log, output);
	} else {
		goto stopProcess_allfine;
	}

	process.kill();
	if (!process.waitForFinished(5 * 1000)) {
		emit logError(log, QString("Could not stop %1 with SIGKILL").arg(description));
		output = process.readAllStandardOutput();
		emit logOutput(log, output);
	} else {
		goto stopProcess_allfine;
	}

	if (!process.state() == QProcess::NotRunning) {
		return false;
	}

stopProcess_allfine:
	emit logInformation(log, QString("OK: %1 stopped.").arg(description));
	output = process.readAllStandardOutput();
	return true;
}

