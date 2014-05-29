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

#include <ev.h>
#include <stdio.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <QtCore>

#include "netgraph.h"
#include "evtcp.h"
#include "tcpsource.h"
#include "tcpsink.h"
#include "tcpparetosource.h"
#include "tcpdashsource.h"
#include "evudp.h"
#include "udpsource.h"
#include "udpsink.h"
#include "udpcbr.h"
#include "udpvbr.h"
#include "util.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

void sigint_cb(struct ev_loop *loop, struct ev_signal *w, int revents)
{
	Q_UNUSED(w);
	Q_UNUSED(revents);
	fprintf(stderr, "\nKeyboard interrupt: closing all connections...\n\n");
	tcp_close_all();
	udp_close_all();

	ev_unloop(loop, EVUNLOOP_ALL);
}

NetGraph netGraph;
QHash<ev_timer *, int> onOffTimer2Connection;

void poissonStartConnectionTimeoutHandler(int revents, void *arg);
void connectionTransferCompletedHandler(void *arg);

// Assigns port numbers to connections.
void assignPorts()
{
	qDebugT();
    netGraph.assignPorts();
}

// Starts servers (sinks).
void initConnection(struct ev_loop *loop, NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.dest].maskedOut)
		return;

	if (c.basicType == "TCP") {
        int port = netGraph.connections[c.index].port;
		c.serverFD = tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								TCPSink::makeTCPSink, NULL,
								c.tcpReceiveWindowSize,
								c.tcpCongestionControl);
	} else if (c.basicType == "TCPx") {
		for (int i = 0; i < c.multiplier; i++) {
            int port = netGraph.connections[c.index].ports[i];
			c.serverFDs.insert(tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
							   TCPSink::makeTCPSink, NULL, c.tcpReceiveWindowSize, c.tcpCongestionControl));
        }
	} else if (c.basicType == "TCP-Poisson-Pareto") {
		int port = netGraph.connections[c.index].port;
		c.serverFD = tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
				TCPSink::makeTCPSink, new TCPSinkArg(true), c.tcpReceiveWindowSize, c.tcpCongestionControl);
		// The first transmission is delayed exponentially
		c.delayStart = true;
    } else if (c.basicType == "TCP-DASH") {
        int port = netGraph.connections[c.index].port;
        c.serverFD = tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
                TCPSink::makeTCPSink, new TCPSinkArg(true), c.tcpReceiveWindowSize, c.tcpCongestionControl);
    } else if (c.basicType == "UDP-CBR") {
		int port = netGraph.connections[c.index].port;
		c.serverFD = udp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								UDPSink::makeUDPSink);
	} else if (c.basicType == "UDP-VBR") {
		int port = netGraph.connections[c.index].port;
		c.serverFD = udp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								UDPVBRSink::makeUDPVBRSink);
	} else if (c.basicType == "UDP-VCBR") {
		int port = netGraph.connections[c.index].port;
		c.serverFD = udp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								UDPSink::makeUDPSink);
	} else {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
		Q_ASSERT_FORCE(false);
	}
}

