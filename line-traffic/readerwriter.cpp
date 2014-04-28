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

#include "readerwriter.h"

#include "chronometer.h"
#include "util.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

ReaderWriter::ReaderWriter(int fd)
	: m_fd(fd)
{
	connected = false;
	stopped = false;
	totalRead = 0;
	totalWritten = 0;
}

ReaderWriter::~ReaderWriter()
{
}

void ReaderWriter::onConnect()
{
	connected = true;
	tConnect = getCurrentTimeNanosec();
}

void ReaderWriter::onRead(QByteArray buffer)
{
	Q_UNUSED(buffer);
	if (!connected)
		onConnect();
}

void ReaderWriter::onWrite()
{
	if (!connected)
		onConnect();
}

void ReaderWriter::onStop()
{
}

quint64 ReaderWriter::getTotalBytesRead()
{
	return totalRead;
}

quint64 ReaderWriter::getTotalBytesWritten()
{
	return totalWritten;
}

qreal ReaderWriter::getRunningTime()
{
	if (stopped)
		return totalRunningTime;
	return getCurrentTimeNanosec() - tConnect;
}

void ReaderWriter::setFd(int fd)
{
	Q_ASSERT(m_fd < 0);
	m_fd = fd;
}

int ReaderWriter::fd()
{
	return m_fd;
}

void ReaderWriter::printDownloadStats()
{
	quint64 downloadRate = (getRunningTime() > 0) ? (bPerSec2bPerNanosec(getTotalBytesRead()) / getRunningTime()) : 0;
	qDebugT() << QString("%1:%2 -> %3:%4 Average download rate: %5 B/s (%6 Mbps) Received: %7 B\n").
				 arg(remoteAddress).arg(remotePort).arg(localAddress).arg(localPort).
				 arg(withCommasStr(downloadRate)).
				 arg(withCommasStr(downloadRate * 1.0e-6 * 8.0)).
				 arg(totalRead);
}

void ReaderWriter::printUploadStats()
{
	quint64 uploadRate = (getRunningTime() > 0) ? (bPerSec2bPerNanosec(getTotalBytesWritten()) / getRunningTime()) : 0;
	qDebugT() << QString("%1:%2 -> %3:%4 Average upload rate: %5 B/s (%6 Mbps) Sent: %7 B\n").
				 arg(localAddress).arg(localPort).
				 arg(remoteAddress).arg(remotePort).
				 arg(withCommasStr(uploadRate)).
				 arg(withCommasStr(uploadRate * 1.0e-6 * 8.0)).
				 arg(totalWritten);
}

void ReaderWriter::write(QByteArray)
{
	// reimplement in child
}

void ReaderWriter::read(QByteArray)
{
	// reimplement in child
}

qint64 ReaderWriter::read()
{
	// reimplement in child
	return 0;
}

void ReaderWriter::stop()
{
	if (stopped)
		return;
	stopped = true;
	tStop = getCurrentTimeNanosec();
	totalRunningTime = tStop - tConnect;
	onStop();
}
