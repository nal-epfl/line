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

#include "remoteprocessssh.h"
#include "util.h"

#define DBG_SSH 0

RemoteProcessSsh::RemoteProcessSsh(QString keyPrefix)
	: keyPrefix(keyPrefix)
{
	connected = false;
	if (this->keyPrefix.isEmpty()) {
		this->keyPrefix = "default";
	}
	childrenKeys = QSet<QString>();
	childCount = 0;
}

RemoteProcessSsh::~RemoteProcessSsh()
{
	if (ssh.state() == QProcess::NotRunning)
		return;
	disconnect();
}

bool RemoteProcessSsh::connect(QString user, QString host, QString port)
{
	Q_ASSERT_FORCE(!connected);
	connected = true;
	this->host = host;
	this->user = user;
	this->port = port;
	bool result;
	ssh.setProcessChannelMode(QProcess::SeparateChannels);
	ssh.setReadChannel(QProcess::StandardOutput);
	sigset_t sigsetNew;
	sigemptyset(&sigsetNew);
	sigaddset(&sigsetNew, SIGINT);
	pthread_sigmask(SIG_BLOCK, &sigsetNew, NULL);
    ssh.start("ssh",
			  QStringList()
			  << "-T"
			  << "-C"
			  << QString("%1@%2").arg(user).arg(host)
			  << "-p" << port);
	result = ssh.waitForStarted(10000);
	pthread_sigmask(SIG_UNBLOCK, &sigsetNew, NULL);

//	QByteArray motd;
//	while ((motd = ssh.readAllStandardOutput()).length() == 0)
//		ssh.waitForReadyRead(100);
//	qDebug() << "motd" << motd;
	return result;
}

void RemoteProcessSsh::disconnect(bool cleanup)
{
	if (ssh.state() != QProcess::Running)
		return;

	if (cleanup) {
		foreach (QString key, childrenKeys) {
			if (isProcessRunning(key)) {
				signalProcess(key, 9);
				waitForFinished(key, 10);
			}
			sendCommand(QString("rm -f %1.*").arg(key));
		}

		// flush current output
		flush();
		if (sendCommand("hostname")) {
			readLine();
		}
	}

	ssh.terminate();
	ssh.waitForFinished(10000);
}

QString RemoteProcessSsh::startProcess(QString program, QStringList args)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	return startProcess(QString(program + ' ' + args.join(" ")));
}

QString RemoteProcessSsh::startProcess(QString command)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	qDebug() << keyPrefix << ":" << command;
	QString key = QString("%1_subproc_%2").arg(keyPrefix).arg(childCount);

	QString fullcommand = QString("rm -f %1.* ; (%2 1>%1.out 2>%1.err & pid=$! ; echo $pid >%1.pid ; wait $pid ; echo $? > %1.done ) &").arg(key, command);
	if (!sendCommand(fullcommand)) {
		return QString();
	}

	childrenKeys << key;
	childCount++;

	return key;
}

bool RemoteProcessSsh::isProcessRunning(QString key)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	if (!childrenKeys.contains(key)) {
		qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "No such process" << key;
		return false;
	}

	// flush current output
	flush();

	// send command
	QString command = QString("[ -f %1.done ] && echo Y || echo N").arg(key);
	if (!sendCommand(command)) {
		return false;
	}

	QString result = readLine().trimmed();
	if (result == "Y")
		return false;
	if (result == "N")
		return true;

	qDebug() << __FILE__ << __FUNCTION__ <<  __LINE__ << "problem, result is:" << result;
	return false;
}

bool RemoteProcessSsh::runCommand(QString command,
								  int *exitCode, QString *exitCodeStr,
								  QString *stdoutStr, QString *stderrStr)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	if (exitCode) {
		*exitCode = -1;
	}
	if (exitCodeStr) {
		*exitCodeStr = QString();
	}
	if (stdoutStr) {
		*stdoutStr = QString();
	}
	if (stderrStr) {
		*stderrStr = QString();
	}
	QString key = startProcess(command);
	if (!waitForFinished(key, -1)) {
		return false;
	}
	if (exitCode) {
		*exitCode = getExitCode(key);
	}
	if (exitCodeStr) {
		*exitCodeStr = getExitCodeStr(key);
	}
	if (stdoutStr) {
		*stdoutStr = readAllStdout(key);
	}
	if (stderrStr) {
		*stderrStr = readAllStderr(key);
	}
	return true;
}

bool RemoteProcessSsh::runRawCommand(QString command)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	return sendCommand(command);
}

bool RemoteProcessSsh::signalProcess(QString key, int sigNo)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	if (!childrenKeys.contains(key)) {
		qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "No such process" << key;
		return false;
	}

	// signal pid
	QString command = QString("kill -%2 $(cat %1.pid)").arg(key).arg(sigNo);
	return sendCommand(command);
}

