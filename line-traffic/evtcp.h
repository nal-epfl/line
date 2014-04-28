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

#ifndef EVTCP_H
#define EVTCP_H

#include <ev.h>
#include <QtCore>

#include "readerwriter.h"

class TCPClient;
typedef TCPClient* (*TCPClientFactoryCallback)(int, void*);

typedef void (*TCPClientTransferCompletedCallback)(void*);

class TCPClient : public ReaderWriter {
public:
	TCPClient(int fd = -1);
	virtual ~TCPClient();

	static TCPClient* makeTCPClient(int fd, void *arg);

	// Call this to write
	virtual void write(QByteArray b);
	// Never call this directly.
	// The parameter b is ignored
	virtual qint64 read();

	QByteArray writeBuffer;
	struct ev_io *w_connect;
	struct ev_io *w_read;
	struct ev_io *w_write;
	struct ev_loop *loop;

	// Other parameters
	TCPClientTransferCompletedCallback transferCompletedCallback;
	void *transferCompletedCallbackArg;
};

class TCPServer {
public:
	inline TCPServer(int fd = -1);
	virtual ~TCPServer();
	void setFd(int fd);
	int fd();

	int m_fd;
	TCPClientFactoryCallback clientFactoryCallback;
	void *clientFactoryCallbackArg;
	QString localAddress;
	quint16 localPort;
	struct ev_io *w_accept;
	struct ev_loop *loop;
	// <= 0 means OS default
	int tcpReceiveWindow;
	// "" means OS default
	QString tcpCongestionControl;
};

/** Call this on exit
*/
// NOT usable from TCPClient methods!
void tcp_close_all();

// NOT usable from TCPClient methods!
void tcp_close_client(int fd);

// Usable from TCPClient methods
void tcp_deferred_close_client(int fd);

// Usable from TCPClient methods
void tcp_deferred_restart_client(int fd);

/**
  * Parameters:
  *  loop: libev event loop
  *  address: address to listen on
  *  port: port to listen on
  *  clientFactoryCallback: function used to construct a TCPClient instance on every accept()
  *  arg: argument to pass to clientFactoryCallback
  *
  * Returns: socket fd.
  */
qint32 tcp_server(struct ev_loop *loop,
				  const char *address,
				  int port,
				  TCPClientFactoryCallback clientFactoryCallback = TCPClient::makeTCPClient,
				  void *clientFactoryCallbackArg = 0,
				  int tcpReceiveWindow = 0,
				  QString tcpCongestionControl = QString());

/**
  * Parameters:
  *  loop: libev event loop
  *  localAddress: local address to use (0.0.0.0 for anything)
  *  address: remote address to connect to
  *  port: remote port to connect to
  *  clientFactoryCallback: function used to construct a TCPClient instance
  *  arg: argument to pass to clientFactoryCallback
  *
  * Returns: socket fd.
  */
qint32 tcp_client(struct ev_loop *loop,
				  const char *localAddress,
				  const char *address,
				  int port,
				  int trafficClass,
				  TCPClientFactoryCallback clientFactoryCallback = TCPClient::makeTCPClient,
				  void *clientFactoryCallbackArg = 0,
				  int tcpReceiveWindow = 0,
				  QString tcpCongestionControl = QString(),
				  TCPClientTransferCompletedCallback transferCompletedCallback = NULL,
				  void *transferCompletedCallbackArg = NULL);

#endif // EVTCP_H
