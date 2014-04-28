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

#include "evtcp.h"

#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/tcp.h>

#include "chronometer.h"
#include "util.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

bool setTCPSocketReceiveWindow(int fd, int win) {
	int recv_win = win;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_win, sizeof(recv_win)) < 0) {
		return false;
	}
	if (setsockopt(fd, SOL_SOCKET, TCP_WINDOW_CLAMP, &recv_win, sizeof(recv_win)) < 0) {
		return false;
	}
	return true;
}

bool setTCPSocketCongestionControl(int fd, QString congestionControl) {
	QByteArray cc = congestionControl.toLatin1();
	if (setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, cc.constData(), cc.length()) < 0) {
		return false;
	}
	return true;
}

TCPClient::TCPClient(int fd) :
	ReaderWriter(fd)
{
	qDebugT() << fd;
	connected = false;
	stopped = false;
	totalRead = 0;
	totalWritten = 0;
	w_connect = NULL;
	w_read = NULL;
	w_write = NULL;
	loop = NULL;
	transferCompletedCallback = NULL;
	transferCompletedCallbackArg = NULL;
}

TCPClient::~TCPClient()
{
	qDebugT() << fd();
	if (w_connect) {
		ev_io_stop(loop, w_connect);
		free(w_connect);
		w_connect = 0;
	}
	if (w_read) {
		ev_io_stop(loop, w_read);
		free(w_read);
		w_read = 0;
	}
	if (w_write) {
		ev_io_stop(loop, w_write);
		free(w_write);
		w_write = 0;
	}
}

TCPClient* TCPClient::makeTCPClient(int fd, void *arg)
{
	Q_UNUSED(arg);
	return new TCPClient(fd);
}

// never call this directly
qint64 TCPClient::read()
{
	qint64 result = 0;
	QByteArray readBuffer;
	while (1) {
		char buffer[20000];
		ssize_t count = recv(m_fd, buffer, 20000, 0);
		if (count <= 0) {
			if (count == EWOULDBLOCK || count == EAGAIN) {
				// nothing
			} else {
				result = count;
			}
		} else {
			result += count;
		}
		if (count <= 0)
			break;
		readBuffer.append(buffer, count);
	}
	totalRead += readBuffer.count();
	onRead(readBuffer);
	return result;
}

// call this to write
void TCPClient::write(QByteArray b)
{
	writeBuffer.append(b);

	while (!writeBuffer.isEmpty()) {
		ssize_t count = send(m_fd, writeBuffer.constData(), writeBuffer.count(), 0);
		if (count <= 0)
			break;
		writeBuffer.remove(0, count);
		totalWritten += count;
	}
}

TCPServer::TCPServer(int fd) :
	m_fd(fd)
{
	qDebugT() << fd;
	w_accept = 0;
	loop = 0;
}

TCPServer::~TCPServer()
{
	qDebugT() << fd();
	if (w_accept) {
		ev_io_stop(loop, w_accept);
		free(w_accept);
	}
}

void TCPServer::setFd(int fd)
{
	Q_ASSERT(m_fd < 0);
	m_fd = fd;
}

int TCPServer::fd()
{
	return m_fd;
}

// key = fd
static QHash<int, TCPClient*> tcpClients;
static QHash<int, TCPServer*> tcpServers;
static QHash<int, TCPClient*> tcpServerConnections;

static void tcp_server_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void tcp_server_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void tcp_server_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

static void closeAll()
{
	qDebugT();
	foreach (TCPClient *c, tcpClients.values()) {
		close(c->fd());
		c->stop();
		delete c;
	}
	tcpClients.clear();
	foreach (TCPClient *c, tcpServerConnections.values()) {
		close(c->fd());
		c->stop();
		delete c;
	}
	tcpServerConnections.clear();
	foreach (TCPServer *s, tcpServers.values()) {
		close(s->fd());
		delete s;
	}
	tcpServers.clear();
}

void tcp_close_all()
{
	qDebugT();
	closeAll();
}