bool RemoteProcessSsh::waitForFinished(QString key, int timeout_s)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	while (timeout_s != 0) {
		if (!isProcessRunning(key))
			return true;
		sleep(1);
		if (timeout_s > 0)
			timeout_s--;
	}
    return !isProcessRunning(key);
}

int RemoteProcessSsh::getExitCode(QString key)
{
    if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
    if (!childrenKeys.contains(key)) {
        qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "No such process" << key;
		return NoSuchProcess;
    }
    if (isProcessRunning(key)) {
        qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "Process did not exit" << key;
		return ProcessStillRunning;
    }

    // flush current output
    flush();

    // send command
    QString command = QString("cat %1.done").arg(key);
    if (!sendCommand(command)) {
		return OperationFailed;
    }

    QString result = readLine().trimmed();
	if (DBG_SSH) qDebug() << "String exit code is:" << result;
    bool ok;
    int exitCode = result.toInt(&ok);
    if (!ok) {
        qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "Bad exit code" << result << key;
		return OperationFailed;
    }

    qDebug() << __FILE__ << __FUNCTION__ <<  __LINE__ << "Exit code is:" << exitCode;
    return exitCode;
}

QString RemoteProcessSsh::getExitCodeStr(QString key)
{
    int exitCode = getExitCode(key);
	if (exitCode == NoSuchProcess) {
        return "Internal error: no such process";
	} else if (exitCode == ProcessStillRunning) {
        return "Internal error: process still running";
	} else if (exitCode == OperationFailed) {
		return "Internal error: cannot retrieve/parse exit code";
    } else if (exitCode == 0) {
        return "Success";
    } else if (exitCode > 128) {
        if (exitCode - 128 == SIGHUP) return "SIGHUP";
        if (exitCode - 128 == SIGINT) return "SIGINT";
        if (exitCode - 128 == SIGQUIT) return "SIGQUIT";
        if (exitCode - 128 == SIGILL) return "SIGILL";
        if (exitCode - 128 == SIGTRAP) return "SIGTRAP";
        if (exitCode - 128 == SIGABRT) return "SIGABRT";
        if (exitCode - 128 == SIGIOT) return "SIGIOT";
        if (exitCode - 128 == SIGBUS) return "SIGBUS";
        if (exitCode - 128 == SIGFPE) return "SIGFPE";
        if (exitCode - 128 == SIGKILL) return "SIGKILL";
		if (exitCode - 128 == SIGUSR1) return "SIGUSR1";
        if (exitCode - 128 == SIGSEGV) return "SIGSEGV";
		if (exitCode - 128 == SIGUSR2) return "SIGUSR2";
        if (exitCode - 128 == SIGPIPE) return "SIGPIPE";
        if (exitCode - 128 == SIGALRM) return "SIGALRM";
        if (exitCode - 128 == SIGTERM) return "SIGTERM";
        if (exitCode - 128 == SIGSTKFLT) return "SIGSTKFLT";
        if (exitCode - 128 == SIGCHLD) return "SIGCHLD";
        if (exitCode - 128 == SIGCONT) return "SIGCONT";
        if (exitCode - 128 == SIGSTOP) return "SIGSTOP";
        if (exitCode - 128 == SIGTSTP) return "SIGTSTP";
        if (exitCode - 128 == SIGTTIN) return "SIGTTIN";
        if (exitCode - 128 == SIGTTOU) return "SIGTTOU";
        if (exitCode - 128 == SIGURG) return "SIGURG";
        if (exitCode - 128 == SIGXCPU) return "SIGXCPU";
        if (exitCode - 128 == SIGXFSZ) return "SIGXFSZ";
        if (exitCode - 128 == SIGVTALRM) return "SIGVTALRM";
        if (exitCode - 128 == SIGPROF) return "SIGPROF";
        if (exitCode - 128 == SIGWINCH) return "SIGWINCH";
        if (exitCode - 128 == SIGIO) return "SIGIO";
        if (exitCode - 128 == SIGPOLL) return "SIGPOLL";
        if (exitCode - 128 == SIGPWR) return "SIGPWR";
        if (exitCode - 128 == SIGSYS) return "SIGSYS";
        return QString::number(exitCode);
    }
    return QString::number(exitCode);
}

QString RemoteProcessSsh::readAllStdout(QString key)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	// flush current output
	flush();

	// send command
	QString command = QString("cp %1.out %1.outtmp; wc -l %1.outtmp; cat %1.outtmp; rm %1.outtmp").arg(key);
	if (!sendCommand(command)) {
		return QString();
	}

	QString line = readLine();
	if (line.isEmpty())
		return QString();

	int lineCount = line.split(' ', QString::SkipEmptyParts).first().toInt();
	QString result;
	for (int i = 0; i < lineCount; i++) {
		line = readLine();
		result += line;
	}
	return result;
}

