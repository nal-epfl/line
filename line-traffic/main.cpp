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
#include <QHostAddress>

#include "netgraph.h"
#include "evtcp.h"
#include "evpid.h"
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
#include "gitinfo.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

NetGraph netGraph;
QHash<ev_timer *, int> onOffTimer2Connection;

void poissonStartConnectionTimeoutHandler(int revents, void *arg);
void connectionTransferCompletedHandler(void *arg, int client_fd);

// Starts servers (sinks).
void startConnectionServer(struct ev_loop *loop, NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.dest].maskedOut)
		return;

	qDebugT() << "Worker handles server socket for connection"
			  << netGraph.nodes[c.source].ip() << netGraph.nodes[c.dest].ip()
			  << c.basicType;

	if (c.implementation == NetGraphConnection::Libev) {
		if (c.basicType == "TCP" || c.basicType == "TCPx") {
			for (int i = 0; i < c.multiplier; i++) {
				int port = netGraph.connections[c.index].ports[i];
				c.serverFDs.insert(tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								   TCPSink::makeTCPSink, NULL, c.tcpReceiveWindowSize, c.tcpCongestionControl));
			}
		} else if (c.basicType == "TCP-Poisson-Pareto") {
			for (int i = 0; i < c.multiplier; i++) {
				int port = netGraph.connections[c.index].ports[i];
				c.serverFDs.insert(tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								   TCPSink::makeTCPSink, new TCPSinkArg(true), c.tcpReceiveWindowSize, c.tcpCongestionControl));
				// The first transmission is delayed exponentially
				c.delayStart = true;
			}
		} else if (c.basicType == "TCP-DASH") {
			for (int i = 0; i < c.multiplier; i++) {
				int port = netGraph.connections[c.index].ports[i];
				c.serverFDs.insert(tcp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								   TCPSink::makeTCPSink, new TCPSinkArg(true), c.tcpReceiveWindowSize, c.tcpCongestionControl));
			}
		} else if (c.basicType == "UDP-CBR") {
			for (int i = 0; i < c.multiplier; i++) {
				int port = netGraph.connections[c.index].ports[i];
				c.serverFDs.insert(udp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								   UDPSink::makeUDPSink));
			}
		} else if (c.basicType == "UDP-VBR") {
			for (int i = 0; i < c.multiplier; i++) {
				int port = netGraph.connections[c.index].ports[i];
				c.serverFDs.insert(udp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								   UDPVBRSink::makeUDPVBRSink));
			}
		} else if (c.basicType == "UDP-VCBR") {
			for (int i = 0; i < c.multiplier; i++) {
				int port = netGraph.connections[c.index].ports[i];
				c.serverFDs.insert(udp_server(loop, qPrintable(netGraph.nodes[c.dest].ip()), port,
								   UDPSink::makeUDPSink));
			}
		} else {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
			Q_ASSERT_FORCE(false);
		}
	} else if (c.implementation == NetGraphConnection::Iperf2) {
		if (c.basicType == "TCP-Poisson-Pareto") {
			int port = netGraph.connections[c.index].ports[0];

			c.serverFDs.insert(create_process(loop,
											  QString("iperf --server --port %1 --bind %2 %3")
											  .arg(port)
											  .arg(netGraph.nodes[c.dest].ip())
											  .arg(c.tcpReceiveWindowSize <= 0 ? QString() : QString("--window %1").arg(c.tcpReceiveWindowSize))
											  .toLatin1().constData()));
			// The first transmission is delayed exponentially
			c.delayStart = true;
		} else {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
			Q_ASSERT_FORCE(false);
		}
	} else if (c.implementation == NetGraphConnection::Iperf3) {
		if (c.basicType == "TCP-Poisson-Pareto") {
			int port = netGraph.connections[c.index].ports[0];

			c.serverFDs.insert(create_process(loop,
											  QString("iperf3 --server --port %1 --bind %2 %3")
											  .arg(port)
											  .arg(netGraph.nodes[c.dest].ip())
											  .arg(c.tcpReceiveWindowSize <= 0 ? QString() : QString("--window %1").arg(c.tcpReceiveWindowSize))
											  .toLatin1().constData()));
			// The first transmission is delayed exponentially
			c.delayStart = true;
		} else {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
			Q_ASSERT_FORCE(false);
		}
	} else {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
		Q_ASSERT_FORCE(false);
	}
}

