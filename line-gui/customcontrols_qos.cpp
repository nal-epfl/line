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

#include "customcontrols.h"
#include "ui_customcontrols.h"
#include "ui_mainwindow.h"
#include "flowlayout.h"

#include "../line-gui/intervalmeasurements.h"
#include "chronometer.h"
#include "../remote_config.h"
#include "util.h"

// Bottleneck: link 1 (index 0)
// Bottleneck: 100 Mbps, 10 ms delay, queue auto adjusted
// Bottleneck: 2 policing classes: 1.0/0.3
// Bottleneck: 1 shaper: 1.0
// Buffer bloat factor: 20%
// 50 TCP-Poisson-Pareto sequential flows, with Pareto scale 0.5 Mb, shape 2 and rate 0.1 new flows/s
// Duration 600s, interval size 5s (120 intervals)
void CustomControls::autoPolicingSetupDefaults(bool shaping)
{
	const qint32 bottleneck = 0;

	if (!ui->checkAutoDiffRTT->isChecked()) {
		qint32 linkDelay = ui->spinAutoRTT->value() / 6;
		foreach (NetGraphEdge e, mainWindow->editor->graph()->edges) {
			mainWindow->editor->graph()->edges[e.index].delay_ms = linkDelay;
		}
	} else {
		// Do it outside this function!!!
	}

	// Link: 100 Mbps, 10 ms delay, auto adjust queue
	{
		NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
		e.bandwidth = 100 * 1000 * 1000 / 8 / 1000;
		e.queueLength = NetGraph::optimalQueueLength(e.bandwidth * 1.0e3 / 8.0, e.delay_ms);
		e.policerCount = 2;
		e.policerWeights = ui->checkAutoNeutral->isChecked() ?
							   (QList<qreal>() << 1.0 << 1.0).toVector() :
							   (QList<qreal>() << 1.0 << 0.3).toVector();
		e.queueCount = 1;
		e.queueWeights = (QList<qreal>() << 1.0).toVector();
	}

	// TCP-Poisson-Pareto sequential flows, with size 10 Mb and rate 0.1 new flows/s
	if (!ui->checkAutoCustomTraffic->isChecked()) {
		for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
			NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
			if (c.basicType == "TCP-Poisson-Pareto") {
				c.paretoScale_b = 1 * 1000 * 1000;
				c.poissonRate = 0.1;
				c.paretoAlpha = 2;
				c.sequential = true;
				if (shaping) {
					c.multiplier = 70 + (ui->spinAutoRTT->value() - 50) / 7;
				} else {
					c.multiplier = 50 + (ui->spinAutoRTT->value() - 50) / 7;
				}
				if (!ui->checkAutoDiffTcpCc->isChecked()) {
					c.tcpCongestionControl = ui->cmbAutoTcp->currentText();
				}
				c.setTypeFromParams();
			}
		}
	}

    mainWindow->ui->txtGenericTimeout->setText("600s");
    //mainWindow->ui->txtIntervalSize->setText("5s");
    //mainWindow->ui->txtIntervalSize->setText("100ms");
}

void CustomControls::autoPolicingGenerateSuffix()
{
    bool neutral = (mainWindow->editor->graph()->edges[0].policerCount == 1 && mainWindow->editor->graph()->edges[0].queueCount == 1) ||
                   (mainWindow->editor->graph()->edges[0].queueCount == 1 && mainWindow->editor->graph()->edges[0].policerCount == 2 &&
            mainWindow->editor->graph()->edges[0].policerWeights[0] == 1.0 &&
            mainWindow->editor->graph()->edges[0].policerWeights[1] == 1.0);
    bool policing = !neutral && mainWindow->editor->graph()->edges[0].policerCount == 2 && mainWindow->editor->graph()->edges[0].queueCount == 1;
    bool shaping = !neutral && mainWindow->editor->graph()->edges[0].policerCount == 1 && mainWindow->editor->graph()->edges[0].queueCount == 2;

    bool largeBuffers = mainWindow->ui->cmbBufferSize->currentText() == "large" && mainWindow->ui->spinBufferBloadFactor->value() == 1.0;
    bool mediumBuffers = mainWindow->ui->cmbBufferSize->currentText() == "large" && mainWindow->ui->spinBufferBloadFactor->value() < 1.0;
    bool smallBuffers = mainWindow->ui->cmbBufferSize->currentText() == "small";
    Q_UNUSED(largeBuffers);
    Q_UNUSED(mediumBuffers);
    Q_UNUSED(smallBuffers);

    QString tcp;
    if (ui->checkAutoDiffTcpCc->isChecked()) {
        tcp = QString("%1-%2")
              .arg(mainWindow->editor->graph()->connections[0].tcpCongestionControl)
                .arg(mainWindow->editor->graph()->connections[2].tcpCongestionControl);
    } else {
        for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
            NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
            if (c.basicType == "TCP-Poisson-Pareto") {
                tcp = c.tcpCongestionControl;
            }
        }
    }

    qreal transferSize1 = 0;
    qreal transferSize2 = 0;
    for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
        NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
        if (c.basicType == "TCP-Poisson-Pareto") {
            if (c.trafficClass == 0) {
                transferSize1 = c.paretoScale_b / 1.0e6;;
            } else {
                transferSize2 = c.paretoScale_b / 1.0e6;;
            }
        }
    }

    mainWindow->ui->txtBatchExpSuffix->setText(QString("link-100Mbps-pareto-%1Mb-pareto-%2Mb-%3%4%5%6")
                                   .arg(transferSize1)
                                   .arg(transferSize2)
                                   .arg(neutral ?
                                            QString("neutral") :
                                            policing ?
                                            QString("policing-%1-%2")
                                            .arg(mainWindow->editor->graph()->edges[0].policerWeights[0], 0, 'f', 1)
                                            .arg(mainWindow->editor->graph()->edges[0].policerWeights[1], 0, 'f', 1) :
                                            shaping ?
                                                QString("shaping-%1-%2")
                                                .arg(mainWindow->editor->graph()->edges[0].queueWeights[0], 0, 'f', 1)
                                                .arg(mainWindow->editor->graph()->edges[0].queueWeights[1], 0, 'f', 1) :
                                                QString("unknown"))
                                   .arg(QString("-buffers-%1-%2-%3-%4")
                                        .arg(mainWindow->ui->cmbBufferSize->currentText())
                                        .arg(mainWindow->ui->cmbQoSBufferScaling->currentText())
                                        .arg(mainWindow->ui->spinBufferBloadFactor->value())
                                        .arg(mainWindow->ui->cmbQueueingDiscipline->currentText()))
                                   .arg(QString("-rtt-%1")
                                        .arg(!ui->checkAutoDiffRTT->isChecked() ?
                                                 QString("%1").arg(ui->spinAutoRTT->value())
                                               : QString("%1-%2")
                                                 .arg(mainWindow->editor->graph()->edges[0].delay_ms * 2 + mainWindow->editor->graph()->edges[2].delay_ms * 4)
                                        .arg(mainWindow->editor->graph()->edges[0].delay_ms * 2 + mainWindow->editor->graph()->edges[6].delay_ms * 4)))
            .arg(QString("-cc-%1").arg(tcp)));
}

