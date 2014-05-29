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

#include "deploy.h"

#include "../remote_config.h"
#include "util.h"

void generateHostDeploymentScript(NetGraph &g, bool realRouting);

bool deploy(QString paramsFileName) {
	RunParams runParams;
	if (!loadRunParams(runParams, paramsFileName)) {
		return false;
	}

	NetGraph g;
	g.setFileName(QDir(".").absolutePath() + "/" + runParams.graphName + ".graph");
	g.loadFromByteArray(qUncompress(runParams.graphSerializedCompressed));

	if (runParams.realRouting) {
		QList<QString> customIPs = QList<QString>()
								   << "192.168.20.2"
								   << "192.168.40.2"
								   << "192.168.21.2"
								   << "192.168.41.2"
								   << "192.168.22.2"
								   << "192.168.42.2"
								   << "192.168.23.2"
								   << "192.168.43.2";
		int index = 0;
		for (int i = 0; i < g.nodes.count(); i++) {
			if (g.nodes[i].nodeType == NETGRAPH_NODE_HOST) {
				g.nodes[i].customIp = customIPs[index % customIPs.count()];
				g.nodes[i].customIpForeign = g.nodes[i].customIp;
				index++;
			}
		}
	}

	if (!g.saveToFile()) {
		return false;
	}

	if (!deploy(g, runParams)) {
		return false;
	}

	return true;
}