void tcp_close_client(int fd)
{
	qDebugT() << fd;
    if (tcpClients.contains(fd)) {
        TCPClient *c = tcpClients[fd];
		int port = c->localPort;
		QString address = c->localAddress;
        close(c->fd());
        c->stop();
		tcpClients.remove(fd);
		delete c;
		c = NULL;
		foreach (TCPClient *sc, tcpServerConnections.values()) {
			// NOTE: this filter ignores the first 16 bits of the IP address, matching both 10.0 or 10.128 in
			// emulations, or 192.168 in experiments with real traffic.
			if (sc->remoteAddress.split(".").mid(2) == address.split(".").mid(2) &&
				sc->remotePort == port) {
				close(sc->fd());
				sc->stop();
				tcpServerConnections.remove(sc->fd());
				delete sc;
				sc = NULL;
			}
		}
    }
}

QSet<int> tcp_fds_to_close;

void tcp_deferred_close_client_helper(int, void *)
{
	foreach (int fd, tcp_fds_to_close) {
		qDebugT() << fd;
		if (tcpClients.contains(fd)) {
			TCPClient *c = tcpClients[fd];
			if (c->transferCompletedCallback) {
				c->transferCompletedCallback(c->transferCompletedCallbackArg);
			}
		}
		tcp_close_client(fd);
	}
	tcp_fds_to_close.clear();
}

void tcp_deferred_close_client(int fd)
{
	qDebugT() << fd;
	if (tcpClients.contains(fd)) {
		TCPClient *c = tcpClients[fd];
		tcp_fds_to_close.insert(fd);
		ev_once(c->loop, -1, 0, 0, tcp_deferred_close_client_helper, 0);
	}
}

/* Accept client requests */
static void tcp_server_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	qDebugT();
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	char client_addr_str[200] = "";
	int client_fd;
	struct ev_io *w_client_write = (struct ev_io*) malloc(sizeof(struct ev_io));
	struct ev_io *w_client_read = (struct ev_io*) malloc(sizeof(struct ev_io));

	if (EV_ERROR & revents) {
		perror("got invalid event");
		exit(-1);
	}

	// Accept client request
	client_fd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);
	qDebugT() << client_fd;

	if (client_fd < 0) {
		perror("accept error");
		exit(-1);
	}

	if (fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		perror("fcntl error");
		exit(-1);
	}

	if (tcpServers[watcher->fd]->tcpReceiveWindow > 0) {
		if (!setTCPSocketReceiveWindow(client_fd, tcpServers[watcher->fd]->tcpReceiveWindow)) {
			perror("Error setting receive window size\n");
			exit(-1);
		}
	}

	if (!tcpServers[watcher->fd]->tcpCongestionControl.isEmpty()) {
		if (!setTCPSocketCongestionControl(client_fd, tcpServers[watcher->fd]->tcpCongestionControl)) {
			perror("Error setting TCP congestion control algorithm\n");
			exit(-1);
		}
	}

	inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_addr_str, 200);

	tcpServerConnections[client_fd] = tcpServers[watcher->fd]->clientFactoryCallback(client_fd, tcpServers[watcher->fd]->clientFactoryCallbackArg);
	tcpServerConnections[client_fd]->localAddress = tcpServers[watcher->fd]->localAddress;
	tcpServerConnections[client_fd]->localPort = tcpServers[watcher->fd]->localPort;
	tcpServerConnections[client_fd]->remoteAddress = client_addr_str;
	tcpServerConnections[client_fd]->remotePort = ntohs(client_addr.sin_port);
	tcpServerConnections[client_fd]->onConnect();
	tcpServerConnections[client_fd]->w_read = w_client_read;
	tcpServerConnections[client_fd]->w_write = w_client_write;
	tcpServerConnections[client_fd]->loop = loop;

	// printf("Successfully connected with client.\n");

	// Initialize and start watcher to read client requests
	ev_io_init(w_client_write, tcp_server_write_cb, client_fd, EV_WRITE);
	ev_io_start(loop, w_client_write);

	ev_io_init(w_client_read, tcp_server_read_cb, client_fd, EV_READ);
	ev_io_start(loop, w_client_read);
}

static void tcp_server_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	tcpServerConnections[watcher->fd]->onWrite();
}