// Starts clients (sources).
void startConnection(NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.source].maskedOut)
		return;
	struct ev_loop *loop = static_cast<struct ev_loop *>(c.ev_loop);
	if (c.basicType == "TCP") {
        int port = netGraph.connections[c.index].port;
		c.clientFD = tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
				qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
				c.trafficClass, TCPSource::makeTCPSource, NULL, c.tcpReceiveWindowSize, c.tcpCongestionControl);
	} else if (c.basicType == "TCPx") {
        c.clientFDs.clear();
		for (int i = 0; i < c.multiplier; i++) {
            int port = netGraph.connections[c.index].ports[i];
			c.clientFDs.insert(tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
							   qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
					c.trafficClass, TCPSource::makeTCPSource, NULL, c.tcpReceiveWindowSize, c.tcpCongestionControl));
        }
	} else if (c.basicType == "TCP-Poisson-Pareto") {
		TCPParetoSourceArg paretoParams;
		paretoParams.alpha = c.paretoAlpha;
		paretoParams.scale = c.paretoScale_b / 8.0;

		if (!c.delayStart) {
			qDebugT() << "Creating Poisson connection";
			int port = netGraph.connections[c.index].port;
			c.clientFD = tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
					qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
					c.trafficClass, TCPParetoSource::makeTCPParetoSource, &paretoParams, c.tcpReceiveWindowSize,
					c.tcpCongestionControl,
					c.sequential ? connectionTransferCompletedHandler : NULL,
					c.sequential ? &c : NULL);
		}
		if (c.delayStart || !c.sequential) {
			// schedule next start
			c.delayStart = false;
			qreal delay = -log(1.0 - frandex()) / c.poissonRate;
			qDebugT() << "Delaying Poisson connection" << delay;
			ev_once(loop, -1, 0, delay, poissonStartConnectionTimeoutHandler, &c);
		}
    } else if (c.basicType == "TCP-DASH") {
        TCPDashSourceArg params(c.rate_Mbps * 1.0e6 / 8.0,
                                c.bufferingRate_Mbps * 1.0e6 / 8.0,
                                c.bufferingTime_s,
                                c.streamingPeriod_s);
        qDebugT() << "Creating DASH connection";
        int port = netGraph.connections[c.index].port;
        c.clientFD = tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
                qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
                c.trafficClass, TCPDashSource::makeTCPDashSource, &params, c.tcpReceiveWindowSize,
                c.tcpCongestionControl);
    } else if (c.basicType == "UDP-CBR") {
		qreal rate_Bps = c.rate_Mbps * 1.0e6 / 8.0;
		int port = netGraph.connections[c.index].port;
		UDPCBRSourceArg params(rate_Bps, 1400, c.poisson);
		c.clientFD = udp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
				qPrintable(netGraph.nodes[c.dest].ipForeign()), port, c.trafficClass,
				UDPCBRSource::makeUDPCBRSource, &params);
	} else if (c.basicType == "UDP-VBR") {
		int port = netGraph.connections[c.index].port;
		UDPVBRSourceArg params(1000);
		c.clientFD = udp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
				qPrintable(netGraph.nodes[c.dest].ipForeign()), port, c.trafficClass,
				UDPVBRSource::makeUDPVBRSource, &params);
	} else if (c.basicType == "UDP-VCBR") {
		qreal rate_Bps = c.rate_Mbps * 1.0e6 / 8.0;
		int port = netGraph.connections[c.index].port;
		UDPVCBRSourceArg params(rate_Bps);
		c.clientFD = udp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
				qPrintable(netGraph.nodes[c.dest].ipForeign()), port, c.trafficClass,
				UDPVCBRSource::makeUDPVCBRSource, &params);
	} else {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
		Q_ASSERT_FORCE(false);
	}
}

// Stops clients (sources)
void stopConnection(NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.source].maskedOut)
		return;

	if (c.basicType == "TCP") {
        tcp_close_client(c.clientFD);
        c.clientFD = -1;
	} else if (c.basicType == "TCPx") {
		foreach (int fd, c.clientFDs) {
			tcp_close_client(fd);
        }
	} else if (c.basicType == "UDP-CBR" || c.basicType == "UDP-VBR" || c.basicType == "UDP-VCBR") {
		udp_close_client(c.clientFD);
		c.clientFD = -1;
	} else {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
		Q_ASSERT_FORCE(false);
	}
}

void connectionTransferCompleted(NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.source].maskedOut)
		return;

	if (c.basicType == "TCP") {
		// Nothing to do
	} else if (c.basicType == "TCPx") {
		// Nothing to do
	} else if (c.basicType == "TCP-Poisson-Pareto") {
		netGraph.connections[c.index].delayStart = true;
		ev_once(static_cast<struct ev_loop*>(c.ev_loop),
				-1, 0, 0, poissonStartConnectionTimeoutHandler, &netGraph.connections[c.index]);
	} else if (c.basicType == "TCP-Repeated-Pareto") {
		// Nothing to do
    } else if (c.basicType == "TCP-DASH") {
        // Nothing to do
    } else if (c.basicType == "UDP-CBR") {
		// Nothing to do
	} else if (c.basicType == "UDP-VBR") {
		// Nothing to do
	} else if (c.basicType == "UDP-VCBR") {
		// Nothing to do
	} else {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
		Q_ASSERT_FORCE(false);
	}
}