void CustomControls::on_btnAutoPolicingPlot1_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(false);

	const qint32 bottleneck = 0;

	QList<qreal> policingRates;
	policingRates << 1.0 << 0.5 << 0.4 << 0.3 << 0.2;
	foreach (qreal policingRate, policingRates) {
		// Adjust policers
		{
			NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
			e.policerCount = 2;
			e.policerWeights.resize(e.policerCount);
			e.policerWeights[0] = 1.0;
			e.policerWeights[1] = policingRate;
		}
		autoPolicingGenerateSuffix();
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

// Same as Plot1 but with long flows
void CustomControls::on_btnAutoPolicingPlot1b_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(false);

	// Adjust flows
	for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
		NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
		if (c.basicType == "TCP-Poisson-Pareto") {
			c.paretoScale_b = 9999 * 1.0e6;
			c.multiplier = 4;
			c.setTypeFromParams();
		}
	}

	const qint32 bottleneck = 0;

	QList<qreal> policingRates;
	policingRates << 1.0 << 0.5 << 0.4 << 0.3 << 0.2;
	foreach (qreal policingRate, policingRates) {
		// Adjust policers
		{
			NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
			e.policerCount = 2;
			e.policerWeights.resize(e.policerCount);
			e.policerWeights[0] = 1.0;
			e.policerWeights[1] = policingRate;
		}
		autoPolicingGenerateSuffix();
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

// Same as Plot1 but with long flows only for class 2
void CustomControls::on_btnAutoPolicingPlot1c_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(false);

	// Adjust flows
	for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
		NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
		if (c.basicType == "TCP-Poisson-Pareto" && c.trafficClass == 1) {
			c.paretoScale_b = 9999 * 1.0e6;
			c.multiplier = 4;
			c.setTypeFromParams();
		}
	}

	const qint32 bottleneck = 0;

	QList<qreal> policingRates;
	policingRates << 1.0 << 0.5 << 0.4 << 0.3 << 0.2;
	foreach (qreal policingRate, policingRates) {
		// Adjust policers
		{
			NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
			e.policerCount = 2;
			e.policerWeights.resize(e.policerCount);
			e.policerWeights[0] = 1.0;
			e.policerWeights[1] = policingRate;
		}
		autoPolicingGenerateSuffix();
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

// Same as Plot1 but with shaping
void CustomControls::on_btnAutoPolicingPlot7_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(true);

	const qint32 bottleneck = 0;

	QList<qreal> shapingRates;
	shapingRates << 0.5 << 0.4 << 0.3 << 0.2;
	foreach (qreal shapingRate, shapingRates) {
		// Adjust shapers
		{
			NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
			e.policerCount = 1;
			e.policerWeights.resize(e.policerCount);
			e.policerWeights[0] = 1.0;

			e.queueCount = 2;
			e.queueWeights.resize(e.queueCount);
			e.queueWeights[0] = 1.0 - shapingRate;
			e.queueWeights[1] = shapingRate;
		}
		autoPolicingGenerateSuffix();
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

// Same as Plot 7 but with long flows
void CustomControls::on_btnAutoPolicingPlot7b_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(true);

	// Adjust flows
	for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
		NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
		if (c.basicType == "TCP-Poisson-Pareto") {
			c.paretoScale_b = 9999 * 1.0e6;
			c.multiplier = 4;
			c.setTypeFromParams();
		}
	}

	const qint32 bottleneck = 0;

	QList<qreal> shapingRates;
	shapingRates << 0.5 << 0.4 << 0.3 << 0.2;
	foreach (qreal shapingRate, shapingRates) {
		// Adjust shapers
		{
			NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
			e.policerCount = 1;
			e.policerWeights.resize(e.policerCount);
			e.policerWeights[0] = 1.0;

			e.queueCount = 2;
			e.queueWeights.resize(e.queueCount);
			e.queueWeights[0] = 1.0 - shapingRate;
			e.queueWeights[1] = shapingRate;
		}
		autoPolicingGenerateSuffix();
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

void CustomControls::on_btnAutoPolicingPlot2_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot2_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(false);

	autoPolicingPlot2common();
}

// Like 2, but with shaping
void CustomControls::on_btnAutoPolicingPlot2b_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot2_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(true);

	const qint32 bottleneck = 0;
	const qreal shapingRate = 0.3;
	// Adjust shapers
	{
		NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
		e.policerCount = 1;
		e.policerWeights.resize(e.policerCount);
		e.policerWeights[0] = 1.0;

		e.queueCount = 2;
		e.queueWeights.resize(e.queueCount);
		e.queueWeights[0] = 1.0 - shapingRate;
		e.queueWeights[1] = shapingRate;
	}

	autoPolicingPlot2common();
}

void CustomControls::autoPolicingPlot2common()
{
	QList<RealRealPair> transferSizes;
	const qreal mega = 1.0e6;
	if (!ui->checkAutoDiffSize->isChecked()) {
		if (!ui->checkAutoFast->isChecked()) {
			transferSizes << RealRealPair(0.2, 0.2)
						  << RealRealPair(0.5, 0.5)
						  << RealRealPair(1, 1)
						  << RealRealPair(5, 5)
						  << RealRealPair(10, 10)
						  << RealRealPair(20, 20)
						  << RealRealPair(40, 40)
						  << RealRealPair(9999, 9999);
		} else {
			transferSizes << RealRealPair(1, 1)
						  << RealRealPair(40, 40)
						  << RealRealPair(9999, 9999);
		}
	} else {
		if (!ui->checkAutoFast->isChecked()) {
			transferSizes << RealRealPair(1, 0.2)
						  << RealRealPair(1, 0.5)
						  << RealRealPair(1, 1)
						  << RealRealPair(1, 5)
						  << RealRealPair(1, 10)
						  << RealRealPair(1, 20)
						  << RealRealPair(1, 40)
						  << RealRealPair(1, 9999);
		} else {
			transferSizes << RealRealPair(1, 1)
						  << RealRealPair(1, 10)
						  << RealRealPair(1, 40)
						  << RealRealPair(1, 9999);
		}
	}
	if (ui->checkAutoReverseSize->isChecked()) {
		for (int i = 0; i < transferSizes.count(); i++) {
			qSwap(transferSizes[i].first, transferSizes[i].second);
		}
	}

    bool largeBuffers = mainWindow->ui->cmbBufferSize->currentText() == "large" && mainWindow->ui->spinBufferBloadFactor->value() == 1.0;
    bool mediumBuffers = mainWindow->ui->cmbBufferSize->currentText() == "large" && mainWindow->ui->spinBufferBloadFactor->value() < 1.0;
    bool smallBuffers = mainWindow->ui->cmbBufferSize->currentText() == "small";
    Q_UNUSED(largeBuffers);
    Q_UNUSED(mediumBuffers);
    Q_UNUSED(smallBuffers);

	foreach (RealRealPair transferSizeByClass, transferSizes) {
		// Adjust flows
		for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
			NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
			qreal transferSize = c.trafficClass == 0 ? transferSizeByClass.first : transferSizeByClass.second;
			if (c.basicType == "TCP-Poisson-Pareto") {
				c.paretoScale_b = transferSize * mega;
				if (transferSize == 0.2) {
					if (largeBuffers) {
						// TODO
						c.multiplier = 800;
					} else {
						c.multiplier = 230;
					}
				} else if (transferSize == 0.5) {
					if (largeBuffers) {
						// TODO
						c.multiplier = 180;
					} else {
						c.multiplier = 50;
					}
				} else if (transferSize == 1) {
					if (largeBuffers) {
						c.multiplier = 70;
					} else {
						c.multiplier = 20;
					}
				} else if (transferSize == 5) {
					c.multiplier = 4;
				} else if (transferSize == 10) {
					c.multiplier = 4;
				} else if (transferSize == 20) {
					c.multiplier = 4;
				} else if (transferSize == 40) {
					c.multiplier = 4;
				} else if (transferSize == 9999) {
					c.multiplier = 4;
				} else {
					qDebug() << __FILE__ << __LINE__ << "not implemented";
					continue;
				}
				c.setTypeFromParams();
			}
		}
		autoPolicingGenerateSuffix();
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

void CustomControls::on_btnAuto2All_clicked()
{
	// Edge indices in natural indexing
	QList<qint32> edges1 = QList<qint32>() << 3 << 4 << 5 << 6 << 11 << 12 << 13 << 14;
	QList<qint32> edges2 = QList<qint32>() << 7 << 8 << 9 << 10 << 15 << 16 << 17 << 18;
	QList<qint32> edges12 = QList<qint32>() << 1 << 2;

	QList<QString> bufferSizes = QList<QString>() << "large" << "medium" << "small";
	QList<QString> qosBufferScalings = ui->checkAutoNeutral->isChecked() ?
										   (QList<QString>() << "none") :
										   (QList<QString>() << "none" << "down");
	QList<QInt32Pair> delays12;

	if (!ui->checkAutoDiffRTT->isChecked()) {
		delays12 << QInt32Pair(ui->spinAutoRTT->value() / 6,
							   ui->spinAutoRTT->value() / 6);
	} else {
		qint32 linkDelay1 = 50 / 6;
		qint32 linkDelay2 = (120 / 2 - linkDelay1) / 2;
		delays12 << QInt32Pair(linkDelay1, linkDelay2) << QInt32Pair(linkDelay2, linkDelay1);
	}

	QList<QStringPair> ccs12;

	if (!ui->checkAutoDiffTcpCc->isChecked()) {
		ccs12 << QStringPair("", "");
	} else {
		ccs12 << QStringPair("cubic", "reno") << QStringPair("reno", "cubic");
	}

	foreach (QString bufferSize, bufferSizes) {
		if (bufferSize == "medium") {
			bufferSize = "large";
			mainWindow->ui->spinBufferBloadFactor->setValue(0.2);
		} else {
			mainWindow->ui->spinBufferBloadFactor->setValue(1.0);
		}
		setCurrentItem(mainWindow->ui->cmbBufferSize, bufferSize);
		foreach (QString qosBufferScaling, qosBufferScalings) {
			setCurrentItem(mainWindow->ui->cmbQoSBufferScaling, qosBufferScaling);
			foreach (QInt32Pair delay12, delays12) {
				foreach (qint32 e, edges1) {
					mainWindow->editor->graph()->edges[e - 1].delay_ms = delay12.first;
				}
				foreach (qint32 e, edges12) {
					mainWindow->editor->graph()->edges[e - 1].delay_ms = qMin(delay12.first, delay12.second);
				}
				foreach (qint32 e, edges2) {
					mainWindow->editor->graph()->edges[e - 1].delay_ms = delay12.second;
				}
				foreach (QStringPair cc12, ccs12) {
					foreach (NetGraphConnection c, mainWindow->editor->graph()->connections) {
						if (c.trafficClass == 0) {
							mainWindow->editor->graph()->connections[c.index].tcpCongestionControl = cc12.first;
						} else {
							mainWindow->editor->graph()->connections[c.index].tcpCongestionControl = cc12.second;
						}
						mainWindow->editor->graph()->connections[c.index].setTypeFromParams();
					}
					on_btnAutoPolicingPlot2_clicked();
					if (!ui->checkAutoNeutral->isChecked()) {
						on_btnAutoPolicingPlot2b_clicked();
					}
				}
			}
		}
	}
}

void CustomControls::on_btnAutoPolicingPlot3_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(false);

	const qint32 bottleneck = 0;

	QList<qreal> bandwidthsMbps;
	bandwidthsMbps << 20 << 100 << 500 << 1000;
	foreach (qreal bandwidth, bandwidthsMbps) {
		// Adjust bottleneck bandwidth
		{
			NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
			e.bandwidth = bandwidth * 1000.0 / 8.0;
			e.queueLength = NetGraph::optimalQueueLength(e.bandwidth * 1.0e3 / 8.0, e.delay_ms);
		}
		// Adjust flows
		for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
			NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
			if (c.basicType == "TCP-Poisson-Pareto") {
				if (bandwidth == 20) {
					c.multiplier = 1;
					c.poissonRate = 0.1;
					c.setTypeFromParams();
				} else if (bandwidth == 100) {
					c.multiplier = 50;
					c.poissonRate = 0.1;
					c.setTypeFromParams();
				} else if (bandwidth == 500) {
					c.multiplier = 700;
					c.poissonRate = 0.1;
					c.setTypeFromParams();
				} else if (bandwidth == 1000) {
					c.multiplier = 1400;
					c.poissonRate = 0.1;
					c.setTypeFromParams();
				} else {
					qDebug() << __FILE__ << __LINE__ << "not implemented";
					continue;
				}
			}
		}
		mainWindow->ui->txtBatchExpSuffix->setText(QString("link-%1Mbps%2%3")
									   .arg(bandwidth)
									   .arg(QString("-buffers-%1-%2-%3-%4")
											.arg(mainWindow->ui->cmbBufferSize->currentText())
											.arg(mainWindow->ui->cmbQoSBufferScaling->currentText())
											.arg(mainWindow->ui->spinBufferBloadFactor->value())
											.arg(mainWindow->ui->cmbQueueingDiscipline->currentText()))
									   .arg(QString("-rtt-%1")
											.arg(ui->spinAutoRTT->value())));
		mainWindow->on_btnEnqueue_clicked(true);
	}
	mainWindow->updateQueueTable();
}

