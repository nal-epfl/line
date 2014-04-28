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

#include "evudp.h"

#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>

#include "chronometer.h"
#include "util.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

UDPClient::UDPClient(int fd) :
	ReaderWriter(fd)
{
	qDebugT() << fd;
	tConnect = getCurrentTimeNanosec();
	stopped = false;
	totalRead = 0;
	totalWritten = 0;
	loop = NULL;
	w_read = NULL;
	w_write = NULL;
}

UDPClient::~UDPClient()
{
	qDebugT() << fd();
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

UDPClient* UDPClient::makeUDPClient(int fd, void *arg)
{
	Q_UNUSED(arg);
	return new UDPClient(fd);
}

// never call this directly
void UDPClient::read(QByteArray buffer)
{
	totalRead += buffer.count();
	onRead(buffer);
}

// call this to write
void UDPClient::write(QByteArray b)
{
	if (b.count() <= 0 || b.count() > 65536)
		return;

	ssize_t count;

	count = sendto(fd(), b.constData(), b.count(), 0, (struct sockaddr *) &remote_addr, sizeof(remote_addr));
	if (count <= 0) {
		perror("sendto");
		return;
	}

	totalWritten += count;
}

UDPServer::UDPServer(int fd) :
	m_fd(fd)
{
	qDebugT() << fd;
	loop = NULL;
	w_read = NULL;
	w_write = NULL;
}

UDPServer::~UDPServer()
{
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

void UDPServer::setFd(int fd)
{
	Q_ASSERT(m_fd < 0);
	m_fd = fd;
}

int UDPServer::fd()
{
	return m_fd;
}

// key = fd
static QHash<int, UDPClient*> UDPClients;
static QHash<int, UDPServer*> UDPServers;
// serverfd -> ip:port -> UDPClien*
static QHash<int, QHash<QString, UDPClient*> > UDPServerConnections;

static void udp_server_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void udp_server_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

static void closeAll()
{
	qDebugT();
	foreach (UDPClient *c, UDPClients.values()) {
		close(c->fd());
		c->stop();
		delete c;
	}
	UDPClients.clear();
	foreach (int server_fd, UDPServerConnections.uniqueKeys()) {
		foreach (UDPClient *c, UDPServerConnections[server_fd].values()) {
			c->stop();
			delete c;
		}
	}
	UDPServerConnections.clear();
	foreach (UDPServer *s, UDPServers.values()) {
		close(s->fd());
		delete s;
	}
	UDPServers.clear();
}

void udp_close_all()
{
	qDebugT();
	closeAll();
}

void udp_close_client(int fd)
{
	qDebugT() << fd;
    if (UDPClients.contains(fd)) {
        UDPClient *c = UDPClients[fd];
		int port = c->localPort;
		QString address = c->localAddress;
        close(c->fd());
        c->stop();
		UDPClients.remove(fd);
		delete c;
		c = NULL;
		foreach (int server_fd, UDPServerConnections.uniqueKeys()) {
			foreach (QString clientKey, UDPServerConnections[server_fd].uniqueKeys()) {
				UDPClient *sc = UDPServerConnections[server_fd][clientKey];
				// NOTE: this filter ignores the first 16 bits of the IP address, matching both 10.0 or 10.128 in
				// emulations, or 192.168 in experiments with real traffic.
				if (sc->remoteAddress.split(".").mid(2) ==
					address.split(".").mid(2) &&
					sc->remotePort == port) {
					// IMPORTANT: we do not close the socket, because it is the listening server socket. UDP does not
					// have separate sockets per flow.
					// All we need to do here is remove and free the server's client data structure.
					delete sc;
					sc = NULL;
					UDPServerConnections[server_fd].remove(clientKey);
				}
			}
		}
    }
}

#define BUFSIZE 65536
static void udp_server_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char client_addr_str[200] = "";
	ssize_t count = 0;
	char buffer[BUFSIZE];

	if ((count = recvfrom(watcher->fd, buffer, BUFSIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
		perror("server recvfrom");
		return;
	}

	inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_addr_str, 200);
	QString clientKey = QString("%1:%2:%3").arg(watcher->fd).arg(client_addr_str).arg(ntohs(client_addr.sin_port));

	if (!UDPServerConnections.contains(watcher->fd)) {
		UDPServerConnections[watcher->fd] = QHash<QString, UDPClient*>();
	}
	if (!UDPServerConnections[watcher->fd].contains(clientKey)) {
		qDebugT() << "UDP server: new client" << clientKey;
		UDPServerConnections[watcher->fd][clientKey] =
				UDPServers[watcher->fd]->clientFactoryCallback(watcher->fd,
															   UDPServers[watcher->fd]->clientFactoryCallbackArg);
		UDPServerConnections[watcher->fd][clientKey]->localAddress = UDPServers[watcher->fd]->localAddress;
		UDPServerConnections[watcher->fd][clientKey]->localPort = UDPServers[watcher->fd]->localPort;
		UDPServerConnections[watcher->fd][clientKey]->remoteAddress = client_addr_str;
		UDPServerConnections[watcher->fd][clientKey]->remotePort = ntohs(client_addr.sin_port);
		UDPServerConnections[watcher->fd][clientKey]->remote_addr = client_addr;
		// These two must be NULL, because the server "client" actually shares the fd and watcher of the server.
		UDPServerConnections[watcher->fd][clientKey]->w_read = NULL;
		UDPServerConnections[watcher->fd][clientKey]->w_write = NULL;
		UDPServerConnections[watcher->fd][clientKey]->loop = loop;
		UDPServerConnections[watcher->fd][clientKey]->onConnect();
	}

	if (count > 0) {
		UDPServerConnections[watcher->fd][clientKey]->read(QByteArray(buffer, count));
	} else if (count == 0) {
		qDebugT() << "UDP server: closing connection to client" << clientKey;
		UDPServerConnections[watcher->fd][clientKey]->stop();
	}
}

static void udp_server_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	foreach (UDPClient *c, UDPServerConnections[watcher->fd]) {
		// make sure we can still write
		struct pollfd pfds[1];
		pfds[0].fd = watcher->fd;
		pfds[0].events = POLLOUT;
		poll(pfds, 1, 0);
		if ((pfds[0].revents & POLLOUT) != 0) {
			c->onWrite();
		}
	}
}

static void udp_client_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	UDPClients[watcher->fd]->onWrite();
}