// Starts clients (sources).
void startConnectionClient(NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.source].maskedOut)
		return;

	qDebugT() << "Worker handles client socket for connection"
			  << netGraph.nodes[c.source].ip() << netGraph.nodes[c.dest].ip()
			  << c.basicType;

	struct ev_loop *loop = static_cast<struct ev_loop *>(c.ev_loop);

	c.clientFDs.clear();

	if (c.implementation == NetGraphConnection::Libev) {
		for (int i = 0; i < c.multiplier; i++) {
			if (c.basicType == "TCP") {
				int port = netGraph.connections[c.index].ports[i];
				c.clientFDs.insert(tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
								   qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
						c.trafficClass, TCPSource::makeTCPSource, NULL, c.tcpReceiveWindowSize, c.tcpCongestionControl));
			} else if (c.basicType == "TCPx") {
				int port = netGraph.connections[c.index].ports[i];
				c.clientFDs.insert(tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
								   qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
						c.trafficClass, TCPSource::makeTCPSource, NULL, c.tcpReceiveWindowSize, c.tcpCongestionControl));
			} else if (c.basicType == "TCP-Poisson-Pareto") {
				TCPParetoSourceArg paretoParams;
				paretoParams.alpha = c.paretoAlpha;
				paretoParams.scale = c.paretoScale_b / 8.0 / qreal(c.multiplier);

				if (!c.delayStart) {
					qDebugT() << "Creating Poisson connection";
					int port = netGraph.connections[c.index].ports[i];
					c.clientFDs.insert(tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
									   qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
							c.trafficClass, TCPParetoSource::makeTCPParetoSource, &paretoParams, c.tcpReceiveWindowSize,
							c.tcpCongestionControl,
							c.sequential ? connectionTransferCompletedHandler : NULL,
							c.sequential ? &c : NULL));
				}
				if (c.delayStart || !c.sequential) {
					// schedule next start
					c.delayStart = false;
					qreal delay = -log(1.0 - frandex()) / c.poissonRate;
					qDebugT() << "Delaying Poisson connection" << delay;
					ev_once(loop, -1, 0, delay, poissonStartConnectionTimeoutHandler, &c);
				}
			} else if (c.basicType == "TCP-DASH") {
				TCPDashSourceArg params(c.rate_Mbps * 1.0e6 / 8.0 / qreal(c.multiplier),
										c.bufferingRate_Mbps * 1.0e6 / 8.0 / qreal(c.multiplier),
										c.bufferingTime_s,
										c.streamingPeriod_s);
				qDebugT() << "Creating DASH connection";
				int port = netGraph.connections[c.index].ports[i];
				c.clientFDs.insert(tcp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
								   qPrintable(netGraph.nodes[c.dest].ipForeign()), port,
						c.trafficClass, TCPDashSource::makeTCPDashSource, &params, c.tcpReceiveWindowSize,
						c.tcpCongestionControl));
			} else if (c.basicType == "UDP-CBR") {
				qreal rate_Bps = c.rate_Mbps * 1.0e6 / 8.0 / qreal(c.multiplier);
				int port = netGraph.connections[c.index].ports[i];
				UDPCBRSourceArg params(rate_Bps, 1400, c.poisson);
				c.clientFDs.insert(udp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
								   qPrintable(netGraph.nodes[c.dest].ipForeign()), port, c.trafficClass,
						UDPCBRSource::makeUDPCBRSource, &params));
			} else if (c.basicType == "UDP-VBR") {
				int port = netGraph.connections[c.index].ports[i];
				UDPVBRSourceArg params(1000);
				c.clientFDs.insert(udp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
								   qPrintable(netGraph.nodes[c.dest].ipForeign()), port, c.trafficClass,
						UDPVBRSource::makeUDPVBRSource, &params));
			} else if (c.basicType == "UDP-VCBR") {
				qreal rate_Bps = c.rate_Mbps * 1.0e6 / 8.0 / qreal(c.multiplier);
				int port = netGraph.connections[c.index].ports[i];
				UDPVCBRSourceArg params(rate_Bps);
				c.clientFDs.insert(udp_client(loop, qPrintable(netGraph.nodes[c.source].ip()),
								   qPrintable(netGraph.nodes[c.dest].ipForeign()), port, c.trafficClass,
						UDPVCBRSource::makeUDPVCBRSource, &params));
			} else {
				qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
				Q_ASSERT_FORCE(false);
			}
		}
	} else if (c.implementation == NetGraphConnection::Iperf2) {
		if (c.basicType == "TCP-Poisson-Pareto") {
			TCPParetoSourceArg paretoParams;
			paretoParams.alpha = c.paretoAlpha;
			paretoParams.scale = c.paretoScale_b / 8.0 / qreal(c.multiplier);

			// uniform is uniformly distributed in (0, 1]
			qreal uniform = 1.0 - frandex();
			quint64 transferSize = paretoParams.scale / pow(uniform, (1.0/paretoParams.alpha));
			qDebugT() << "Pareto transfer size (bytes): " << transferSize;

			if (!c.delayStart) {
				qDebugT() << "Creating Poisson connection";
				int port = netGraph.connections[c.index].ports[0];
				c.clientFDs.insert(create_process(loop,
												  QString("iperf --client %1 --port %2 --bind %3 --num %4 --parallel %5 --tos %6 %7")
												  .arg(netGraph.nodes[c.dest].ipForeign())
												  .arg(port)
												  .arg(netGraph.nodes[c.source].ip())
												  .arg(transferSize)
												  .arg(c.multiplier)
												  .arg(QString("%1").arg(c.trafficClass << 3, 2, 16, QLatin1Char('0')))
												  .arg(c.tcpCongestionControl.isEmpty() ? QString() : QString("--linux-congestion %1").arg(c.tcpCongestionControl))
												  .toLatin1().constData(),
												  c.sequential ? connectionTransferCompletedHandler : NULL,
												  c.sequential ? &c : NULL));
			}
			if (c.delayStart || !c.sequential) {
				// schedule next start
				c.delayStart = false;
				qreal delay = -log(1.0 - frandex()) / c.poissonRate;
				qDebugT() << "Delaying Poisson connection" << delay;
				ev_once(loop, -1, 0, delay, poissonStartConnectionTimeoutHandler, &c);
			}
		} else {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
			Q_ASSERT_FORCE(false);
		}
	} else if (c.implementation == NetGraphConnection::Iperf3) {
		if (c.basicType == "TCP-Poisson-Pareto") {
			TCPParetoSourceArg paretoParams;
			paretoParams.alpha = c.paretoAlpha;
			paretoParams.scale = c.paretoScale_b / 8.0 / qreal(c.multiplier);

			// uniform is uniformly distributed in (0, 1]
			qreal uniform = 1.0 - frandex();
			quint64 transferSize = paretoParams.scale / pow(uniform, (1.0/paretoParams.alpha));
			qDebugT() << "Pareto transfer size (bytes): " << transferSize;

			if (!c.delayStart) {
				qDebugT() << "Creating Poisson connection";
				int port = netGraph.connections[c.index].ports[0];
				c.clientFDs.insert(create_process(loop,
												  QString("iperf3 --client %1 --port %2 --bind %3 --bytes %4 --parallel %5 --tos %6 %7")
												  .arg(netGraph.nodes[c.dest].ipForeign())
												  .arg(port)
												  .arg(netGraph.nodes[c.source].ip())
												  .arg(transferSize)
												  .arg(c.multiplier)
												  .arg(QString("%1").arg(c.trafficClass << 3, 2, 16, QLatin1Char('0')))
												  .arg(c.tcpCongestionControl.isEmpty() ? QString() : QString("--linux-congestion %1").arg(c.tcpCongestionControl))
												  .toLatin1().constData(),
												  c.sequential ? connectionTransferCompletedHandler : NULL,
												  c.sequential ? &c : NULL));
			}
			if (c.delayStart || !c.sequential) {
				// schedule next start
				c.delayStart = false;
				qreal delay = -log(1.0 - frandex()) / c.poissonRate;
				qDebugT() << "Delaying Poisson connection" << delay;
				ev_once(loop, -1, 0, delay, poissonStartConnectionTimeoutHandler, &c);
			}
		} else {
			qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
			Q_ASSERT_FORCE(false);
		}
	} else {
		qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "Could not parse parameters" << c.encodedType;
		Q_ASSERT_FORCE(false);
	}
}

