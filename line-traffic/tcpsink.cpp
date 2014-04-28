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

#include "tcpsink.h"
#include "util.h"

TCPSink::TCPSink(int fd, TCPSinkArg params) :
	TCPClient(fd),
	params(params)
{
}

TCPClient* TCPSink::makeTCPSink(int fd, void *arg)
{
	return new TCPSink(fd, arg ? *(TCPSinkArg*)arg : TCPSinkArg());
}

void TCPSink::onRead(QByteArray buffer)
{
	if (params.closeOnNull) {
		if (!buffer.isEmpty()) {
			if (buffer.at(buffer.count() - 1) == '\0') {
				tcp_deferred_close_client(fd());
			}
		}
	}
}

void TCPSink::onStop()
{
	TCPClient::onStop();
	printDownloadStats();
}


TCPSinkArg::TCPSinkArg(bool closeOnNull)
	: closeOnNull(closeOnNull)
{
}