static void udp_client_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	Q_UNUSED(loop);

	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	struct sockaddr_in server_addr;
	socklen_t server_addr_len = sizeof(server_addr);
	ssize_t count = 0;
	char buffer[BUFSIZE];

	if ((count = recvfrom(watcher->fd, buffer, BUFSIZE, 0, (struct sockaddr *) &server_addr, &server_addr_len)) < 0) {
		perror("client recvfrom");
		return;
	}

	// TODO Should we handle count = 0 (peer closed connection -- how does that even work with UDP?!)?
	if (count == 0) {
		qDebugT() << "UDP client: read 0 bytes -- cannot handle this!!!";
	}

	// don't receive from strangers
	if (UDPClients[watcher->fd]->remote_addr.sin_addr.s_addr != server_addr.sin_addr.s_addr ||
		UDPClients[watcher->fd]->remote_addr.sin_port != server_addr.sin_port) {
		return;
	}

	UDPClients[watcher->fd]->read(QByteArray(buffer, count));
}

/**
  * Parameters:
  *  loop: libev event loop
  *  address: address to listen on
  *  port: port to listen on
  *  clientFactoryCallback: function used to construct a UDPClient instance on every accept()
  *  arg: argument to pass to clientFactoryCallback
  */
qint32 udp_server(struct ev_loop *loop, const char *address, int port,
				UDPClientFactoryCallback clientFactoryCallback, void *arg)
{
	qDebugT() << QString("UDP server: %1:%2").
				 arg(address).
				 arg(port);
	int fd;
	struct sockaddr_in addr;
	struct ev_io *w_read = (struct ev_io*) malloc(sizeof(struct ev_io));
	struct ev_io *w_write = (struct ev_io*) malloc(sizeof(struct ev_io));

	// Create server socket
	if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket error");
		exit(-1);
	}
	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		perror("fcntl error");
		exit(-1);
	}

	UDPServers[fd] = new UDPServer(fd);
	UDPServers[fd]->clientFactoryCallback = clientFactoryCallback;
	UDPServers[fd]->clientFactoryCallbackArg = arg;
	UDPServers[fd]->localAddress = address;
	UDPServers[fd]->localPort = port;
	UDPServers[fd]->loop = loop;
	UDPServers[fd]->w_read = w_read;
	UDPServers[fd]->w_write = w_write;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!inet_pton(AF_INET, address, &addr.sin_addr.s_addr)) {
		perror("inet_pton error");
		exit(-1);
	}

	// Bind socket to address
	if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		perror("bind error");
		exit(-1);
	}

	// Initialize and start a watcher to read datagrams
	ev_io_init(w_read, udp_server_read_cb, fd, EV_READ);
	ev_io_start(loop, w_read);
	ev_io_init(w_write, udp_server_write_cb, fd, EV_WRITE);
	ev_io_start(loop, w_write);

    // ALl fine
    return fd;
}