static void tcp_server_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	int code = tcpServerConnections[watcher->fd]->read();
	if (code == 0) {
		// close
		int fd = watcher->fd;
		close(fd);
		tcpServerConnections[fd]->stop();
		delete tcpServerConnections[fd];
		tcpServerConnections.remove(fd);
	}
}

static void tcp_client_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	if (!tcpClients[watcher->fd]->connected) {
		int code;
		socklen_t optlen = sizeof(code);
		if (getsockopt(watcher->fd, SOL_SOCKET, SO_ERROR, &code, &optlen) < 0) {
			perror("getsockopt error");
			exit(-1);
		}
		if (code == EINPROGRESS)
			return;
		if (code) {
			perror("connect callback error");
			qDebug() << code << strerror(code);
			if (code == EHOSTUNREACH) {
				qDebug() << "Could not connect" <<
							tcpClients[watcher->fd]->localAddress << "->" << tcpClients[watcher->fd]->remoteAddress;
				qDebug() << "You might have to redeploy.";
			}
			exit(-1);
		}
		if (code == 0) {
			struct sockaddr_in address;
			socklen_t addrlen = sizeof(address);
			bzero(&address, sizeof(address));
			if (getsockname(watcher->fd, (struct sockaddr*)&address, &addrlen) == 0) {
				char client_addr_str[200] = "";
				if (inet_ntop(AF_INET, &address.sin_addr.s_addr, client_addr_str, 200) == 0) {
					tcpClients[watcher->fd]->localAddress = client_addr_str;
				}
				tcpClients[watcher->fd]->localPort = ntohs(address.sin_port);
			}
			tcpClients[watcher->fd]->onConnect();
		}
	}

	if (tcpClients[watcher->fd]->connected) {
		tcpClients[watcher->fd]->onWrite();
	}
}

static void tcp_client_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	tcpClients[watcher->fd]->read();
}

/**
  * Parameters:
  *  loop: libev event loop
  *  address: address to listen on
  *  port: port to listen on
  *  clientFactoryCallback: function used to construct a TCPClient instance on every accept()
  *  clientFactoryCallbackArg: argument to pass to clientFactoryCallback
  */
qint32 tcp_server(struct ev_loop *loop,
				  const char *address,
				  int port,
				  TCPClientFactoryCallback clientFactoryCallback,
				  void *clientFactoryCallbackArg,
				  int tcpReceiveWindow,
				  QString tcpCongestionControl)
{
	int fd;
	struct sockaddr_in addr;
	struct ev_io *w_accept = (struct ev_io*) malloc(sizeof(struct ev_io));

	qDebugT() << "TCP server:" << address << ":" << port << "rwin" << tcpReceiveWindow;

	// Create server socket
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit(-1);
	}

	qDebugT() << fd;

	int reuseaddr = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
		perror("socket error");
		exit(-1);
	}

	if (tcpReceiveWindow > 0) {
		if (!setTCPSocketReceiveWindow(fd, tcpReceiveWindow)) {
			perror("Error setting receive window size\n");
			exit(-1);
		}
	}

	if (!tcpCongestionControl.isEmpty()) {
		if (!setTCPSocketCongestionControl(fd, tcpCongestionControl)) {
			perror("Error setting TCP congestion control algorithm\n");
			exit(-1);
		}
	}

	tcpServers[fd] = new TCPServer(fd);
	tcpServers[fd]->clientFactoryCallback = clientFactoryCallback;
	tcpServers[fd]->clientFactoryCallbackArg = clientFactoryCallbackArg;
	tcpServers[fd]->localAddress = address;
	tcpServers[fd]->localPort = port;
	tcpServers[fd]->w_accept = w_accept;
	tcpServers[fd]->loop = loop;
	tcpServers[fd]->tcpReceiveWindow = tcpReceiveWindow;
	tcpServers[fd]->tcpCongestionControl = tcpCongestionControl;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!inet_pton(AF_INET, address, &addr.sin_addr)) {
		perror("inet_pton error");
		exit(-1);
	}

	// Bind socket to address
	if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		perror("bind error");
		exit(-1);
	}

	// Start listing on the socket
	if (listen(fd, INT_MAX) < 0) {
		perror("listen error");
		exit(-1);
	}

	// Initialize and start a watcher to accepts client requests
	ev_io_init(w_accept, tcp_server_accept_cb, fd, EV_READ);
	ev_io_start(loop, w_accept);

    // All fine
    return fd;
}