void CustomControls::on_btnAutoPolicingPlot4_clicked()
{
	if (mainWindow->ui->cmbSimulation->currentIndex() != 0) {
		mainWindow->ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnAutoPolicingPlot1_clicked()));
		 return;
	}

	autoPolicingSetupDefaults(false);

	QList<int> flowCounters;
	flowCounters << 30 << 40 << 50 << 60 << 70;
	foreach (int flowCount, flowCounters) {
		// Adjust flows
		for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
			NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
			if (c.basicType == "TCP-Poisson-Pareto") {
				c.multiplier = flowCount;
				c.setTypeFromParams();
			}
		}
		mainWindow->ui->txtBatchExpSuffix->setText(QString("link-100Mbps-nflows-%1%2%3")
									   .arg(flowCount)
									   .arg(QString("-buffers-%1-%2-%3-%4")
											.arg(mainWindow->ui->cmbBufferSize->currentText())
											.arg(mainWindow->ui->cmbQoSBufferScaling->currentText())
											.arg(mainWindow->ui->spinBufferBloadFactor->value())
											.arg(mainWindow->ui->cmbQueueingDiscipline->currentText()))
									   .arg(QString("-rtt-%1")
											.arg(ui->spinAutoRTT->value())));
		mainWindow->on_btnEnqueue_clicked(true);
	}
    mainWindow->updateQueueTable();
}

///////////// ALL CLEAN FROM HERE

void CustomControls::autoReset(int durationSec)
{
    const qint32 bottleneck = 0;

    // Link 1: 100 Mbps, neutral
    {
        NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
        e.bandwidth = 100 * 1000 * 1000 / 8 / 1000;
    }

    mainWindow->ui->checkReadjustQueues->setChecked(true);
    mainWindow->ui->txtGenericTimeout->setText(QString("%1s").arg(durationSec));
    mainWindow->ui->txtIntervalSize->setText("50ms");
    //mainWindow->ui->txtIntervalSize->setText("100ms");
    //mainWindow->ui->txtIntervalSize->setText("5s");
}

void CustomControls::setNeutral()
{
    const qint32 bottleneck = 0;

    NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
    e.policerCount = 1;
    e.policerWeights = (QList<qreal>() << 1.0).toVector();
    e.queueCount = 1;
    e.queueWeights = (QList<qreal>() << 1.0).toVector();
}

void CustomControls::setPolicing(qreal weight1, qreal weight2)
{
    const qint32 bottleneck = 0;

    NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
    e.policerCount = 2;
    e.policerWeights = (QList<qreal>() << weight1 << weight2).toVector();
    e.queueCount = 1;
    e.queueWeights = (QList<qreal>() << 1.0).toVector();
}

void CustomControls::setShaping(qreal weight1, qreal weight2)
{
    const qint32 bottleneck = 0;

    if (weight1 + weight2 != 1.0) {
        ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
    }

    NetGraphEdge &e = mainWindow->editor->graph()->edges[bottleneck];
    e.policerCount = 1;
    e.policerWeights = (QList<qreal>() << 1.0).toVector();
    e.queueCount = 2;
    e.queueWeights = (QList<qreal>() << weight1 << weight2).toVector();
}

void CustomControls::setRtt(qint32 rtt1, qint32 rtt2)
{
    qint32 linkDelay1 = rtt1 / 6;
    qint32 linkDelay2 = (rtt2 / 2 - linkDelay1) / 2;

    QList<qint32> edges1 = QList<qint32>() << 3 << 4 << 5 << 6 << 11 << 12 << 13 << 14;
    QList<qint32> edges2 = QList<qint32>() << 7 << 8 << 9 << 10 << 15 << 16 << 17 << 18;
    QList<qint32> edges12 = QList<qint32>() << 1 << 2;

    foreach (qint32 e, edges1) {
        mainWindow->editor->graph()->edges[e - 1].delay_ms = linkDelay1;
    }
    foreach (qint32 e, edges12) {
        mainWindow->editor->graph()->edges[e - 1].delay_ms = qMin(linkDelay1, linkDelay2);
    }
    foreach (qint32 e, edges2) {
        mainWindow->editor->graph()->edges[e - 1].delay_ms = linkDelay2;
    }
}

void CustomControls::setTcp(QString tcp1, QString tcp2)
{
    foreach (NetGraphConnection c, mainWindow->editor->graph()->connections) {
        if (c.basicType.startsWith("TCP")) {
            mainWindow->editor->graph()->connections[c.index].tcpCongestionControl = c.trafficClass == 0 ? tcp1 : tcp2;
            mainWindow->editor->graph()->connections[c.index].setTypeFromParams();
        }
    }
}

void CustomControls::setBuffer(QString bufferSize, qreal bufferScaling, QString bufferQoSScaling)
{
    if (!setCurrentItem(mainWindow->ui->cmbBufferSize, bufferSize)) {
        ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
    }
    mainWindow->ui->spinBufferBloadFactor->setValue(bufferScaling);
    if (!setCurrentItem(mainWindow->ui->cmbQoSBufferScaling, bufferQoSScaling)) {
        ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
    }
}

void CustomControls::setQDisc(QString qdisc)
{
    if (!setCurrentItem(mainWindow->ui->cmbQueueingDiscipline, qdisc)) {
        ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
    }
}