// Stops clients (sources)
void stopConnectionClient(NetGraphConnection &c)
{
	qDebugT();
	if (c.maskedOut)
		return;
	if (netGraph.nodes[c.source].maskedOut)
		return;

	if (c.basicType == "TCP" || c.basicType == "TCPx") {
		foreach (int fd, c.clientFDs) {
			tcp_close_client(fd);
        }
	} else if (c.basicType == "UDP-CBR" || c.basicType == "UDP-VBR" || c.basicType == "UDP-VCBR") {
		foreach (int fd, c.clientFDs) {
			udp_close_client(fd);
		}
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
				-1,
				0,
				0,
				poissonStartConnectionTimeoutHandler, &netGraph.connections[c.index]);
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
		startConnectionClient(netGraph.connections[onOffTimer2Connection[w]]);
    }
}

void timeout_stop_connection(struct ev_loop *loop, ev_timer *w, int revents)
{
	qDebugT();
    Q_UNUSED(loop);
    Q_UNUSED(revents);
	if (onOffTimer2Connection.contains(w)) {
		stopConnectionClient(netGraph.connections[onOffTimer2Connection[w]]);
    }
}

void createOnOffTimers()
{
	qDebugT();
    foreach (NetGraphConnection c, netGraph.connections) {
		if (c.maskedOut)
			continue;
		if (netGraph.nodes[c.source].maskedOut)
			continue;

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
	startConnectionClient(c);
}

void connectionTransferCompletedHandler(void *arg, int client_fd)
{
	Q_ASSERT_FORCE(arg != NULL);
	qDebugT() << "connectionTransferCompletedHandler";
	NetGraphConnection &c = *static_cast<NetGraphConnection *>(arg);
	c.clientFDs.remove(client_fd);
	if (c.clientFDs.isEmpty())
		connectionTransferCompleted(c);
}

bool start = false;
bool stop = false;
bool stopMaster = false;
void masterSignalCallback(int s) {
	if (s == SIGUSR1) {
		start = true;
		return;
	}
	stopMaster = true;
}

void childSignalCallback(int s) {
	if (s == SIGUSR1) {
		start = true;
		return;
	} else if (s == SIGINT) {
		stop = true;
		return;
	}
}

void sigint_cb(struct ev_loop *loop, struct ev_signal *w, int revents)
{
	Q_UNUSED(w);
	Q_UNUSED(revents);
	stop = true;
	fprintf(stderr, "\nKeyboard interrupt: closing all connections...\n\n");
	tcp_close_all();
	udp_close_all();

	ev_unloop(loop, EVUNLOOP_ALL);
}

void runMaster(QString program, int argc, char **argv)
{
	signal(SIGINT, masterSignalCallback);
	signal(SIGTERM, masterSignalCallback);
	signal(SIGUSR1, masterSignalCallback);

	long numCpu = qMax(1L, sysconf(_SC_NPROCESSORS_ONLN) / 2);
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
        child->start(program, args);

		children << child;
	}

	while (!stopMaster) {
		if (start) {
			for (int iChild = 0; iChild < numCpu; iChild++) {
				kill(children[iChild]->pid(), SIGUSR1);
			}
		}
		sleep(1);
	}

	for (int iChild = 0; iChild < numCpu; iChild++) {
		kill(children[iChild]->pid(), SIGINT);
	}

	for (int iChild = 0; iChild < numCpu; iChild++) {
		children[iChild]->waitForFinished();
		qDebug() << children[iChild]->readAll();
		int exitCode = children[iChild]->exitCode();
		if (exitCode) {
			qDebug() << "Worker" << iChild << "crashed with exit code" << exitCode;
		}
	}
}