/**
  * Parameters:
  *  loop: libev event loop
  *  localAddress: local address to use (0.0.0.0 for anything)
  *  address: remote address to connect to
  *  port: remote port to connect to
  *  clientFactoryCallback: function used to construct a TCPClient instance
  *  clientFactoryCallbackArg: argument to pass to clientFactoryCallback
  */
qint32 tcp_client(struct ev_loop *loop,
				  const char *localAddress,
				  const char *address,
				  int port,
				  int trafficClass,
				  TCPClientFactoryCallback clientFactoryCallback,
				  void *clientFactoryCallbackArg,
				  int tcpReceiveWindow,
				  QString tcpCongestionControl,
				  TCPClientTransferCompletedCallback transferCompletedCallback,
				  void *transferCompletedCallbackArg)
{
	int client_fd;
	struct ev_io *w_connect = (struct ev_io*) malloc(sizeof(struct ev_io));
	struct ev_io *w_read = (struct ev_io*) malloc(sizeof(struct ev_io));
	struct sockaddr_in addr;

	qDebugT() << "TCP client:" << localAddress  << address << ":" << port << "class" << trafficClass;

	// Create client socket
	if ((client_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit(-1);
	}

	qDebugT() << client_fd;

	if (fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		perror("fcntl error");
		exit(-1);
	}
	int reuseaddr = 1;
	if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
		perror("socket error");
		exit(-1);
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	if (!inet_pton(AF_INET, localAddress, &addr.sin_addr.s_addr)) {
		perror("inet_pton error");
		exit(-1);
	}

	// Bind socket to address
	if (bind(client_fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		perror("client bind error");
		exit(-1);
	}

	// b10 in the last two bits means ECN supported
	quint8 diffserv = (quint8(qMin(7, qMax(0, trafficClass))) << 3) | quint8(2);
    setsockopt(client_fd, IPPROTO_IP, IP_TOS, &diffserv, sizeof(diffserv));

	if (tcpReceiveWindow > 0) {
		if (!setTCPSocketReceiveWindow(client_fd, tcpReceiveWindow)) {
			perror("Error setting receive window size\n");
			exit(-1);
		}
	}

	if (!tcpCongestionControl.isEmpty()) {
		if (!setTCPSocketCongestionControl(client_fd, tcpCongestionControl)) {
			perror("Error setting TCP congestion control algorithm\n");
			exit(-1);
		}
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!inet_pton(AF_INET, address, &addr.sin_addr.s_addr)) {
		perror("inet_pton error");
		exit(-1);
	}

	// tcpClients[client_fd] = new TCPSource(client_fd);
	tcpClients[client_fd] = clientFactoryCallback(client_fd, clientFactoryCallbackArg);
	tcpClients[client_fd]->localAddress = localAddress;
	tcpClients[client_fd]->localPort = ntohs(addr.sin_port); // probably only available after connect
	tcpClients[client_fd]->remoteAddress = address;
	tcpClients[client_fd]->remotePort = port;
	tcpClients[client_fd]->w_connect = w_connect;
	tcpClients[client_fd]->w_read = w_read;
	tcpClients[client_fd]->loop = loop;
	tcpClients[client_fd]->transferCompletedCallback = transferCompletedCallback;
	tcpClients[client_fd]->transferCompletedCallbackArg = transferCompletedCallbackArg;

	// Initialize and start a watcher
	ev_io_init(w_connect, tcp_client_write_cb, client_fd, EV_WRITE);
	ev_io_start(loop, w_connect);

	// Initialize and start a watcher
	ev_io_init(w_read, tcp_client_read_cb, client_fd, EV_READ);
	ev_io_start(loop, w_read);

	// Connect to server socket
	if (connect(client_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		if (errno != EINPROGRESS) {
			perror("Connect error");
			exit(-1);
		}
	}

    // All fine
    return client_fd;
}