// transferSize in Mb
// Returns -1 in case of error.
int getNumFlows(int transferSize, bool largeBuffers, QString congestion)
{
	if (transferSize == 1) {
		if (largeBuffers) {
			if (congestion == "none1") {
				return 70 / 3;
			} else if (congestion == "none2") {
				return 70 / 2;
			} else if (congestion == "none3") {
				return 2 * 70 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 70;
			} else if (congestion == "medium") {
				return 100;
			} else if (congestion == "high") {
				return 140;
			} else {
				return -1;
			}
		} else {
			if (congestion == "none1") {
				return 30 / 3;
			} else if (congestion == "none2") {
				return 30 / 2;
			} else if (congestion == "none3") {
				return 2 * 30 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 30;
			} else if (congestion == "medium") {
				return 50;
			} else if (congestion == "high") {
				return 100;
			} else {
				return -1;
			}
		}
	} else if (transferSize == 10) {
		if (largeBuffers) {
			if (congestion == "none1") {
				return 15 / 3;
			} else if (congestion == "none2") {
				return 15 / 2;
			} else if (congestion == "none3") {
				return 2 * 15 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 15;
			} else if (congestion == "medium") {
				return 50;
			} else if (congestion == "high") {
				return 80;
			} else {
				return -1;
			}
		} else {
			if (congestion == "none1") {
				return 12 / 3;
			} else if (congestion == "none2") {
				return 12 / 2;
			} else if (congestion == "none3") {
				return 2 * 12 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 12;
			} else if (congestion == "medium") {
				return 35;
			} else if (congestion == "high") {
				return 50;
			} else {
				return -1;
			}
		}
	} else if (transferSize == 40) {
		if (largeBuffers) {
			if (congestion == "none1") {
				return 20 / 3;
			} else if (congestion == "none2") {
				return 20 / 2;
			} else if (congestion == "none3") {
				return 2 * 20 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 20;
			} else if (congestion == "medium") {
				return 50;
			} else if (congestion == "high") {
				return 80;
			} else {
				return -1;
			}
		} else {
			if (congestion == "none1") {
				return 12 / 3;
			} else if (congestion == "none2") {
				return 12 / 2;
			} else if (congestion == "none3") {
				return 2 * 12 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 12;
			} else if (congestion == "medium") {
				return 25;
			} else if (congestion == "high") {
				return 40;
			} else {
				return -1;
			}
		}
	} else if (transferSize == 9999) {
		if (largeBuffers) {
			if (congestion == "none1") {
				return 15 / 3;
			} else if (congestion == "none2") {
				return 15 / 2;
			} else if (congestion == "none3") {
				return 2 * 15 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 15;
			} else if (congestion == "medium") {
				return 40;
			} else if (congestion == "high") {
				return 70;
			} else {
				return -1;
			}
		} else {
			if (congestion == "none1") {
				return 10 / 3;
			} else if (congestion == "none2") {
				return 10 / 2;
			} else if (congestion == "none3") {
				return 2 * 10 / 3;
			} else if (congestion == "none") {
				return -1;
			} else if (congestion == "light") {
				return 10;
			} else if (congestion == "medium") {
				return 20;
			} else if (congestion == "high") {
				return 30;
			} else {
				return -1;
			}
		}
	}
	return -1;
}

void CustomControls::setTransferSize(qreal size1, qreal size2,
                                     QString congestion1, QString congestion2,
                                     QString buffers, qreal bufferScaling)
{
    bool largeBuffers = buffers == "large" && bufferScaling >= 1.0;
    bool mediumBuffers = buffers == "large" && bufferScaling < 1.0;
    bool smallBuffers = buffers == "small";
    Q_UNUSED(mediumBuffers);
    Q_UNUSED(smallBuffers);

    for (int iConnection = 0; iConnection < mainWindow->editor->graph()->connections.count(); iConnection++) {
        NetGraphConnection &c = mainWindow->editor->graph()->connections[iConnection];
        if (c.basicType == "TCP-Poisson-Pareto") {
            qreal transferSize = c.trafficClass == 0 ? size1 : size2;
            QString congestion = c.trafficClass == 0 ? congestion1 : congestion2;
            c.paretoScale_b = transferSize * 1.0e6;
			c.multiplier = getNumFlows(transferSize, largeBuffers, congestion);
			if (c.multiplier <= 0) {
				c.multiplier = 1;
				ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
                continue;
            }
            c.setTypeFromParams();
        }
    }
}

QString CustomControls::autoGenerateSuffix(QString hidden,
                                           QString bufferSize, qreal bufferScaling, QString bufferQoSScaling,
                                           QString qdisc,
                                           qint32 rtt1, qint32 rtt2,
                                           QString tcp1, QString tcp2,
                                           qreal size1, qreal size2,
                                           QString congestion1, QString congestion2)
{
    NetGraph *graph = mainWindow->editor->graph();

    bool neutral = (graph->edges[0].policerCount == 1 && graph->edges[0].queueCount == 1) ||
                   (graph->edges[0].queueCount == 1 && graph->edges[0].policerCount == 2 &&
            graph->edges[0].policerWeights[0] == 1.0 &&
            graph->edges[0].policerWeights[1] == 1.0);
    bool policing = !neutral && graph->edges[0].policerCount == 2 && graph->edges[0].queueCount == 1;
    bool shaping = !neutral && graph->edges[0].policerCount == 1 && graph->edges[0].queueCount == 2;
    QString qos;
    if (policing && shaping) {
        qos = QString("policing-%1-%2-shaping-%3-%4")
              .arg(graph->edges[0].policerWeights[0], 0, 'f', 1)
                .arg(graph->edges[0].policerWeights[1], 0, 'f', 1)
                .arg(graph->edges[0].queueWeights[0], 0, 'f', 1)
                .arg(graph->edges[0].queueWeights[1], 0, 'f', 1);
    } else if (policing) {
        qos = QString("policing-%1-%2")
              .arg(graph->edges[0].policerWeights[0], 0, 'f', 1)
                .arg(graph->edges[0].policerWeights[1], 0, 'f', 1);
    } else if (shaping) {
        qos = QString("shaping-%1-%2")
              .arg(graph->edges[0].queueWeights[0], 0, 'f', 1)
                .arg(graph->edges[0].queueWeights[1], 0, 'f', 1);
    } else {
        qos = "neutral";
    }

    bool largeBuffers = bufferSize == "large" && bufferScaling == 1.0;
    bool mediumBuffers = bufferSize == "large" && bufferScaling < 1.0;
    bool smallBuffers = bufferSize == "small";
    Q_UNUSED(largeBuffers);
    Q_UNUSED(mediumBuffers);
    Q_UNUSED(smallBuffers);

    QString result;

    result += QString("link-100Mbps");

    if (hidden != "transfer") {
        if (hidden != "transfer-size") {
            result += QString("-transfer-size-%1-%2").arg(size1).arg(size2);
        }

        if (hidden != "congestion") {
            result += QString("-congestion-%1-%2").arg(congestion1).arg(congestion2);
        }
    }

    if (hidden != "qos") {
        result += QString("-qos-%1").arg(qos);
    }

    if (hidden != "buffers") {
        result += QString("-buffers-%1-%2-%3-%4")
                  .arg(bufferSize)
                  .arg(bufferQoSScaling)
                  .arg(bufferScaling)
                  .arg(qdisc);
    }

    if (hidden != "rtt") {
        result += QString("-rtt-%1-%2").arg(rtt1).arg(rtt2);
    }

    if (hidden != "tcp") {
        result += QString("-tcp-%1-%2").arg(tcp1).arg(tcp2);
    }

    return result;
}