QString RemoteProcessSsh::readTailStdout(QString key)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	// flush current output
	flush();

	// send command
	QString command = QString("tail %1.out > %1.outtmp; wc -l %1.outtmp; cat %1.outtmp; rm %1.outtmp").arg(key);
	if (!sendCommand(command)) {
		return QString();
	}

	QString line = readLine();
	if (line.isEmpty())
		return QString();

	int lineCount = line.split(' ', QString::SkipEmptyParts).first().toInt();
	QString result;
	for (int i = 0; i < lineCount; i++) {
		line = readLine();
		result += line;
	}
	return result;
}

QString RemoteProcessSsh::readAllStderr(QString key)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	// flush current output
	flush();

	// send command
	QString command = QString("cp %1.err %1.errtmp; wc -l %1.errtmp; cat %1.errtmp; rm %1.errtmp").arg(key);
	if (!sendCommand(command)) {
		return QString();
	}

	QString line = readLine();
	if (line.isEmpty())
		return QString();

	int lineCount = line.split(' ', QString::SkipEmptyParts).first().toInt();
	QString result;
	for (int i = 0; i < lineCount; i++) {
		line = readLine();
		result += line;
	}
	return result;
}

QString RemoteProcessSsh::readTailStderr(QString key)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	// flush current output
	flush();

	// send command
	QString command = QString("tail %1.err > %1.errtmp; wc -l %1.errtmp; cat %1.errtmp; rm %1.errtmp").arg(key);
	if (!sendCommand(command)) {
		return QString();
	}

	QString line = readLine();
	if (line.isEmpty())
		return QString();

	int lineCount = line.split(' ', QString::SkipEmptyParts).first().toInt();
	QString result;
	for (int i = 0; i < lineCount; i++) {
		line = readLine();
		result += line;
	}
	return result;
}

bool RemoteProcessSsh::uploadToRemote(QString source, QString target)
{
	QString command = "scp";
	QStringList args = QStringList()
			<< "-C"
			<< "-P" << port
			<< source
			<< QString("%1@%2:%3").arg(user).arg(host).arg(target);

	if (QProcess::execute(command, args) != 0) {
		return false;
	}
	return true;
}

bool RemoteProcessSsh::downloadFromRemote(QString source, QString target)
{
	QString command = "scp";
	QStringList args = QStringList()
			<< "-C"
			<< "-P" << port
			<< QString("%1@%2:%3").arg(user).arg(host).arg(source)
			<< target;

	if (QProcess::execute(command, args) != 0) {
		return false;
	}
	return true;
}

QString RemoteProcessSsh::getHostname()
{
	return host;
}

bool RemoteProcessSsh::sendCommand(QString cmd)
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__ << cmd;
	if (!cmd.endsWith('\n'))
		cmd = cmd + '\n';
	QByteArray data = cmd.toAscii();
	while (!data.isEmpty()) {
		qint64 writtenCount = ssh.write(data);
		if (ssh.state() != QProcess::Running) {
			qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "QProcess not running!";
			qDebug() << ssh.readAllStandardOutput();
			qDebug() << ssh.readAllStandardError();
		}
		if (writtenCount < 0) {
			qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "Could not write to ssh (broken pipe)";
			return false;
		}
		data = data.mid(writtenCount);
		bool ok = ssh.waitForBytesWritten(10000);
		if (!ok) {
			qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "Could not write to ssh (timeout)";
			return false;
		}
	}
	return true;
}

QString RemoteProcessSsh::readLine()
{
	if (DBG_SSH) qDebug() << __FILE__ << __FUNCTION__;
	int sleepTime_ms = 1000;
	int maxTries = 20;

	QByteArray line = QByteArray();
	for (int tries = 0; tries < maxTries; tries++) {
		line.append(ssh.readLine());
		if (!line.isEmpty()) {
			if (line.endsWith('\n'))
				break;
			tries = -1;
		}
		ssh.waitForReadyRead(sleepTime_ms);
	}

	if (line.isEmpty())
		qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "Could not read from ssh (timeout)";

	// qDebug() << "READ:" << line;

	return QString(line);
}

void RemoteProcessSsh::flush()
{
	ssh.waitForReadyRead(100);
	if (DBG_SSH) qDebug() << "FLUSH:" << ssh.readAllStandardOutput();
	if (DBG_SSH) qDebug() << "FLUSH:" << ssh.readAllStandardError();
}
