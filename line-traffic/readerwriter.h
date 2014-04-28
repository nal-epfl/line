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

#ifndef READERWRITER_H
#define READERWRITER_H

#include <QtCore>

class ReaderWriter
{
public:
	ReaderWriter(int fd = 0);
	virtual ~ReaderWriter();

	virtual void onConnect();
	virtual void onRead(QByteArray buffer);
	virtual void onWrite();
	virtual void onStop();

	virtual quint64 getTotalBytesRead();
	virtual quint64 getTotalBytesWritten();
	virtual qreal getRunningTime();
	virtual void setFd(int fd);
	virtual int fd();

	virtual void printDownloadStats();
	virtual void printUploadStats();

	// Call this to write
	virtual void write(QByteArray b);

	// Never call this directly.
	virtual void read(QByteArray b);
	virtual qint64 read();

	// Never call this directly.
	// For TCP, use instead tcp_close_client() or the higher level stopConnection().
	virtual void stop();

	// socket
	int m_fd;
	// state
	bool connected;
	bool stopped;
	// nanoseconds
	quint64 tConnect;
	quint64 tStop;
	quint64 totalRunningTime;
	// bytes
	quint64 totalRead;
	quint64 totalWritten;
	// addressing (host order, human readable)
	QString localAddress;
	quint16 localPort;
	QString remoteAddress;
	quint16 remotePort;
};

#endif // READERWRITER_H