void CustomControls::autoGenerate(QList<QStringPair> tcpValues,
                                  QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues,
                                  QList<QInt32Pair> rttValues,
                                  QList<QRealStringRealStringTuple> sizeCongestionValues,
                                  QList<QStringRealRealRealRealTuple> qosValues,
                                  QString plotName,
                                  QString xAxis,
                                  int durationSec)
{
	if (mainWindow->editor->graph()->fileName != "quad.graph") {
		QMessageBox::critical(this, "Invalid operation", "This operation can only be applied to the toplogy quad.", QMessageBox::Ok);
		return;
	}

    autoReset(durationSec);

    QHash<QString, QStringList> plot2experiments;

    foreach (QStringPair tcp, tcpValues) {
        setTcp(tcp.first, tcp.second);
        foreach (QStringRealRealRealRealTuple qos, qosValues) {
            setNeutral();
            if (qos.first == "neutral") {
                setNeutral();
            } else if (qos.first == "policing") {
                setPolicing(qos.second, qos.third);
            } else if (qos.first == "shaping") {
                setShaping(qos.fourth, qos.fifth);
            } else if (qos.first == "policing-shaping") {
                setPolicing(qos.second, qos.third);
                setShaping(qos.fourth, qos.fifth);
            } else {
                ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
                continue;
            }
            foreach (QInt32Pair rtt, rttValues) {
                setRtt(rtt.first, rtt.second);
                foreach (QStringRealStringStringTuple buffersScalingQosScalingQdisc, buffersScalingQosScalingQdiscValues) {
                    setBuffer(buffersScalingQosScalingQdisc.first,
                              buffersScalingQosScalingQdisc.second,
                              buffersScalingQosScalingQdisc.third);
                    setQDisc(buffersScalingQosScalingQdisc.fourth);
                    foreach (QRealStringRealStringTuple sizeCongestion, sizeCongestionValues) {
                        setTransferSize(sizeCongestion.first, sizeCongestion.third,
                                        sizeCongestion.second, sizeCongestion.fourth,
                                        buffersScalingQosScalingQdisc.first, buffersScalingQosScalingQdisc.second);
                        QString expSuffix = autoGenerateSuffix("",
                                                               buffersScalingQosScalingQdisc.first,
                                                               buffersScalingQosScalingQdisc.second,
                                                               buffersScalingQosScalingQdisc.third,
                                                               buffersScalingQosScalingQdisc.fourth,
                                                               rtt.first, rtt.second,
                                                               tcp.first, tcp.second,
                                                               sizeCongestion.first, sizeCongestion.third,
                                                               sizeCongestion.second, sizeCongestion.fourth);
                        QString plotSuffix = QString("plot-") + plotName + "-" +
                                             autoGenerateSuffix(xAxis,
                                                                buffersScalingQosScalingQdisc.first,
                                                                buffersScalingQosScalingQdisc.second,
                                                                buffersScalingQosScalingQdisc.third,
                                                                buffersScalingQosScalingQdisc.fourth,
                                                                rtt.first, rtt.second,
                                                                tcp.first, tcp.second,
                                                                sizeCongestion.first, sizeCongestion.third,
                                                                sizeCongestion.second, sizeCongestion.fourth);
                        mainWindow->ui->txtBatchExpSuffix->setText(expSuffix);
                        mainWindow->on_btnEnqueue_clicked(true);
                        plot2experiments[plotSuffix].append(mainWindow->experimentQueue.last().workingDir);
                    }
                }
            }
        }
    }

    foreach (QString plot, plot2experiments.uniqueKeys()) {
        ui->txtAuto->appendPlainText(QString("mkdir '%1'").arg(plot));
        foreach (QString experiment, plot2experiments[plot]) {
            ui->txtAuto->appendPlainText(QString("cp -r '%1' '%2'").arg(experiment).arg(plot));
        }
        ui->txtAuto->appendPlainText("");
    }

    mainWindow->updateQueueTable();
}

void CustomControls::autoABase(QList<QStringRealRealRealRealTuple> &qosValues,
                               QList<QStringPair> &tcpValues,
                               QList<QStringRealStringStringTuple> &buffersScalingQosScalingQdiscValues)
{
    qosValues = QList<QStringRealRealRealRealTuple>()
                << QStringRealRealRealRealTuple("neutral", 0, 0, 0, 0);

    tcpValues = QList<QStringPair>()
                << QStringPair("cubic", "cubic")
                << QStringPair("reno", "reno");

    buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
                                          << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail")
                                          << QStringRealStringStringTuple("small", 1.0, "none", "drop-tail")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             ;
}

void CustomControls::autoBBase(QList<QStringRealRealRealRealTuple> &qosValues,
                               QList<QStringPair> &tcpValues,
                               QList<QStringRealStringStringTuple> &buffersScalingQosScalingQdiscValues)
{
    qosValues = QList<QStringRealRealRealRealTuple>()
                << QStringRealRealRealRealTuple("policing", 1.0, 0.3, 0, 0);

    tcpValues = QList<QStringPair>()
                << QStringPair("cubic", "cubic")
                << QStringPair("reno", "reno");

    buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
                                          << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail")
                                          << QStringRealStringStringTuple("small", 1.0, "none", "drop-tail")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             ;
}

void CustomControls::autoCBase(QList<QStringRealRealRealRealTuple> &qosValues,
                               QList<QStringPair> &tcpValues,
                               QList<QStringRealStringStringTuple> &buffersScalingQosScalingQdiscValues)
{
    qosValues = QList<QStringRealRealRealRealTuple>()
                << QStringRealRealRealRealTuple("shaping", 0, 0, 0.7, 0.3);

    tcpValues = QList<QStringPair>()
                << QStringPair("cubic", "cubic")
                << QStringPair("reno", "reno");

    buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
                                          << QStringRealStringStringTuple("large", 1.0, "down", "drop-tail")
                                          << QStringRealStringStringTuple("small", 1.0, "down", "drop-tail")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             ;
}

/////////////// PLOTS a

void CustomControls::on_btnAutoAaNeutralSizeGap_clicked()
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    autoABase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(1, "light", 10, "light")
                                                             << QRealStringRealStringTuple(1, "light", 40, "light")
                                                             << QRealStringRealStringTuple(1, "light", 9999, "light");

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 "Aa",
                 "transfer-size");
}

void CustomControls::autoBCaQosSize(bool shaping)
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    if (!shaping) {
        autoBBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    } else {
        autoCBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    }

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light");

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 !shaping ? "Ba" : "Ca",
                 "transfer-size");
}

void CustomControls::on_btnAutoBaPolicingSize_clicked()
{
    autoBCaQosSize(false);
}

void CustomControls::on_btnAutoCaShapingSize_clicked()
{
    autoBCaQosSize(true);
}

/////////////// PLOTS b

void CustomControls::on_btnAutoAbNeutralRttGap_clicked()
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    autoABase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50)
                                  << QInt32Pair(50, 80)
                                  << QInt32Pair(50, 120)
                                  << QInt32Pair(50, 200);

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 "Ab",
                 "rtt");
}

void CustomControls::autoBCbQosRtt(bool shaping)
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    if (!shaping) {
        autoBBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    } else {
        autoCBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    }

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50)
                                  << QInt32Pair(80, 80)
                                  << QInt32Pair(120, 120)
                                  << QInt32Pair(200, 200);

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 !shaping ? "Bb" : "Cb",
                 "rtt");
}

void CustomControls::on_btnAutoBbPolicingRtt_clicked()
{
    autoBCbQosRtt(false);
}

void CustomControls::on_btnAutoCbShapingRtt_clicked()
{
    autoBCbQosRtt(true);
}

/////////////// PLOTS c

void CustomControls::on_btnAutoAcNeutralCongestion_clicked()
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    autoABase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "none1", 1, "none1")
                                                             << QRealStringRealStringTuple(10, "none1", 10, "none1")
                                                             << QRealStringRealStringTuple(40, "none1", 40, "none1")
                                                             << QRealStringRealStringTuple(9999, "none1", 9999, "none1")
                                                             << QRealStringRealStringTuple(1, "none2", 1, "none2")
                                                             << QRealStringRealStringTuple(10, "none2", 10, "none2")
                                                             << QRealStringRealStringTuple(40, "none2", 40, "none2")
                                                             << QRealStringRealStringTuple(9999, "none2", 9999, "none2")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "none3")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "none3")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "none3")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "none3")
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light")
                                                             << QRealStringRealStringTuple(1, "medium", 1, "medium")
                                                             << QRealStringRealStringTuple(10, "medium", 10, "medium")
                                                             << QRealStringRealStringTuple(40, "medium", 40, "medium")
                                                             << QRealStringRealStringTuple(9999, "medium", 9999, "medium")
                                                             << QRealStringRealStringTuple(1, "high", 1, "high")
                                                             << QRealStringRealStringTuple(10, "high", 10, "high")
                                                             << QRealStringRealStringTuple(40, "high", 40, "high")
                                                             << QRealStringRealStringTuple(9999, "high", 9999, "high");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 "Ac",
                 "congestion");
}

void CustomControls::on_btnAutoAcNeutralCongestionGap_clicked()
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    autoABase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "none3", 1, "none1")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "none1")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "none1")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "none1")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "none2")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "none2")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "none2")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "none2")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "none3")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "none3")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "none3")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "none3")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "light")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "light")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "light")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "medium")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "medium")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "medium")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "medium")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "high")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "high")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "high")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "high");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 "Ac2",
                 "congestion");
}