bool deploy(NetGraph &g, const RunParams &params) {
	QString description;
	QString command;
	QStringList args;
	QString graphName = g.fileName;
	QString core = params.coreHostName;
	QString corePort = params.corePort;
	QString client = params.clientHostName;
	QString clientPort = params.clientPort;
	QList<QString> clients = params.realRouting ? params.clientHostNames : (QList<QString>() << client);

	qDebug() << "Computing routes...";
	g.computeRoutes();

	if (params.readjustQueues) {
		qDebug() << "Readjusting queues...";
		if (params.bufferSize == "large") {
			qDebug() << "Using large buffers.";
			if (!g.readjustQueues()) {
				qError() << "Could not readjust all queues!";
			}
		} else if (params.bufferSize == "small") {
			qDebug() << "Using small buffers.";
			if (!g.readjustQueuesSmall()) {
				qError() << "Could not readjust all queues!";
			}
		} else {
			qError() << "Could not readjust queues!";
			return false;
		}
	}

    if (params.sampleAllTimelines) {
		g.recordTimelinesEverywhere(params.intervalSize);
    }

	if (params.setFixedTcpReceiveWindowSize) {
		qDebug() << QString("Setting the TCP receive window size to %1 for all connections...").arg(params.fixedTcpReceiveWindowSize);
		foreach (NetGraphConnection c, g.connections) {
			g.connections[c.index].tcpReceiveWindowSize = params.fixedTcpReceiveWindowSize;
		}
	} else if (params.setAutoTcpReceiveWindowSize) {
		qDebug() << QString("Setting the TCP receive window size to auto (scaled by %1) for all connections...").arg(params.scaleAutoTcpReceiveWindowSize);
		foreach (NetGraphConnection c, g.connections) {
			qint64 tcpReceiveWindowSize = 0;
			bool ok1, ok2;
			NetGraphPath pFwd = g.pathByNodeIndexTry(c.source, c.dest, ok1);
			NetGraphPath pRev = g.pathByNodeIndexTry(c.dest, c.source, ok2);
			if (ok1 && !pFwd.edgeList.isEmpty() &&
				ok2 && !pRev.edgeList.isEmpty()) {
				qreal bwBottleneck = 1.0e9;
				qreal rtt = 0;
				foreach (NetGraphEdge e, pFwd.edgeList) {
					qreal bw = e.bandwidth;
					qreal q = e.queueLength * 1500;
					qreal tq = q / bw;
					qreal tp = e.delay_ms;
					qreal t = tq + tp;
					rtt += t;
					bwBottleneck = qMin(bwBottleneck, bw);
				}
				foreach (NetGraphEdge e, pRev.edgeList) {
					qreal bw = e.bandwidth;
					qreal q = e.queueLength * 1500;
					qreal tq = q / bw;
					qreal tp = e.delay_ms;
					qreal t = tq + tp;
					rtt += t;
				}
				tcpReceiveWindowSize = qMax(2048.0,
											qMin((2LL << 31) - 1.0,
												 ceil(bwBottleneck * rtt * params.scaleAutoTcpReceiveWindowSize)));
			}
			qDebug() << QString("Setting the TCP receive window size to %1 B for connection %2 -> %3...")
								.arg(tcpReceiveWindowSize).arg(c.source).arg(c.dest);
			g.connections[c.index].tcpReceiveWindowSize = tcpReceiveWindowSize;
		}
	} else {
		qDebug() << QString("Setting the TCP receive window size to default for all connections...");
		foreach (NetGraphConnection c, g.connections) {
			g.connections[c.index].tcpReceiveWindowSize = 0;
		}
	}

	qDebug() << "Saving...";
	if (!g.saveToFile())
		return false;

	if (params.fakeEmulation) {
		qDebug() << "Skipped the rest of deployment because this is a fake emulation.";
		return true;
	}

	generateHostDeploymentScript(g, params.realRouting);

	if (!params.realRouting) {
		description = "Uploading the graph file to the router emulator";
		command = "scp";
		args = QStringList() << "-P" << corePort <<
								graphName << QString("root@%1:~").arg(core);
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}
	}

	if (params.realRouting) {
		foreach (QString host, QStringList() << clients << params.routerHostNames) {
			description = "Turning off network card offloading";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(host) <<
									"-p" << clientPort <<
									"sh -c 'for i in eth0 eth1 eth2 eth3 eth5 eth6 eth7 ; "
									"do "
									"  for f in rx tx sg tso ufo gso gro lro rxvlan txvlan ntuple rxhash ; "
									"  do "
									"    ethtool --offload $i $f off ; "
									"    ethtool --pause $i rx off tx off ; "
									"  done ; "
									"done'";
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		}
		const int nonNeutralEdge = 0;
		if (g.edges[nonNeutralEdge].policerCount == 2) {
			QString scriptQoS =
					QString("tc qdisc del dev eth6 root; "
							" "
							"tc qdisc add dev eth6 root handle 1: htb default 0 r2q 100; "
							"tc class add dev eth6 parent 1: classid 1:0 htb prio 0 rate %1kBps burst %2B; "
							" "
							"tc filter add dev eth6 protocol ip prio 0 "
							"  u32 match ip src 192.168.20.2/32 match ip dst 192.168.40.2/32 "
							"  police rate %3kBps buffer %2B drop "
							"  classid 1:0; "
							"tc filter add dev eth6 protocol ip prio 0 "
							"  u32 match ip src 192.168.21.2/32 match ip dst 192.168.41.2/32 "
							"  police rate %3kBps buffer %2B drop "
							"  classid 1:0; "
							"tc filter add dev eth6 protocol ip prio 0 "
							"  u32 match ip src 192.168.22.2/32 match ip dst 192.168.42.2/32 "
							"  police rate %4kBps buffer %2B drop "
							"  classid 1:0; "
							"tc filter add dev eth6 protocol ip prio 0 "
							"  u32 match ip src 192.168.23.2/32 match ip dst 192.168.43.2/32 "
							"  police rate %4kBps buffer %2B drop "
							"  classid 1:0; "
							" "
							"ip link set eth6 txqueuelen %5; "
							" "
							"tc qdisc show dev eth6; "
							"tc filter show dev eth6")
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth))
					.arg(qint64(g.edges[nonNeutralEdge].queueLength * ETH_FRAME_LEN * params.bufferBloatFactor))
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth * g.edges[nonNeutralEdge].policerWeights[0]))
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth * g.edges[nonNeutralEdge].policerWeights[1]))
					.arg(qint64(g.edges[nonNeutralEdge].queueLength * params.bufferBloatFactor));

			description = "Configuring traffic policing";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(params.routerHostNames[0]) <<
									"-p" << corePort <<
									QString("sh -c '%1'").arg(scriptQoS);
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		} else if (g.edges[nonNeutralEdge].queueCount == 2) {
			QString scriptQoS =
					QString("tc qdisc del dev eth6 root; "
							" "
							"tc qdisc add dev eth6 root handle 1: htb default 1 r2q 100; "
							"tc class add dev eth6 parent 1: classid 1:1 htb prio 0 rate %2kBps ceil %1kBps burst %4B; "
							"tc class add dev eth6 parent 1: classid 1:2 htb prio 0 rate %3kBps ceil %3kBps burst %4B; "
							" "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.20.2/32 match ip dst 192.168.40.2/32 flowid 1:1; "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.21.2/32 match ip dst 192.168.41.2/32 flowid 1:1; "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.22.2/32 match ip dst 192.168.42.2/32 flowid 1:2; "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.23.2/32 match ip dst 192.168.43.2/32 flowid 1:2; "
							" "
							"ip link set eth6 txqueuelen %5; "
							" "
							"tc qdisc show dev eth6; "
							"tc filter show dev eth6")
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth))
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth * g.edges[nonNeutralEdge].queueWeights[0]))
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth * g.edges[nonNeutralEdge].queueWeights[1]))
					.arg(qint64(g.edges[nonNeutralEdge].queueLength * ETH_FRAME_LEN * params.bufferBloatFactor))
					.arg(qint64(g.edges[nonNeutralEdge].queueLength * params.bufferBloatFactor));

			description = "Configuring traffic shaping";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(params.routerHostNames[0]) <<
									"-p" << corePort <<
									QString("sh -c '%1'").arg(scriptQoS);
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		} else if (g.edges[nonNeutralEdge].policerCount == 1 &&
				   g.edges[nonNeutralEdge].queueCount == 1) {
			QString scriptQoS =
					QString("tc qdisc del dev eth6 root; "
							" "
							"tc qdisc add dev eth6 root handle 1: htb default 1 r2q 100; "
							"tc class add dev eth6 parent 1: classid 1:1 htb prio 0 rate %2kBps ceil %1kBps burst %B; "
							"tc class add dev eth6 parent 1: classid 1:2 htb prio 0 rate %3kBps ceil %3kBps burst %B; "
							" "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.20.2/32 match ip dst 192.168.40.2/32 "
							"  flowid 1:1; "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.21.2/32 match ip dst 192.168.41.2/32 "
							"  flowid 1:1; "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.22.2/32 match ip dst 192.168.42.2/32 "
							"  flowid 1:2; "
							"tc filter add dev eth6 protocol ip prio 1 "
							"  u32 match ip src 192.168.23.2/32 match ip dst 192.168.43.2/32 "
							"  flowid 1:2; "
							" "
							"ip link set eth6 txqueuelen %5; "
							" "
							"tc qdisc show dev eth6; "
							"tc filter show dev eth6")
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth))
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth * g.edges[nonNeutralEdge].queueWeights[0]))
					.arg(qint64(g.edges[nonNeutralEdge].bandwidth * g.edges[nonNeutralEdge].queueWeights[0]))
					.arg(qint64(g.edges[nonNeutralEdge].queueLength * ETH_FRAME_LEN * params.bufferBloatFactor))
					.arg(qint64(g.edges[nonNeutralEdge].queueLength * params.bufferBloatFactor));

			description = "Configuring traffic shaping (neutral shaping)";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(params.routerHostNames[0]) <<
									"-p" << corePort <<
									QString("sh -c '%1'").arg(scriptQoS);
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		} else {
			qError() << "Could not configure real routing experiment!";
			return false;
		}

		{
			// Delays
			QList<qreal> rtts;
			rtts << 2 << 2 << 2	<< 2;
			for (int i = 0; i < 4; i++) {
				qint32 src, dest;
				if (i == 0) {
					src = 2;
					dest = 3;
				} else if (i == 1) {
					src = 4;
					dest = 5;
				} else if (i == 2) {
					src = 6;
					dest = 7;
				} else if (i == 3) {
					src = 8;
					dest = 9;
				}
				bool ok1, ok2;
				NetGraphPath pFwd = g.pathByNodeIndexTry(src, dest, ok1);
				NetGraphPath pRev = g.pathByNodeIndexTry(dest, src, ok2);
				if (ok1 && !pFwd.edgeList.isEmpty() &&
					ok2 && !pRev.edgeList.isEmpty()) {
					qreal rtt = 0;
					foreach (NetGraphEdge e, pFwd.edgeList) {
						rtt += e.delay_ms;
					}
					foreach (NetGraphEdge e, pRev.edgeList) {
						rtt += e.delay_ms;
					}
					rtts[i] = rtt;
				}
			}
			qreal minHalfRtt = rtts[0] / 2;
			foreach (qreal rtt, rtts) {
				minHalfRtt = qMin(minHalfRtt, rtt / 2);
			}

			QString scriptDelay =
					QString("tc qdisc del dev eth6 root; "
							"tc qdisc del dev eth0 root; "
							"tc qdisc del dev eth1 root; "
							"tc qdisc del dev eth2 root; "
							"tc qdisc del dev eth3 root; "
							" "
							"tc qdisc add dev eth6 root netem delay %1ms limit 100000; "
							"tc qdisc add dev eth0 root netem delay %2ms limit 100000; "
							"tc qdisc add dev eth1 root netem delay %3ms limit 100000; "
							"tc qdisc add dev eth2 root netem delay %4ms limit 100000; "
							"tc qdisc add dev eth3 root netem delay %5ms limit 100000")
					.arg(qint64(minHalfRtt))
					.arg(qint64(rtts[0] - minHalfRtt))
					.arg(qint64(rtts[1] - minHalfRtt))
					.arg(qint64(rtts[2] - minHalfRtt))
					.arg(qint64(rtts[3] - minHalfRtt));

			description = "Configuring delays";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(params.routerHostNames[1]) <<
									"-p" << corePort <<
									QString("sh -c '%1'").arg(scriptDelay);
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		}
	} else {
		foreach (QString host, QStringList() << params.clientHostName
											 << params.coreHostName) {
			description = "Turning off traffic control";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(host) <<
									"-p" << clientPort <<
                                    "sh -c 'for i in " REMOTE_DEDICATED_IF_ROUTER " ; "
									"do "
									"  tc qdisc del dev $i root; "
									"done'";
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		}
		foreach (QString host, QStringList() << params.clientHostName) {
			description = "Turning on network card offloading";
			command = "ssh";
			args = QStringList() << "-f" << QString("root@%1").arg(host) <<
									"-p" << clientPort <<
                                    "sh -c 'for i in " REMOTE_DEDICATED_IF_HOSTS "; "
									"do "
									"  for f in lro ntuple ; "
									"  do "
									"    ethtool --offload $i $f off ; "
									"    ethtool --pause $i rx off tx off ; "
									"  done ; "
									"  for f in rx tx sg tso ufo gso gro rxvlan txvlan rxhash ; "
									"  do "
									"    ethtool --offload $i $f on ; "
									"  done ; "
									"done'";
			if (!runCommand(command, args, description)) {
				qError() << "Deployment aborted.";
				return false;
			}
		}
	}

	foreach (QString host, clients) {
		description = "Uploading the graph file to the host emulator";
		command = "scp";
		args = QStringList() << "-P" << clientPort <<
								graphName << QString("root@%1:~").arg(host);
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}

		description = "Uploading the deployment script to the host emulator";
		command = "scp";
		args = QStringList() << "-P" << clientPort <<
								"deployhost.pl" << QString("root@%1:~").arg(host);
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}

		description = "Running the deployment script on the host emulator";
		command = "ssh";
		args = QStringList() << "-f" << QString("root@%1").arg(host) <<
								"-p" << clientPort <<
								"sh -c '(perl deployhost.pl 1> deploy.log 2> deploy.err &)'";
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}

		qDebug() << "Wait...";
		int fastSleeps = 5;
		while (1) {
			command = "ssh";
			args = QStringList() << QString("root@%1").arg(host) <<
								 "-p" << clientPort <<
								 "sh -c 'pstree | grep deployhost.pl || /bin/true'";
			description = "Waiting...";
			QString output;
			if (!runCommand(command, args, description, false, output)) {
				qError() << "Deployment aborted.";
				return false;
			}
			output = output.trimmed();
			if (output.isEmpty())
				break;
			if (fastSleeps > 0) {
				sleep(1);
				fastSleeps--;
			} else {
				sleep(5);
			}
		}

		description = "Showing the deploy log for the host emulator";
		command = "ssh";
		args = QStringList() << QString("root@%1").arg(host) <<
							 "-p" << clientPort <<
							 "sh -c 'cat deploy.log'";
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}

		sleep(5);

		description = "Checking for deploy errors for the host emulator";
		command = "ssh";
		args = QStringList() << QString("root@%1").arg(host) <<
								"-p" << clientPort <<
								"sh -c 'cat deploy.err'";
		QString output;
		if (!runCommand(command, args, description, false, output)) {
			qError() << "Deployment aborted.";
			return false;
		}
		output = output.trimmed();
		if (!output.isEmpty()) {
			qError() << output;
			qError() << "Deployment aborted.";
			return false;
		}

		description = "Uploading the clear script to the host emulator";
		command = "scp";
		args = QStringList() << "-P" << clientPort <<
							 "clearall.pl" << QString("root@%1:~").arg(host);
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}
	}

	if (!params.realRouting) {
		description = "Running the deployment script on the router emulator";
		command = "ssh";
		args = QStringList() << "-f" << QString("root@%1").arg(core) << "-p" << corePort << "sh -c '(/usr/bin/deploycore.pl 1> deploy.log 2> deploy.err &)'";
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}

		qDebug() << "Waiting...";
		int fastSleeps = 5;
		while (1) {
			command = "ssh";
			args = QStringList() << QString("root@%1").arg(core) << "-p" << corePort << "sh -c 'pstree | grep deploycore.pl || /bin/true'";
			description = "Waiting...";
			QString output;
			if (!runCommand(command, args, description, false, output)) {
				qError() << "Deployment aborted.";
				return false;
			}
			output = output.trimmed();
			if (output.isEmpty())
				break;
			if (fastSleeps > 0) {
				sleep(1);
				fastSleeps--;
			} else {
				sleep(5);
			}
		}

		description = "Showing the deploy log for the router emulator";
		command = "ssh";
		args = QStringList() << QString("root@%1").arg(core) << "-p" << corePort << "sh -c 'cat deploy.log'";
		if (!runCommand(command, args, description)) {
			qError() << "Deployment aborted.";
			return false;
		}

		description = "Checking for deploy errors for the router emulator";
		command = "ssh";
		{
			args = QStringList() << QString("root@%1").arg(core) << "-p" << corePort << "sh -c 'cat deploy.err'";
			QString output;
			if (!runCommand(command, args, description, false, output)) {
				qError() << "Deployment aborted.";
				return false;
			}
			output = output.trimmed();
			if (!output.isEmpty()) {
				qError() << output;
				qError() << "Deployment aborted.";
				return false;
			}
		}
	}

	qDebug() << "Deployment finished.";
	return true;
}