void timeout_start_connection(struct ev_loop *loop, ev_timer *w, int revents)
{
	qDebugT();
	Q_UNUSED(loop);
    Q_UNUSED(revents);
	if (onOffTimer2Connection.contains(w)) {
		startConnection(netGraph.connections[onOffTimer2Connection[w]]);
    }
}

void timeout_stop_connection(struct ev_loop *loop, ev_timer *w, int revents)
{
	qDebugT();
    Q_UNUSED(loop);
    Q_UNUSED(revents);
	if (onOffTimer2Connection.contains(w)) {
		stopConnection(netGraph.connections[onOffTimer2Connection[w]]);
    }
}

void createOnOffTimers()
{
	qDebugT();
    foreach (NetGraphConnection c, netGraph.connections) {
		if (c.maskedOut)
			continue;
		if (netGraph.nodes[c.source].maskedOut)
			return;
		qreal firstStart = c.onOff ? frand() * (c.onDurationMax + c.offDurationMax) : frand();
		qreal onDuration = c.onOff ? c.onDurationMin + frand() * (c.onDurationMax - c.onDurationMin) : 0.0;
		qreal offDuration = c.onOff ? c.offDurationMin + frand() * (c.offDurationMax - c.offDurationMin) : 0.0;

		struct ev_loop *loop = static_cast<struct ev_loop*>(c.ev_loop);
        {
            ev_timer *timerOn = (ev_timer *)malloc(sizeof(ev_timer));
			onOffTimer2Connection[timerOn] = c.index;
			ev_timer_init(timerOn, timeout_start_connection, firstStart, c.onOff ? onDuration + offDuration : 0.0);
			ev_timer_start(loop, timerOn);
        }
		if (c.onOff) {
            ev_timer *timerOff = (ev_timer *)malloc(sizeof(ev_timer));
			onOffTimer2Connection[timerOff] = c.index;
			ev_timer_init(timerOff, timeout_stop_connection, firstStart + onDuration, onDuration + offDuration);
            ev_timer_start(loop, timerOff);
        }
    }
}

void poissonStartConnectionTimeoutHandler(int revents, void *arg)
{
	qDebugT();
	Q_UNUSED(revents);
	Q_ASSERT_FORCE(arg != NULL);
	NetGraphConnection &c = *static_cast<NetGraphConnection *>(arg);
	startConnection(c);
}

void connectionTransferCompletedHandler(void *arg)
{
	Q_ASSERT_FORCE(arg != NULL);
	qDebugT() << "connectionTransferCompletedHandler";
	NetGraphConnection &c = *static_cast<NetGraphConnection *>(arg);
	connectionTransferCompleted(c);
}

bool stopMaster = false;
void masterSignalCallback(int ) {
	stopMaster = true;
}

void runMaster(int argc, char **argv)
{
	signal(SIGINT, masterSignalCallback);
	signal(SIGTERM, masterSignalCallback);

	long numCpu = sysconf(_SC_NPROCESSORS_ONLN) / 2;
	qDebug() << "Spawning" << numCpu << "processes...";

	QVector<QProcess*> children;
	for (int iChild = 0; iChild < numCpu; iChild++) {
		QProcess *child = new QProcess();
		QStringList args;
		for (int i = 0; i < argc; i++) {
			args << argv[i];
		}
		args << "--child" << QString::number(iChild) << QString::number(numCpu);

		child->setProcessChannelMode(QProcess::MergedChannels);
        child->start("line-traffic", args);

		children << child;
	}

	while (!stopMaster) {
		sleep(1);
	}

	for (int iChild = 0; iChild < numCpu; iChild++) {
		kill(children[iChild]->pid(), 2); // SIGINT
	}

	for (int iChild = 0; iChild < numCpu; iChild++) {
		children[iChild]->waitForFinished();
		qDebug() << children[iChild]->readAll();
	}
}