void CustomControls::autoBCcQosCongestion(bool shaping)
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    if (!shaping) {
        autoBBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    } else {
        autoCBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    }

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "none1", 1, "none1")
                                                             << QRealStringRealStringTuple(10, "none1", 10, "none1")
                                                             << QRealStringRealStringTuple(40, "none1", 40, "none1")
                                                             << QRealStringRealStringTuple(9999, "none1", 9999, "none1")
                                                             << QRealStringRealStringTuple(1, "none2", 1, "none2")
                                                             << QRealStringRealStringTuple(10, "none2", 10, "none2")
                                                             << QRealStringRealStringTuple(40, "none2", 40, "none2")
                                                             << QRealStringRealStringTuple(9999, "none2", 9999, "none2")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "none3")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "none3")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "none3")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "none3")
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light")
                                                             << QRealStringRealStringTuple(1, "medium", 1, "medium")
                                                             << QRealStringRealStringTuple(10, "medium", 10, "medium")
                                                             << QRealStringRealStringTuple(40, "medium", 40, "medium")
                                                             << QRealStringRealStringTuple(9999, "medium", 9999, "medium")
                                                             << QRealStringRealStringTuple(1, "high", 1, "high")
                                                             << QRealStringRealStringTuple(10, "high", 10, "high")
                                                             << QRealStringRealStringTuple(40, "high", 40, "high")
                                                             << QRealStringRealStringTuple(9999, "high", 9999, "high");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 !shaping ? "Bc" : "Cc",
                 "congestion");
}

void CustomControls::on_btnAutoBcPolicingCongestion_clicked()
{
    autoBCcQosCongestion(false);
}

void CustomControls::on_btnAutoCcShapingCongestion_clicked()
{
    autoBCcQosCongestion(true);
}

/////////////// PLOTS d

void CustomControls::on_btnAutoAdNeutralTcp_clicked()
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    autoABase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    tcpValues = QList<QStringPair>()
                << QStringPair("cubic", "reno")
                << QStringPair("reno", "cubic");

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 "Ad",
                 "tcp");
}

void CustomControls::autoBCdQoSQoS(bool shaping)
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    if (!shaping) {
        autoBBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    } else {
        autoCBase(qosValues, tcpValues, buffersScalingQosScalingQdiscValues);
    }

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "light", 1, "light")
                                                             << QRealStringRealStringTuple(10, "light", 10, "light")
                                                             << QRealStringRealStringTuple(40, "light", 40, "light")
                                                             << QRealStringRealStringTuple(9999, "light", 9999, "light");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    tcpValues = QList<QStringPair>()
                << QStringPair("cubic", "cubic")
                << QStringPair("reno", "reno");

    if (!shaping) {
        qosValues = QList<QStringRealRealRealRealTuple>()
                    << QStringRealRealRealRealTuple("policing", 1.0, 0.5, 0, 0)
                    << QStringRealRealRealRealTuple("policing", 1.0, 0.4, 0, 0)
                    << QStringRealRealRealRealTuple("policing", 1.0, 0.3, 0, 0)
                    << QStringRealRealRealRealTuple("policing", 1.0, 0.2, 0, 0);
    } else {
        qosValues = QList<QStringRealRealRealRealTuple>()
                    << QStringRealRealRealRealTuple("shaping", 0, 0, 0.5, 0.5)
                    << QStringRealRealRealRealTuple("shaping", 0, 0, 0.6, 0.4)
                    << QStringRealRealRealRealTuple("shaping", 0, 0, 0.7, 0.3)
                    << QStringRealRealRealRealTuple("shaping", 0, 0, 0.8, 0.2);
    }

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 !shaping ? "Bd" : "Cd",
                 "qos");
}

void CustomControls::on_btnAutoBdPolicingQoS_clicked()
{
    autoBCdQoSQoS(false);
}

void CustomControls::on_btnAutoCdShapingQoS_clicked()
{
    autoBCdQoSQoS(true);
}

void CustomControls::on_btnAutoCalibrateNone_clicked()
{
    QList<QStringRealRealRealRealTuple> qosValues;
    QList<QStringPair> tcpValues;
    QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

    qosValues = QList<QStringRealRealRealRealTuple>()
                << QStringRealRealRealRealTuple("policing", 1.0, 0.3, 0, 0)
                << QStringRealRealRealRealTuple("shaping", 0, 0, 0.7, 0.3)
                << QStringRealRealRealRealTuple("neutral", 0, 0, 0, 0);

    tcpValues = QList<QStringPair>()
                << QStringPair("cubic", "cubic");

    buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
                                          << QStringRealStringStringTuple("large", 1.0, "down", "drop-tail")
                                          << QStringRealStringStringTuple("small", 1.0, "down", "drop-tail")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             // TODO implement this << QStringPair("large", 1.0, "none", "wred")
                                             ;

    QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
                                                             << QRealStringRealStringTuple(1, "none1", 1, "none1")
                                                             << QRealStringRealStringTuple(10, "none1", 10, "none1")
                                                             << QRealStringRealStringTuple(40, "none1", 40, "none1")
                                                             << QRealStringRealStringTuple(9999, "none1", 9999, "none1")
                                                             << QRealStringRealStringTuple(1, "none2", 1, "none2")
                                                             << QRealStringRealStringTuple(10, "none2", 10, "none2")
                                                             << QRealStringRealStringTuple(40, "none2", 40, "none2")
                                                             << QRealStringRealStringTuple(9999, "none2", 9999, "none2")
                                                             << QRealStringRealStringTuple(1, "none3", 1, "none3")
                                                             << QRealStringRealStringTuple(10, "none3", 10, "none3")
                                                             << QRealStringRealStringTuple(40, "none3", 40, "none3")
                                                             << QRealStringRealStringTuple(9999, "none3", 9999, "none3");

    QList<QInt32Pair> rttValues = QList<QInt32Pair>()
                                  << QInt32Pair(50, 50);

    autoGenerate(tcpValues,
                 buffersScalingQosScalingQdiscValues,
                 rttValues,
                 sizeCongestionValues,
                 qosValues,
                 "CalibrateNone",
                 "congestion",
                 90);
}

/////////////// PLOTS D

// Node indices use natural ordering.
// The traffic class uses natural ordering.
void CustomControls::createConnection(NetGraph *g, qint32 n1, qint32 n2, QString traffic, int trafficClass)
{
    QStringList trafficTypes = traffic.split("+");

    foreach (QString trafficType, trafficTypes) {
        QStringList parts = trafficType.split(" x ");
        QString basicType = parts.first().trimmed();
        int multiplier = 1;
        if (parts.count() == 2) {
            bool ok;
            multiplier = parts[1].toInt(&ok);
            if (!ok) {
                ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
                continue;
            }
        } else if (parts.count() > 2) {
            ui->txtAuto->appendPlainText(QString("Warning: %1:%2").arg(__FILE__).arg(__LINE__));
            continue;
        }
        if (basicType == "file-transfer") {
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 9999Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier),
                                                QByteArray()));
        } else if (basicType == "web") {
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 1Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier * 8),
                                                QByteArray()));
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 10Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier * 2),
                                                QByteArray()));
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 40Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier * 1),
                                                QByteArray()));
        } else if (basicType == "1Mb") {
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 1Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier),
                                                QByteArray()));
        } else if (basicType == "10Mb") {
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 10Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier),
                                                QByteArray()));
        } else if (basicType == "40Mb") {
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 40Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier),
                                                QByteArray()));
        } else if (basicType == "9999Mb") {
            g->addConnection(NetGraphConnection(n1 - 1,
                                                n2 - 1,
                                                QString("TCP-Poisson-Pareto 0.1 2.0 9999Mb sequential class %1 x %2")
                                                .arg(trafficClass - 1)
                                                .arg(multiplier),
                                                QByteArray()));
        } else if (basicType.startsWith("dash-video")) {
            parts = basicType.split(" ", QString::SkipEmptyParts);
            if (parts.count() != 2) {
                ui->txtAuto->appendPlainText(QString("Warning: %1:%2 %3").arg(__FILE__).arg(__LINE__).arg(basicType));
            } else {
                bool ok;
                qreal speed = parts[1].replace("Mbps", "").toDouble(&ok);
                if (!ok || speed <= 0) {
                    ui->txtAuto->appendPlainText(QString("Warning: %1:%2 %3").arg(__FILE__).arg(__LINE__).arg(basicType));
                } else {
                    g->addConnection(NetGraphConnection(n1 - 1,
                                                        n2 - 1,
                                                        QString("TCP-DASH %1 10 60 5 class %2 x %3")
                                                        .arg(speed)
                                                        .arg(trafficClass - 1)
                                                        .arg(multiplier),
                                                        QByteArray()));
                }
            }
        } else {
            ui->txtAuto->appendPlainText(QString("Warning: %1:%2 %3").arg(__FILE__).arg(__LINE__).arg(basicType));
        }
    }
}

