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

#ifndef REMOTEPROCESSSSH_H
#define REMOTEPROCESSSSH_H

#include <signal.h>
#include <QtCore>

class RemoteProcessSsh
{
public:
	enum RemoteExitCode {
		NoSuchProcess = -99999,
		ProcessStillRunning = -88888,
		OperationFailed = -77777
	};

	RemoteProcessSsh(QString keyPrefix = QString());
	~RemoteProcessSsh();

	// Connect to the remote host.
	// You can call this only once. If you call it multiple times, the program will be aborted.
	bool connect(QString user, QString host, QString port);

	// Disconnect from the remote host.
	void disconnect(bool cleanup = true);

	// Blocking method of running programs remotely:

	// Runs a command remotely and waits for it to finish.
	// The command runs in a subshell.
	// Returns true if the command could be started, and it finished (regardless of the exit code).
	bool runCommand(QString command,
					int *exitCode = NULL,
					QString *exitCodeStr = NULL,
					QString *stdoutStr = NULL,
					QString *stderrStr = NULL);

	// Runs a raw command in the remote shell. Does not wait for it to finish.
	// Returns false in case of write error, otherwise it returns true.
	bool runRawCommand(QString command);

	// Non-blocking method of running programs remotely:

	// Starts a process remotely.
	// The command runs in a subshell.
	// Returns a key that uniquely identifies this process.
	QString startProcess(QString program, QStringList args);

	// Overloaded function.
	// Starts a process remotely.
	// The command runs in a subshell.
	// Returns a key that uniquely identifies this process.
	QString startProcess(QString command);

	// Returns true if the process is still running.
	bool isProcessRunning(QString key);

	// Sends a signal to a process.
	// Returns false in case of error.
	bool signalProcess(QString key, int sigNo);

	// Blocks until the process has finished.
	// Use timeout of -1 for unlimited block.
	// Returns false in case of error.
	// Can be called multiple times for the same process.
	bool waitForFinished(QString key, int timeout_s);

	// Returns the exit code of the process, or, in case of error:
	// NoSuchProcess, ProcessStillRunning, OperationFailed.
	// Can be called multiple times for the same process.
    int getExitCode(QString key);

	// Returns the exit code of the process, as a string message.
	// Can be called multiple times for the same process.
    QString getExitCodeStr(QString key);

	// Returns the standard output of the process, or a null QString in
	// case of error.
	QString readAllStdout(QString key);

	QString readTailStdout(QString key);

	// Returns the error output of the process, or a null QString in
	// case of error.
	QString readAllStderr(QString key);

	QString readTailStderr(QString key);

	// Uploads via scp a file. Source and target are paths (do not include username, host etc).
	bool uploadToRemote(QString source, QString target);

	// Downloads via scp a file. Source and target are paths (do not include username, host etc).
	bool downloadFromRemote(QString source, QString target);

	QString getHostname();

protected:
	// Used to keep track of whether we tried to connect.
	bool connected;
	// This is the local ssh client process, created during connect().
	QProcess ssh;
	// This string is prepended to all process keys.
	QString keyPrefix;
	// Counter incremented for every process that is created - used to generate unique keys.
	int childCount;
	// Stores the keys for all the child processes that were ever started.
	QSet<QString> childrenKeys;

	QString host;
	QString port;
	QString user;

	QString readLine();
	void flush();
	bool sendCommand(QString cmd);
};

typedef QSharedPointer<RemoteProcessSsh> PointerSsh;

#endif // REMOTEPROCESSSSH_H
