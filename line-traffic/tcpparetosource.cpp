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

#include "tcpparetosource.h"
#include "util.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

TCPParetoSource::TCPParetoSource(int fd, TCPParetoSourceArg params)
	: TCPClient(fd)
{
	// uniform is uniformly distributed in (0, 1]
	qreal uniform = 1.0 - frandex();
	transferSize = params.scale / pow(uniform, (1.0/params.alpha));
	qDebugT() << "Pareto transfer size (bytes): " << transferSize;
}

TCPClient *TCPParetoSource::makeTCPParetoSource(int fd, void *arg)
{
	TCPParetoSourceArg *params = (TCPParetoSourceArg*)arg;
	return new TCPParetoSource(fd, *params);
}

void TCPParetoSource::onWrite()
{
	TCPClient::onWrite();

	if (getTotalBytesWritten() >= transferSize) {
		qDebugT() << "TCPParetoSource finished";
		tcp_deferred_close_client(m_fd);
		return;
	}

	QByteArray b;
	quint64 left = transferSize - getTotalBytesWritten();
	const unsigned int bufferSize = 10000;
	if (left > bufferSize) {
		b.fill('z', bufferSize);
	} else {
		b.fill('z', left - 1);
		b.append('\0');
	}
	write(b);
}

void TCPParetoSource::onStop()
{
	TCPClient::onStop();
	printUploadStats();
}