void generateHostDeploymentScript(NetGraph &g, bool realRouting) {
	//QMutexLockerDbg locker(g.mutex, __FUNCTION__); Q_UNUSED(locker);
	QStringList lines;
	lines << QString("#!/usr/bin/perl");

	QString netdev = REMOTE_DEDICATED_IF_HOSTS;
	QList<QString> netdevs = QList<QString>() << "eth0" << "eth1" << "eth2" << "eth3";

	lines << QString("sub command {\n"
					 "my ($cmd) = @_;\n"
					 "print \"$cmd\\n\";\n"
					 "system \"$cmd\";\n"
					 "}");

	lines << QString("# Flush the routing table - only the 10/8 routes\n"
					 "foreach my $route (`netstat -rn | grep '^10.'`) {\n"
					 "my ($dest, $gate, $linuxmask) = split ' ',$route;\n"
					 "command(\"/sbin/route delete -net $dest netmask $linuxmask\");\n"
					 "}");

	lines << QString("#Set interface params");
	lines << QString("command \"sh -c 'echo 0 | tee /proc/sys/net/ipv4/conf/*/rp_filter'\";");
	lines << QString("command \"sh -c 'echo 1 | tee /proc/sys/net/ipv4/conf/*/accept_local'\";");
	lines << QString("command \"sh -c 'echo 0 | tee /proc/sys/net/ipv4/conf/*/accept_redirects'\";");
	lines << QString("command \"sh -c 'echo 0 | tee /proc/sys/net/ipv4/conf/*/send_redirects'\";");

	lines << QString("#Set up interface aliases");

	lines << QString("command \"sh -c '/sbin/ifdown %1 2> /dev/null'\";").arg(netdev);
	lines << QString("command \"sh -c '/sbin/ifup %1 2> /dev/null'\";").arg(netdev);
	if (!realRouting) {
		foreach (NetGraphNode n, g.nodes) {
			if (n.nodeType != NETGRAPH_NODE_HOST)
				continue;
			if (!n.used)
				continue;
			lines << QString("command \"sh -c '/sbin/ifconfig %1:%2 %3 netmask 255.128.0.0'\";").arg(netdev).arg(n.index).arg(n.ip());
		}
	}
	lines << QString("command \"sh -c '/sbin/ifconfig %1 mtu 1500'\";").arg(netdev);
	if (realRouting) {
		foreach (QString dev, netdevs) {
			lines << QString("command \"sh -c '/sbin/ifconfig %1 mtu 1500'\";").arg(dev);
		}
	}
	// PMTU discovery is very bad for us, since the kernel attempts to send large packets
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/ip_no_pmtu_disc'\";");
	// We want TCP timestamps
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_timestamps'\";");
	// How long to keep sockets in the state FIN-WAIT-2 (seconds)
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_fin_timeout'\";");
	// Allow the reuse of TIME_WAIT sockets for new connections
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse'\";");
	// This seems dangerous:
	lines << QString("command \"sh -c 'echo 0 > /proc/sys/net/ipv4/tcp_tw_recycle'\";");
	// You will see many "TCP: too many orphaned sockets" messages in dmesg. Don't worry about it.
	// Each "orphaned" socket uses 1500 bytes of memory.
	lines << QString("command \"sh -c 'echo 100000 > /proc/sys/net/ipv4/tcp_max_orphans'\";");

	// TCP window size settings
	// Assuming a certain RTT, this is the relation between the window size and the bandwidth limit:
	// (formula: bw = win / rtt, numbers are approximate)
	// Bandwidth limit   Window (RTT 10ms)   Window (RTT 100ms)   Window (RTT 250ms)
	//   1 Mbps           1.25 kB             12.5 kB			   32 kB
	//  10 Mbps           12.5 kB              125 kB			  312 kB
	//  40 Mbps             50 kB              500 kB            1.25 MB
	// 100 Mbps            125 kB             1.25 MB             3.2 MB
	//   1 Gbps           1.25 MB             12.5 MB              32 MB
	//  10 Gbps           12.5 MB              125 MB             312 MB
	// Since in most cases we use a bottleneck up to 40 Mbps, and we have an RTT of up to 200 ms, a window that defaults
	// to 1 MB is enough. We also want to make sure that the maximum window is suitable for a 1 Gbps link, i.e. we need
	// around 32 MB.

	// Set the maximum window to 32 MB
	lines << QString("command \"sh -c 'echo 32000000 > /proc/sys/net/core/rmem_max'\";");
	lines << QString("command \"sh -c 'echo 32000000 > /proc/sys/net/core/wmem_max'\";");

	// Set the default window to 1.4 MB
	lines << QString("command \"sh -c 'echo 1400000 > /proc/sys/net/core/rmem_default'\";");
	lines << QString("command \"sh -c 'echo 1400000 > /proc/sys/net/core/wmem_default'\";");

	// Set the TCP memory limit thresholds (used to decide "memory pressure"). The first two are a threshold with
	// histerezis, the third is a maximum limit. All are in pages, so multiply by 4096 (maybe!) to get the value in
	// bytes. TODO: the Linux defaults seem large (several GB), so leave them as they are.
	// TODO: lines << QString("command \"sh -c 'echo ?? ?? ?? > /proc/sys/net/ipv4/tcp_mem'\";");

	// Set the TCP window (minimum, default, maximum)
	lines << QString("command \"sh -c 'echo 65536 1400000 32000000 > /proc/sys/net/ipv4/tcp_rmem'\";");
	lines << QString("command \"sh -c 'echo 65536 1400000 32000000 > /proc/sys/net/ipv4/tcp_wmem'\";");

	// Enable TCP window scaling
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_window_scaling'\";");

	// Increase the size of the network interface buffer (in packets; assuming 2 KB per packet, a window of 32 MB
	// corresponds to 16,000. So we use double, 30,000.
	lines << QString("command \"sh -c 'echo 30000 > /proc/sys/net/core/netdev_max_backlog'\";");

	// Make sure we use CUBIC for congestion control (default for Linux)
	lines << QString("command \"sh -c 'echo cubic > /proc/sys/net/ipv4/tcp_congestion_control'\";");

	// Disable TCP metric caching between connections
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_no_metrics_save'\";");

	// Flush stored TCP metrics from old connections.
	lines << QString("command \"sh -c 'ip tcp_metrics flush all 1>/dev/null 2>/dev/null || echo no tcp_metrics support'\";");

	// Show the stored TCP metrics
	lines << QString("command \"sh -c 'ip tcp_metrics 1>/dev/null 2>/dev/null || echo no tcp_metrics support'\";");

	// Disable syncookies, we do not need DDoS protection.
	lines << QString("command \"sh -c 'echo 0 > /proc/sys/net/ipv4/tcp_syncookies'\";");

	// Enable TCP selective acks.
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_sack'\";");

    // Disable Slow-Start Restart (required to generate DASH video traffic).
    lines << QString("command \"sh -c 'echo 0 > /proc/sys/net/ipv4/tcp_slow_start_after_idle'\";");

	if (!realRouting) {
		if (QString(REMOTE_DEDICATED_IP_HOSTS) != "127.0.0.1") {
			QString gateway = REMOTE_DEDICATED_IP_ROUTER;
			lines << QString("command \"sh -c '/sbin/route add -net 10.128.0.0/9 gw %1'\"").arg(gateway);
		} else {
			lines << QString("command \"sh -c '/sbin/route add -net 10.128.0.0/9 dev lo'\"");
		}
	}

	saveFile("deployhost.pl", lines.join("\n"));

	QFile clearFile(":/scripts/clearall.pl");
	clearFile.open(QIODevice::ReadOnly);
	QTextStream in(&clearFile);
	QString clearScript = in.readAll();
	clearFile.close();
	saveFile("clearall.pl", clearScript);
}