int main(int argc, char **argv)
{
	unsigned int seed = clock() ^ time(NULL) ^ getpid();
	qDebug() << "seed:" << seed;
	srand(seed);

	argc--, argv++;

	if (argc < 1) {
		fprintf(stderr, "Wrong args\n");
		exit(-1);
	}

	if (QString(argv[0]) == "--master") {
		argc--, argv++;
		runMaster(argc, argv);
		exit(0);
	}

	QString netgraphFileName = argv[0];
	argc--, argv++;

	int workerIndex = 0;
	int numWorkers = 1;

	while (argc > 0) {
		QString arg = argv[0];
		argc--, argv++;
		if (arg == "--child" && argc == 2) {
			workerIndex = QString(argv[0]).toInt();
			argc--, argv++;
			numWorkers = QString(argv[0]).toInt();
			argc--, argv++;
		} else {
			fprintf(stderr, "Wrong args\n");
			exit(-1);
		}
	}

	netGraph.setFileName(netgraphFileName);
	if (!netGraph.loadFromFile()) {
		fprintf(stderr, "Could not read graph\n");
		exit(-1);
	}

	// Multiply connections
    netGraph.flattenConnections();

	// Assign ports
	assignPorts();

	for (int c = 0; c < netGraph.connections.count(); c++) {
		netGraph.connections[c].maskedOut = c % numWorkers != workerIndex;
	}
	QStringList allInterfaceIPs = getAllInterfaceIPs();
	for (int n = 0; n < netGraph.nodes.count(); n++) {
		netGraph.nodes[n].maskedOut = !allInterfaceIPs.contains(netGraph.nodes[n].ip());
	}

	// important to ignore SIGPIPE....who designed read/write this way?!
	signal(SIGPIPE, SIG_IGN);

	struct ev_loop *loop = ev_default_loop(0);
    QString backendName;
    int backendCode = ev_backend(loop);
    if (backendCode == EVBACKEND_SELECT) {
        backendName = "SELECT";
    } else if (backendCode == EVBACKEND_POLL) {
        backendName = "POLL";
    } else if (backendCode == EVBACKEND_EPOLL) {
        backendName = "EPOLL";
    } else if (backendCode == EVBACKEND_KQUEUE) {
        backendName = "KQUEUE";
    } else if (backendCode == EVBACKEND_DEVPOLL) {
        backendName = "DEVPOLL";
    } else if (backendCode == EVBACKEND_PORT) {
        backendName = "PORT";
    } else {
        backendName = QString("0x%1").arg(QString::number(backendCode, 16).toUpper());
    }
    fprintf(stderr, "Event loop backend: %s\n", backendName.toLatin1().data());

	for (int c = 0; c < netGraph.connections.count(); c++) {
		if (!netGraph.connections[c].maskedOut) {
			netGraph.connections[c].ev_loop = loop;
		}
	}

	struct ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, sigint_cb, SIGINT);
	ev_signal_start(loop, &signal_watcher);

	fprintf(stderr, "Connection count: %d\n", netGraph.connections.count());

	// Create servers
	foreach (NetGraphConnection c, netGraph.connections) {
		if (c.maskedOut)
			continue;
		if (netGraph.nodes[c.dest].maskedOut)
			continue;
		initConnection(loop, netGraph.connections[c.index]);
	}

	// Generate on-off events
	createOnOffTimers();

	// Start infinite loop
	ev_loop(loop, 0);

	tcp_close_all();
	udp_close_all();

	return 0;

//  int port = 8000;
//	tcp_server(loop, "127.0.0.1", port, TCPSink::makeTCPSink);
//	tcp_client(loop, "127.0.0.1", "127.0.0.1", port, 0, TCPSource::makeTCPSource);
//	port++;

//	tcp_server(loop, "127.0.0.1", port, TCPSink::makeTCPSink);
//	tcp_client(loop, "127.0.0.1", "127.0.0.1", port, 0, TCPParetoSource::makeTCPParetoSource);
//	port++;

//    udp_server(loop, "127.0.0.1", port, UDPSink::makeUDPSink);
//	udp_client(loop, "127.0.0.1", "127.0.0.1", port, 0, UDPCBRSource::makeUDPCBRSource, new UDPCBRSourceArg(128000));
//    port++;

//	udp_server(loop, "127.0.0.1", port, UDPVBRSink::makeUDPVBRSink);
//	udp_client(loop, "127.0.0.1", "127.0.0.1", port, 0, UDPVBRSource::makeUDPVBRSource, new UDPVBRSourceArg(10000));
//	port++;
}
