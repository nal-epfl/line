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

#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::on_btnBenchmarkGenerateGraph_clicked()
{
	NetGraph g;

	QString trafficInput = ui->cmbBenchmarkTraffic->currentText();
	QString trafficLabel;
    if (trafficInput == "TCP CUBIC (long flows)") {
        trafficLabel = "tcp-long-cubic";
    } else if (trafficInput == "TCP CUBIC (medium flows)") {
        trafficLabel = "tcp-medium-cubic";
    } else if (trafficInput == "TCP CUBIC (short flows)") {
        trafficLabel = "tcp-short-cubic";
    } else if (trafficInput == "TCP NewReno (long flows)") {
        trafficLabel = "tcp-long-reno";
    } else if (trafficInput == "TCP NewReno (medium flows)") {
        trafficLabel = "tcp-medium-reno";
    } else if (trafficInput == "TCP NewReno (short flows)") {
        trafficLabel = "tcp-short-reno";
    } else if (trafficInput == "UDP (constant bitrate)") {
		trafficLabel = "udp-constant";
	} else if (trafficInput == "UDP (average bitrate)") {
		trafficLabel = "udp-average";
	} else {
		QMessageBox::critical(this,
							  "Error",
							  QString("Failure in %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(__FUNCTION__));
		return;
	}

	const int numPaths = ui->spinBenchmarkPaths->value();
	const int numFlowsPerPath = ui->spinBenchmarkFlowsPerPath->value();
	const int numHops = ui->spinBenchmarkHopsPerPath->value();
	const qreal totalBwMbps = ui->spinBenchmarkBandwidth->value();
	const int rtt = ui->spinBenchmarkRTT->value();
	const bool oneBottleneck = ui->checkBenchmarkBottleneck->isChecked();

	const QString graphName = QString("benchmark-paths-%1-flowspp-%2-hops-%3-bw-%4Mbps-rtt-%5-traffic-%6-bottleneck-%7")
							  .arg(numPaths)
							  .arg(numFlowsPerPath)
							  .arg(numHops)
							  .arg(totalBwMbps)
							  .arg(rtt)
							  .arg(trafficLabel)
							  .arg(oneBottleneck ? "yes" : "no");
	g.setFileName(graphName + ".graph");

	const qreal nodeDistance = 200;

	// First index: path
	// Second index: hop
	QList<QList<int> > nodes;

	// Create nodes
	for (int iPath = 0; iPath < numPaths; iPath++) {
		QList<int> pathNodes;
		for (int iHop = 0; iHop < numHops + 1; iHop++) {
			QPointF position = QPointF(nodeDistance * iPath, nodeDistance * iHop);
			int nodeType = ((iHop == 0) || (iHop == numHops)) ? NETGRAPH_NODE_HOST : NETGRAPH_NODE_GATEWAY;
			if (!oneBottleneck) {
				pathNodes << g.addNode(nodeType, position);
			} else {
				if (iHop != numHops - 1 && iHop != numHops - 2) {
					pathNodes << g.addNode(nodeType, position);
				} else {
					if (iPath == 0) {
						position = QPointF(nodeDistance * (numPaths - 1) / 2.0, nodeDistance * iHop);
						pathNodes << g.addNode(nodeType, position);
					} else {
						pathNodes << nodes[0][pathNodes.count()];
					}
				}
			}
		}
		nodes << pathNodes;
	}

	// Create links
	const qreal linkBw_MBps = oneBottleneck ? totalBwMbps : totalBwMbps / numPaths;
	const qreal linkBw_kBps = linkBw_MBps * 1000.0 / 8.0;
	const int linkDelay_ms = qMax(1, rtt / numHops / 2);
	for (int iPath = 0; iPath < numPaths; iPath++) {
		for (int iHop = 0; iHop < numHops; iHop++) {
			if (!oneBottleneck || iPath == 0 || (iHop != numHops - 2)) {
				g.addEdgeSym(nodes[iPath][iHop],
							 nodes[iPath][iHop + 1],
						linkBw_kBps,
						linkDelay_ms,
						0,
						MIN_QUEUE_LEN_FRAMES /* irrelevant */,
						true);
			}
		}
		// Add cross links so we can have a full mesh
		if (!oneBottleneck) {
			if (numPaths > 1 && !(numPaths == 2 && iPath > 0)) {
				const int iPathOther = (iPath + 1) % numPaths;
				g.addEdgeSym(nodes[iPath][1],
						nodes[iPathOther][1],
						linkBw_kBps,
						linkDelay_ms,
						0,
						MIN_QUEUE_LEN_FRAMES,
						true);
			}
		}
	}

	// Create connections
	for (int iPath = 0; iPath < numPaths; iPath++) {
		QString encodedType;
        if (trafficLabel == "tcp-long-cubic") {
            encodedType = QString("TCP cc cubic x %1").arg(numFlowsPerPath);
        } else if (trafficLabel == "tcp-medium-cubic") {
            encodedType = QString("TCP-Poisson-Pareto 1.0 3 8Mb sequential cc cubic x %1").arg(numFlowsPerPath);
        } else if (trafficLabel == "tcp-short-cubic") {
            encodedType = QString("TCP-Poisson-Pareto 1.0 3 2Mb sequential cc cubic x %1").arg(numFlowsPerPath);
        } else if (trafficLabel == "tcp-long-reno") {
            encodedType = QString("TCP cc reno x %1").arg(numFlowsPerPath);
        } else if (trafficLabel == "tcp-medium-reno") {
            encodedType = QString("TCP-Poisson-Pareto 1.0 3 8Mb sequential cc reno x %1").arg(numFlowsPerPath);
        } else if (trafficLabel == "tcp-short-reno") {
            encodedType = QString("TCP-Poisson-Pareto 1.0 3 2Mb sequential cc reno x %1").arg(numFlowsPerPath);
        } else if (trafficLabel == "udp-constant") {
			encodedType = QString("UDP-CBR %1 x %2").arg(linkBw_MBps / numFlowsPerPath).arg(numFlowsPerPath);
		} else if (trafficLabel == "udp-average") {
			encodedType = QString("UDP-CBR %1 poisson x %2").arg(linkBw_MBps / numFlowsPerPath).arg(numFlowsPerPath);
		} else {
			QMessageBox::critical(this,
								  "Error",
								  QString("Failure in %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(__FUNCTION__));
			return;
		}
		g.addConnection(NetGraphConnection(nodes[iPath].first(), nodes[iPath].last(), encodedType, QByteArray()));
	}

	g.saveToFile();
	topoDirChanged(QString());
	for (int i = 0; i < ui->cmbTopologies->count(); i++) {
		if (ui->cmbTopologies->itemText(i) == graphName) {
			ui->cmbTopologies->setCurrentIndex(i);
			break;
		}
	}

    emit logInformation(ui->txtBenchmark, "Graph generated.");
}