// Configures one experiment
// Uses natural link indices
void CustomControls::autoD(QString traffic1,
                           QString traffic2,
                           QString background1,
                           QString background2,
                           QInt32List nonNeutralLinks)
{
    if (mainWindow->editor->graph()->fileName != "pegasus.graph") {
        QMessageBox::critical(this, "Invalid operation", "This operation can only be applied to the toplogy pegasus.", QMessageBox::Ok);
        return;
    }

    QList<QString> domains;

    // Indices given in natural ordering
    QHash<QString, QList<qint32> > vantagePoints1;
    QHash<QString, QList<qint32> > vantagePoints2;
    QHash<QString, QList<qint32> > backgroundPoints1;
    QHash<QString, QList<qint32> > backgroundPoints2;

    vantagePoints1["A"] = QList<qint32>() << 13 << 14;
    vantagePoints2["A"] = QList<qint32>() << 15 << 16;
    backgroundPoints1["A"] = QList<qint32>() << 29;
    backgroundPoints2["A"] = QList<qint32>() << 30;
    domains << "A";

    vantagePoints1["B"] = QList<qint32>() << 17 << 18;
    vantagePoints2["B"] = QList<qint32>() << 19 << 20;
    backgroundPoints1["B"] = QList<qint32>();
    backgroundPoints2["B"] = QList<qint32>();
    domains << "B";

    vantagePoints1["C"] = QList<qint32>() << 21 << 22;
    vantagePoints2["C"] = QList<qint32>() << 23 << 24;
    backgroundPoints1["C"] = QList<qint32>() << 31;
    backgroundPoints2["C"] = QList<qint32>() << 32;
    domains << "C";

    vantagePoints1["D"] = QList<qint32>() << 25 << 26;
    vantagePoints2["D"] = QList<qint32>() << 27 << 28;
    backgroundPoints1["D"] = QList<qint32>() << 33;
    backgroundPoints2["D"] = QList<qint32>() << 34;
    domains << "D";

    NetGraph *g = mainWindow->editor->graph();
    while (!g->connections.isEmpty()) {
        g->deleteConnection(g->connections.count() - 1);
    }

    for (qint32 e = 0; e < g->edges.count(); e++) {
        g->edges[e].queueCount = 1;
        g->edges[e].queueWeights = (QList<qreal>() << 1.0).toVector();
        g->edges[e].policerCount = 1;
        g->edges[e].policerWeights = (QList<qreal>() << 1.0).toVector();
    }

    foreach (qint32 e, nonNeutralLinks) {
        // natural indices!!!
        g->edges[e - 1].policerCount = 2;
        g->edges[e - 1].policerWeights = (QList<qreal>() << 1.0 << 0.3).toVector();
    }

    foreach (QString domain1, domains) {
        foreach (QString domain2, domains) {
            if (domain1 == domain2)
                continue;
            if (!traffic1.isEmpty()) {
                for (int i = 0; i < qMin(vantagePoints1[domain1].count(), vantagePoints1[domain2].count()); i++) {
                    qint32 u1 = vantagePoints1[domain1][i];
                    qint32 v1 = vantagePoints1[domain2][i];
                    createConnection(g, u1, v1, traffic1, 1);
                }
            }
            if (!traffic2.isEmpty()) {
                for (int i = 0; i < qMin(vantagePoints2[domain1].count(), vantagePoints2[domain2].count()); i++) {
                    qint32 u2 = vantagePoints2[domain1][i];
                    qint32 v2 = vantagePoints2[domain2][i];
                    createConnection(g, u2, v2, traffic2, 2);
                }
            }
            if (!background1.isEmpty()) {
                for (int i = 0; i < qMin(backgroundPoints1[domain1].count(), backgroundPoints1[domain2].count()); i++) {
                    qint32 s1 = backgroundPoints1[domain1][i];
                    qint32 t1 = backgroundPoints1[domain2][i];
                    createConnection(g, s1, t1, background1, 1);
                }
            }
            if (!background2.isEmpty()) {
                for (int i = 0; i < qMin(backgroundPoints2[domain1].count(), backgroundPoints2[domain2].count()); i++) {
                    qint32 s2 = backgroundPoints2[domain1][i];
                    qint32 t2 = backgroundPoints2[domain2][i];
                    createConnection(g, s2, t2, background2, 2);
                }
            }
        }
    }
    mainWindow->editor->reloadScene();
    mainWindow->editor->setModified();
}

// Uses natural link indices
void CustomControls::autoDGenerate(QList<QStringQuad> trafficValues,
                                   QList<QInt32List> nonNeutralLinksValues,
                                   QString plotSuffix)
{
    QHash<QString, QStringList> plot2experiments;
    foreach (QInt32List nonNeutralLinks, nonNeutralLinksValues) {
        QString nonNeutralLinksTag = "non-neutral-links";
        if (nonNeutralLinks.isEmpty()) {
            nonNeutralLinksTag += "-none";
        } else {
            foreach (qint32 link, nonNeutralLinks) {
                nonNeutralLinksTag += QString("-%1").arg(link);
            }
        }
        foreach (QStringQuad traffic, trafficValues) {
            autoD(traffic.first,
                  traffic.second,
                  traffic.third,
                  traffic.fourth,
                  nonNeutralLinks);
            QString expSuffix = QString("traffic-%1-%2-%3-%4-%5-%6")
                                .arg(plotSuffix)
                                .arg(traffic.first)
                                .arg(traffic.second)
                                .arg(traffic.third)
                                .arg(traffic.fourth)
                                .arg(nonNeutralLinksTag);
            expSuffix.replace(" ", "");
            expSuffix.replace("+", "-");
            mainWindow->ui->txtBatchExpSuffix->setText(expSuffix);
            mainWindow->on_btnEnqueue_clicked(true);
            plot2experiments[plotSuffix].append(mainWindow->experimentQueue.last().workingDir);
        }
    }

    foreach (QString plot, plot2experiments.uniqueKeys()) {
        ui->txtAuto->appendPlainText(QString("mkdir '%1'").arg(plot));
        foreach (QString experiment, plot2experiments[plot]) {
            ui->txtAuto->appendPlainText(QString("cp -r '%1' '%2'").arg(experiment).arg(plot));
        }
        ui->txtAuto->appendPlainText("");
    }

    mainWindow->updateQueueTable();
}

void CustomControls::on_btnAutoDaFile_clicked()
{
    QList<QStringQuad> trafficValues = QList<QStringQuad>()
                                       << QStringQuad("file-transfer x 1", "file-transfer x 1", "", "")
                                       << QStringQuad("file-transfer x 5", "file-transfer x 5", "", "")
                                       << QStringQuad("file-transfer x 7", "file-transfer x 7", "", "")
                                       << QStringQuad("file-transfer x 10", "file-transfer x 10", "", "")
                                          ;
    QList<QInt32List> nonNeutralLinkValues = QList<QInt32List>()
                                             << (QInt32List())
                                             << (QInt32List() << 5 << 14 << 20);

    autoDGenerate(trafficValues, nonNeutralLinkValues, "Da");
}

void CustomControls::on_btnAutoDbWeb_clicked()
{
    QList<QStringQuad> trafficValues = QList<QStringQuad>()
                                       << QStringQuad("web x 1", "web x 1", "", "")
                                       << QStringQuad("web x 2", "web x 2", "", "")
                                       << QStringQuad("web x 3", "web x 3", "", "")
                                                ;
    QList<QInt32List> nonNeutralLinkValues = QList<QInt32List>()
                                             << (QInt32List())
                                             << (QInt32List() << 5 << 14 << 20);

    autoDGenerate(trafficValues, nonNeutralLinkValues, "Db");
}

void CustomControls::on_btnAutoDcWebFile_clicked()
{
    QList<QStringQuad> trafficValues = QList<QStringQuad>()
                                       << QStringQuad("1Mb x 1 + 10Mb x 1 + 40Mb x 1", "file-transfer x 1", "", "")
                                       << QStringQuad("1Mb x 1 + 10Mb x 1 + 40Mb x 1", "file-transfer x 1",
                                                      "1Mb x 4 + 10Mb x 2 + 40Mb x 1 + file-transfer x 1", "file-transfer x 1")
												;
    QList<QInt32List> nonNeutralLinkValues = QList<QInt32List>()
                                             << (QInt32List())
                                             << (QInt32List() << 5 << 14 << 20);

    autoDGenerate(trafficValues, nonNeutralLinkValues, "Dc");
}

void CustomControls::on_btnAutoDdWebVideo_clicked()
{
    QList<QStringQuad> trafficValues = QList<QStringQuad>()
									   //<< QStringQuad("1Mb x 1 + 10Mb x 1 + 40Mb x 1", "dash-video 2Mbps x 1", "", "")
									   //<< QStringQuad("1Mb x 30 + 10Mb x 10 + 40Mb x 4", "dash-video 2Mbps x 1", "", "")
									   //<< QStringQuad("9999Mb x 1", "dash-video 2Mbps x 1", "", "")
									   //<< QStringQuad("dash-video 2Mbps x 1", "dash-video 2Mbps x 1", "", "")
									   << QStringQuad("dash-video 2Mbps x 5", "dash-video 2Mbps x 5", "file-transfer", "file-transfer")
										  ;
    QList<QInt32List> nonNeutralLinkValues = QList<QInt32List>()
                                             //<< (QInt32List())
                                             << (QInt32List() << 5 << 14 << 20);

    autoDGenerate(trafficValues, nonNeutralLinkValues, "Dd");
}