bool isValidIPv4Address(QString s)
{
	QHostAddress address;
	if (!address.setAddress(s) || address.protocol() != QAbstractSocket::IPv4Protocol) {
		return false;
	}
	return true;
}

void runWithTextConfig(QString program, int argc, char **argv)
{
	if (argc != 1) {
		qDebug() << __FILE__ << __LINE__ << "wrong args, expecting file name";
		exit(-1);
	}
	QString configFileName(argv[0]);
	QFile configFile(configFileName);
	argc--, argv++;
	if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << __FILE__ << __LINE__ << "Cannot open file name:" << configFile.fileName();
		exit(-1);
	}

	NetGraph g;
	g.setFileName(QString("/tmp/%1.graph").arg(QCoreApplication::applicationPid()));

	QTextStream stream(&configFile);
	int lineNumber = 0;
	QHash<QString, int> ip2node;
	QStringList allInterfaceIPs = getAllInterfaceIPs();
	while (true) {
		lineNumber++;
		QString line = stream.readLine();
		if (line.isNull())
			break;
		if (line.isEmpty() || line.startsWith("#"))
			continue;
		QStringList tokens = line.split(' ', QString::SkipEmptyParts);
		if (tokens.isEmpty())
			continue;

		QString cmd = tokens.takeFirst();
		if (cmd == "port") {
			if (tokens.isEmpty()) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: missing port";
				exit(-1);
			}
			QString token = tokens.takeFirst().trimmed();
			bool ok;
			quint16 port = token.toUShort(&ok);
			if (!ok) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: bad port";
				exit(-1);
			}
			g.setBasePort(port);
		} else if (cmd == "connection") {
			if (tokens.isEmpty()) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: missing first ip";
				exit(-1);
			}
			QString ip1 = tokens.takeFirst().trimmed();
			if (!isValidIPv4Address(ip1))
				ip1 = resolveDNSName(ip1);
			if (!isValidIPv4Address(ip1)) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: bad first ip";
				exit(-1);
			}
			if (tokens.isEmpty()) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: missing second ip";
				exit(-1);
			}
			QString ip2 = tokens.takeFirst().trimmed();
			if (!isValidIPv4Address(ip2))
				ip2 = resolveDNSName(ip2);
			if (!isValidIPv4Address(ip2)) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: bad second ip";
				exit(-1);
			}

			if (!ip2node.contains(ip1)) {
				int n = g.addNode(NETGRAPH_NODE_HOST);
				g.nodes[n].customIp = g.nodes[n].customIpForeign = ip1;
				ip2node[ip1] = n;
			}

			if (!ip2node.contains(ip2)) {
				int n = g.addNode(NETGRAPH_NODE_HOST);
				g.nodes[n].customIp = g.nodes[n].customIpForeign = ip2;
				ip2node[ip2] = n;
			}

			if (tokens.isEmpty()) {
				qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: missing connection params";
				exit(-1);
			}

			QStringList hops;
			while (tokens.first() == "via") {
				tokens.removeFirst();
				QString hop = tokens.takeFirst().trimmed();
				if (!isValidIPv4Address(hop))
					hop = resolveDNSName(hop);
				if (!isValidIPv4Address(hop)) {
					qDebug() << __FILE__ << __LINE__ << "Cannot parse config file, line" << lineNumber << line << "reason: bad hop";
					exit(-1);
				}
				hops << hop;

				if (!ip2node.contains(hop)) {
					int n = g.addNode(NETGRAPH_NODE_HOST);
					g.nodes[n].customIp = g.nodes[n].customIpForeign = hop;
					ip2node[hop] = n;
				}
			}

			if (!hops.isEmpty()) {
				if (allInterfaceIPs.contains(ip1) &&
					!allInterfaceIPs.contains(ip2)) {
					ip2 = hops.first();
				} else if (!allInterfaceIPs.contains(ip1) &&
						   allInterfaceIPs.contains(ip2)) {
					ip1 = hops.last();
				}
			}

			g.addConnection(NetGraphConnection(ip2node[ip1], ip2node[ip2], tokens.join(" "), QByteArray()));
		}
	}

	if (!g.saveToFile()) {
		qDebug() << __FILE__ << __LINE__ << "Cannot save file:" << g.fileName;
		exit(-1);
	}

	char **argv2 = new char*[1];
	argv2[0] = strdup(g.fileName.toLatin1().data());
	runMaster(program, 1, argv2);
	free(argv2[0]);
	delete [] argv2;
	exit(0);
}