/**
  * Parameters:
  *  loop: libev event loop
  *  localAddress: local address to use (0.0.0.0 for anything)
  *  address: remote address to connect to
  *  port: remote port to connect to
  *  clientFactoryCallback: function used to construct a UDPClient instance
  *  arg: argument to pass to clientFactoryCallback
  */
qint32 udp_client(struct ev_loop *loop, const char *localAddress, const char *address, int port,
                int trafficClass, UDPClientFactoryCallback clientFactoryCallback, void *arg)
{
	int client_fd;
	struct ev_io *w_read = (struct ev_io*) malloc(sizeof(struct ev_io));
	struct ev_io *w_write = (struct ev_io*) malloc(sizeof(struct ev_io));
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	quint16 local_port = 0;

	// Create client socket
	if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket error");
		exit(-1);
	}
	if (fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		perror("fcntl error");
		exit(-1);
	}

	qDebugT() << "UDP client:" << localAddress << address << ":" << port << "class" << trafficClass;

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
	// Find our local port number
	if (getsockname(client_fd, (struct sockaddr *)&addr, &addr_len) == 0) {
		local_port = ntohs(addr.sin_port);
	}

	// Don't fragment
	{
		int val = IP_PMTUDISC_DO;
		setsockopt(client_fd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	}

	// b10 in the last two bits means ECN supported; UDP does not support ECN!
	quint8 diffserv = (quint8(qMin(7, qMax(0, trafficClass))) << 3) | quint8(0);
    setsockopt(client_fd, IPPROTO_IP, IP_TOS, &diffserv, sizeof(diffserv));

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!inet_pton(AF_INET, address, &addr.sin_addr.s_addr)) {
		perror("inet_pton error");
		exit(-1);
	}

	// UDPClients[client_fd] = new TCPSource(client_fd);
	UDPClients[client_fd] = clientFactoryCallback(client_fd, arg);
	UDPClients[client_fd]->remote_addr = addr;
	UDPClients[client_fd]->localAddress = localAddress;
	UDPClients[client_fd]->localPort = local_port;
	UDPClients[client_fd]->remoteAddress = address;
	UDPClients[client_fd]->remotePort = port;
	UDPClients[client_fd]->w_read = w_read;
	UDPClients[client_fd]->w_write = w_write;
	UDPClients[client_fd]->loop = loop;
	UDPClients[client_fd]->onConnect();

	// Initialize and start a watcher
	ev_io_init(w_write, udp_client_write_cb, client_fd, EV_WRITE);
	ev_io_start(loop, w_write);

	// Initialize and start a watcher
	ev_io_init(w_read, udp_client_read_cb, client_fd, EV_READ);
	ev_io_start(loop, w_read);

    // All fine
    return client_fd;
}

qreal udpRawRate2PayloadRate(qreal rawRate, int frameSize)
{
	return qreal(frameSize) / (frameSize + UDP_HEADER_OVERHEAD) * rawRate;
}