void CustomControls::loadGraph(QString name)
{
	for (int i = 0; i < mainWindow->ui->cmbTopologies->count(); i++) {
		if (mainWindow->ui->cmbTopologies->itemText(i) == name) {
			mainWindow->ui->cmbTopologies->setCurrentIndex(i);
			break;
		}
	}
	QApplication::processEvents();
	Q_ASSERT_FORCE(mainWindow->ui->cmbTopologies->currentText() == name);
}

void CustomControls::on_btnSigcomm7a_clicked()
{
	loadGraph("quad");
	mainWindow->ui->txtIntervalSize->setText("50ms");
	QApplication::processEvents();

	int duration = 600; // seconds

	QList<QStringRealRealRealRealTuple> qosValues;
	qosValues << QStringRealRealRealRealTuple("neutral", 0, 0, 0, 0);

	QList<QStringPair> tcpValues;
	tcpValues << QStringPair("cubic", "cubic");

	QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;
	buffersScalingQosScalingQdiscValues << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail");

	QList<QInt32Pair> rttValues;
	rttValues << QInt32Pair(50, 50);

	QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
															 << QRealStringRealStringTuple(1, "light", 1, "light")
															 << QRealStringRealStringTuple(1, "light", 10, "light")
															 << QRealStringRealStringTuple(1, "light", 40, "light")
															 << QRealStringRealStringTuple(1, "light", 9999, "light");

	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7a",
				 "transfer-size",
				 duration);
}

void CustomControls::on_btnSigcomm7b_clicked()
{
	loadGraph("quad");
	mainWindow->ui->txtIntervalSize->setText("50ms");
	QApplication::processEvents();

	int duration = 600; // seconds

	QList<QStringRealRealRealRealTuple> qosValues;
	qosValues << QStringRealRealRealRealTuple("neutral", 0, 0, 0, 0);

	QList<QStringPair> tcpValues;
	tcpValues << QStringPair("cubic", "cubic");

	QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;
	buffersScalingQosScalingQdiscValues << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail");

	QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
															 << QRealStringRealStringTuple(10, "light", 10, "light");

	QList<QInt32Pair> rttValues = QList<QInt32Pair>()
								  << QInt32Pair(50, 50)
								  << QInt32Pair(50, 80)
								  << QInt32Pair(50, 120)
								  << QInt32Pair(50, 200);

	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7b",
				 "rtt",
				 duration);
}

void CustomControls::on_btnSigcomm7c_clicked()
{
	loadGraph("quad");
	mainWindow->ui->txtIntervalSize->setText("50ms");
	QApplication::processEvents();

	int duration = 600; // seconds

	QList<QStringRealRealRealRealTuple> qosValues;
	qosValues << QStringRealRealRealRealTuple("neutral", 0, 0, 0, 0);

	QList<QStringPair> tcpValues;

	QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;
	buffersScalingQosScalingQdiscValues << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail");

	QList<QRealStringRealStringTuple> sizeCongestionValues;

	QList<QInt32Pair> rttValues = QList<QInt32Pair>()
								  << QInt32Pair(50, 50);

	// Left side:
	tcpValues = QList<QStringPair>()
				<< QStringPair("cubic", "cubic");
	// Note: in the paper we used 1 vs 10 by mistake
	sizeCongestionValues = QList<QRealStringRealStringTuple>()
						   << QRealStringRealStringTuple(10, "light", 10, "light");
	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7c",
				 "congestion",
				 duration);

	// Right side:
	tcpValues = QList<QStringPair>()
				<< QStringPair("cubic", "reno");
	sizeCongestionValues = QList<QRealStringRealStringTuple>()
						   << QRealStringRealStringTuple(10, "light", 10, "light");
	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7c",
				 "congestion",
				 duration);
}

void CustomControls::on_btnSigcomm7d_clicked()
{
	loadGraph("quad");
	mainWindow->ui->txtIntervalSize->setText("50ms");
	QApplication::processEvents();

	int duration = 600; // seconds

	QList<QStringRealRealRealRealTuple> qosValues;
	QList<QStringPair> tcpValues;
	QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

	qosValues = QList<QStringRealRealRealRealTuple>()
				<< QStringRealRealRealRealTuple("policing", 1.0, 0.3, 0, 0);

	tcpValues = QList<QStringPair>()
				<< QStringPair("cubic", "cubic");

	buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
										  << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail");

	QList<QInt32Pair> rttValues = QList<QInt32Pair>()
								  << QInt32Pair(50, 50);

	QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
															 << QRealStringRealStringTuple(1, "light", 1, "light")
															 << QRealStringRealStringTuple(10, "light", 10, "light")
															 << QRealStringRealStringTuple(40, "light", 40, "light")
															 << QRealStringRealStringTuple(9999, "light", 9999, "light");

	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7d",
				 "transfer-size",
				 duration);
}

void CustomControls::on_btnSigcomm7e_clicked()
{
	loadGraph("quad");
	mainWindow->ui->txtIntervalSize->setText("50ms");
	QApplication::processEvents();

	int duration = 600; // seconds

	QList<QStringRealRealRealRealTuple> qosValues;
	QList<QStringPair> tcpValues;
	QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

	qosValues = QList<QStringRealRealRealRealTuple>()
				<< QStringRealRealRealRealTuple("policing", 1.0, 0.3, 0, 0);

	tcpValues = QList<QStringPair>()
				<< QStringPair("cubic", "cubic");

	buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
										  << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail");

	QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
															 << QRealStringRealStringTuple(10, "light", 10, "light");

	QList<QInt32Pair> rttValues = QList<QInt32Pair>()
								  << QInt32Pair(50, 50)
								  << QInt32Pair(80, 80)
								  << QInt32Pair(120, 120)
								  << QInt32Pair(200, 200);

	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7e",
				 "rtt",
				 duration);
}

void CustomControls::on_btnSigcomm7f_clicked()
{
	loadGraph("quad");
	mainWindow->ui->txtIntervalSize->setText("50ms");
	QApplication::processEvents();

	int duration = 600; // seconds

	QList<QStringRealRealRealRealTuple> qosValues;
	QList<QStringPair> tcpValues;
	QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues;

	tcpValues = QList<QStringPair>()
				<< QStringPair("cubic", "cubic");

	buffersScalingQosScalingQdiscValues = QList<QStringRealStringStringTuple>()
										  << QStringRealStringStringTuple("large", 1.0, "none", "drop-tail");

	QList<QRealStringRealStringTuple> sizeCongestionValues = QList<QRealStringRealStringTuple>()
															 << QRealStringRealStringTuple(10, "light", 10, "light");

	QList<QInt32Pair> rttValues = QList<QInt32Pair>()
								  << QInt32Pair(50, 50);

	qosValues = QList<QStringRealRealRealRealTuple>()
				<< QStringRealRealRealRealTuple("policing", 1.0, 0.5, 0, 0)
				<< QStringRealRealRealRealTuple("policing", 1.0, 0.4, 0, 0)
				<< QStringRealRealRealRealTuple("policing", 1.0, 0.3, 0, 0)
				<< QStringRealRealRealRealTuple("policing", 1.0, 0.2, 0, 0);

	autoGenerate(tcpValues,
				 buffersScalingQosScalingQdiscValues,
				 rttValues,
				 sizeCongestionValues,
				 qosValues,
				 "7f",
				 "qos",
				 duration);
}

void CustomControls::on_btnSigcomm8ab_clicked()
{
	int duration = 600; // seconds

	loadGraph("pegasus");
	mainWindow->ui->txtGenericTimeout->setText(QString("%1s").arg(duration));
	mainWindow->ui->txtIntervalSize->setText("100ms");
	QApplication::processEvents();

	QList<QStringQuad> trafficValues = QList<QStringQuad>()
									   << QStringQuad("1Mb x 1 + 10Mb x 1 + 40Mb x 1",  // class 1
													  "file-transfer x 1",  // class 2
													  "1Mb x 4 + 10Mb x 2 + 40Mb x 1 + file-transfer x 1",  // class 1 background
													  "file-transfer x 1"  // class 2 background
													  );
	QList<QInt32List> nonNeutralLinkValues = QList<QInt32List>()
											 << (QInt32List())
											 << (QInt32List() << 5 << 14 << 20);

	autoDGenerate(trafficValues, nonNeutralLinkValues, "8ab");
}