int main(int argc, char **argv)
{
	QCoreApplication a(argc, argv);

	unsigned int seed = clock() ^ time(NULL) ^ getpid();
	qDebug() << "seed:" << seed;
	srand(seed);

	if (!a.arguments().contains("--text-config") &&
		a.arguments().contains("--master"))
		showGitInfo();

	QString program(argv[0]);

	argc--, argv++;

	if (argc < 1) {
		fprintf(stderr, "Wrong args\n");
		exit(-1);
	}

	if (QString(argv[0]) == "--text-config") {
		argc--, argv++;
		runWithTextConfig(program, argc, argv);
		exit(0);
	}

	if (QString(argv[0]) == "--master") {
		argc--, argv++;
		runMaster(program, argc, argv);
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

	// Assign ports
	netGraph.assignPorts();

	for (int c = 0; c < netGraph.connections.count(); c++) {
		netGraph.connections[c].maskedOut = c % numWorkers != workerIndex;
	}
	QStringList allInterfaceIPs = getAllInterfaceIPs();
	for (int n = 0; n < netGraph.nodes.count(); n++) {
		netGraph.nodes[n].maskedOut = !allInterfaceIPs.contains(netGraph.nodes[n].ip());
	}

	// important to ignore SIGPIPE....who designed read/write this way?!
	signal(SIGPIPE, SIG_IGN);

	signal(SIGUSR1, childSignalCallback);

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

	fprintf(stderr, "Connection count: %d\n", netGraph.connections.count());

	// Create servers
	foreach (NetGraphConnection c, netGraph.connections) {
		if (c.maskedOut)
			continue;
		if (netGraph.nodes[c.dest].maskedOut)
			continue;
		startConnectionServer(loop, netGraph.connections[c.index]);
	}

	signal(SIGINT, childSignalCallback);

	while (!start && !stop) {
		usleep(1000);
	}

	// Generate on-off events
	createOnOffTimers();

	struct ev_signal signal_watcher;
	ev_signal_init(&signal_watcher, sigint_cb, SIGINT);
	ev_signal_start(loop, &signal_watcher);

	// Start infinite loop
	if (!stop) {
		ev_loop(loop, 0);
	}

	tcp_close_all();
	udp_close_all();
	process_close_all();

	return 0;
}
