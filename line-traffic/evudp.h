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

#ifndef EVUDP_H
#define EVUDP_H

#include <ev.h>
#include <netinet/in.h>
#include <QtCore>

#include "readerwriter.h"

// 14 ETH
// 20 B IP
// 8 B UDP
#define UDP_HEADER_OVERHEAD 42

qreal udpRawRate2PayloadRate(qreal rawRate, int frameSize);

class UDPClient;
typedef UDPClient* (*UDPClientFactoryCallback)(int, void*);

class UDPClient : public ReaderWriter {
public:
	UDPClient(int fd = -1);
	virtual ~UDPClient();

	static UDPClient* makeUDPClient(int fd, void *arg);
	/* For client endpoints, onConnect() is called right after the creation of the socket.
	   For server endpoints, onConnect() is called if a datagram is received from a new IP:port combination.
	*/

	// never call this directly
	void read(QByteArray b);
	// call this to write
	void write(QByteArray b);

	struct sockaddr_in remote_addr;
	struct ev_loop *loop;
	struct ev_io *w_read;
	struct ev_io *w_write;
};

class UDPServer {
public:
	inline UDPServer(int fd = -1);
	virtual ~UDPServer();
	void setFd(int fd);
	int fd();

	int m_fd;
	UDPClientFactoryCallback clientFactoryCallback;
	void *clientFactoryCallbackArg;
	QString localAddress;
	quint16 localPort;
	struct ev_loop *loop;
	struct ev_io *w_read;
	struct ev_io *w_write;
};

/** Call this on exit
*/
void udp_close_all();

void udp_close_client(int fd);

/**
  * Parameters:
  *  loop: libev event loop
  *  address: address to listen on
  *  port: port to listen on
  *  clientFactoryCallback: function used to construct a UDPClient instance on every accept()
  *  arg: argument to pass to clientFactoryCallback
  *
  * Returns: socket fd.
  */
qint32 udp_server(struct ev_loop *loop, const char *address, int port,
                  UDPClientFactoryCallback clientFactoryCallback = UDPClient::makeUDPClient, void *arg = 0);

/**
  * Parameters:
  *  loop: libev event loop
  *  localAddress: local address to use (0.0.0.0 for anything)
  *  address: remote address to connect to
  *  port: remote port to connect to
  *  clientFactoryCallback: function used to construct a UDPClient instance
  *  arg: argument to pass to clientFactoryCallback
  *
  * Returns: socket fd.
  */
qint32 udp_client(struct ev_loop *loop, const char *localAddress, const char *address, int port,
                  int trafficClass, UDPClientFactoryCallback clientFactoryCallback = UDPClient::makeUDPClient, void *arg = 0);


#endif // EVUDP_H
