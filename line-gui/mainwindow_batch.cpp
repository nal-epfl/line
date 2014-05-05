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

#include "line-record_processor.h"

#include "chronometer.h"

bool MainWindow::getRunParams(RunParams &params, QPlainTextEdit *log)
{
	bool accepted = true;
	quint64 duration = 0;
	if (ui->checkGenericTimeout->isChecked()) {
		updateTimeBox(ui->txtGenericTimeout, accepted, duration);
		duration /= 1000000000ULL;
	}
	quint64 intervalSize = 5000000000ULL;
	updateTimeBox(ui->txtIntervalSize, accepted, intervalSize);

	if (!accepted) {
		doLogError(log, "Invalid duration value.");
		return false;
	}

	params = RunParams(ui->txtBatchExpSuffix->text(),
					   ui->checkCapture->isChecked(),
					   ui->spinCapturePacketCount->value(),
					   ui->spinCaptureEventCount->value(),
					   intervalSize,
					   duration * 1000000000ULL,
					   ui->spinBufferBloadFactor->value(),
					   ui->cmbBufferSize->currentText(),
					   ui->cmbQoSBufferScaling->currentText(),
					   ui->cmbQueueingDiscipline->currentText(),
					   ui->checkReadjustQueues->isChecked(),
					   ui->checkTcpReceiveWin->isChecked(),
					   ui->checkTcpReceiveWin->isChecked() && ui->checkTcpReceiveWinFixed->isChecked(),
					   ui->spinTcpReceiveWinFixed->value() * 1024,
					   ui->checkTcpReceiveWin->isChecked() && ui->checkTcpReceiveWinAuto->isChecked(),
					   ui->spinTcpReceiveWinScaleFactor->value(),
					   qCompress(editor->graph()->saveToByteArray()),
					   getGraphName(editor->graph()),
					   QDir(".").absolutePath(),
					   ui->spinRepeats->value(),
                       ui->checkSampleTimelinesEverywhere->isChecked(),
                       ui->checkFlowTracking->isChecked(),
                       ui->cmbExpType->currentText() == "simulation",
                       ui->cmbExpType->currentText() == "real network",
					   getClientHostname(),
					   getClientPort(),
					   getCoreHostname(),
					   getCorePort(),
                       QList<QString>(),
                       QList<QString>(),
					   ui->spinFakeCLFraction->value() / 100.0,
					   ui->spinFakeCLProbMin->value() / 100.0,
					   ui->spinFakeCLProbMax->value() / 100.0,
					   ui->spinFakeGLProbMin->value() / 100.0,
					   ui->spinFakeGLProbMax->value() / 100.0,
					   ui->spinFakeCLLossMin->value() / 100.0,
					   ui->spinFakeCLLossMax->value() / 100.0,
					   ui->spinFakeGLLossMin->value() / 100.0,
					   ui->spinFakeGLLossMax->value() / 100.0,
					   ui->spinFakeCLLossNoise->value() / 100.0,
					   ui->spinFakeGLLossNoise->value() / 100.0);
	return true;
}

void MainWindow::on_cmbExpType_currentIndexChanged(const QString &)
{
    ui->boxSimParams->setVisible(ui->cmbExpType->currentText() == "simulation");
    ui->scrollAreaWidgetContents->adjustSize();
    QApplication::processEvents();
    ui->scrollArea->setMaximumHeight(ui->scrollAreaWidgetContents->height() + 24);
}

void MainWindow::setUIToRunParams(const RunParams &params)
{
	ui->checkGenericTimeout->setChecked(true);
	ui->txtGenericTimeout->setText(timeToString(params.estimatedDuration));
	ui->txtIntervalSize->setText(timeToString(params.intervalSize));
	ui->txtBatchExpSuffix->setText(params.experimentSuffix);
	ui->checkCapture->setChecked(params.capture);
	ui->spinCapturePacketCount->setValue(params.capturePacketLimit);
	ui->spinCaptureEventCount->setValue(params.captureEventLimit);
	ui->spinBufferBloadFactor->setValue(params.bufferBloatFactor);
	setCurrentItem(ui->cmbBufferSize, params.bufferSize);
	setCurrentItem(ui->cmbQoSBufferScaling, params.qosBufferScaling);
	setCurrentItem(ui->cmbQueueingDiscipline, params.queuingDiscipline);
	ui->checkReadjustQueues->setChecked(params.readjustQueues);
	ui->checkTcpReceiveWin->setChecked(params.setTcpReceiveWindowSize);
	ui->checkTcpReceiveWinFixed->setChecked(params.setFixedTcpReceiveWindowSize);
	ui->spinTcpReceiveWinFixed->setValue(params.fixedTcpReceiveWindowSize / 1024);
	ui->checkTcpReceiveWinAuto->setChecked(params.setAutoTcpReceiveWindowSize);
	ui->spinTcpReceiveWinScaleFactor->setValue(params.scaleAutoTcpReceiveWindowSize);
	ui->spinRepeats->setValue(params.repeats);
    setCurrentItem(ui->cmbExpType, params.fakeEmulation ?
                       "simulation" :
                       params.realRouting ?
                           "real netowrk" :
                           "emulation");
	ui->spinFakeCLFraction->setValue(params.congestedLinkFraction * 100.0);
	ui->spinFakeCLProbMin->setValue(params.minProbCongestedLinkIsCongested * 100.0);
	ui->spinFakeCLProbMax->setValue(params.maxProbCongestedLinkIsCongested * 100.0);
	ui->spinFakeGLProbMin->setValue(params.minProbGoodLinkIsCongested * 100.0);
	ui->spinFakeGLProbMax->setValue(params.maxProbGoodLinkIsCongested * 100.0);
	ui->spinFakeCLLossMin->setValue(params.minLossRateOfCongestedLink * 100.0);
	ui->spinFakeCLLossMax->setValue(params.maxLossRateOfCongestedLink * 100.0);
	ui->spinFakeGLLossMin->setValue(params.minLossRateOfGoodLink * 100.0);
	ui->spinFakeGLLossMax->setValue(params.maxLossRateOfGoodLink * 100.0);
	ui->spinFakeCLLossNoise->setValue(params.congestedRateNoise * 100.0);
	ui->spinFakeGLLossNoise->setValue(params.goodRateNoise * 100.0);
}

///// Experiment queue handling
void MainWindow::on_btnEnqueue_clicked(bool automatic)
{
	RunParams params;
	if (!getRunParams(params, ui->txtBatch)) {
		emit logError(ui->txtExperimentQueue, QString("Problem on %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(__FUNCTION__));
		return;
	}
	for (int i = 0; i < ui->spinRepeats->value(); i++) {
		QString testId = params.graphName +
						 QDateTime::currentDateTime().toString(".yyyy.MM.dd.hh.mm.ss") +
						 QString(".%1.%2").arg(getCurrentTimeNanosec()).arg(rand()) + "-" +
						 ui->txtBatchExpSuffix->text();
		params.workingDir = QDir(".").absolutePath() + "/" + testId;
		experimentQueue << params;
		emit logInformation(ui->txtExperimentQueue, QString("Experiment added to the queue: %1")
							.arg(params.experimentSuffix));
	}
	if (!automatic) {
		updateQueueTable();
	}
}

void MainWindow::on_btnEnqueueAndStart_clicked()
{
	on_btnEnqueue_clicked();
	on_btnQueueStartAll_clicked();
}

void MainWindow::updateQueueTable()
{
	ui->tableExperimentQueue->clearContents();
	ui->tableExperimentQueue->setRowCount(0);
	foreach (RunParams params, experimentQueue) {
		ui->tableExperimentQueue->setRowCount(ui->tableExperimentQueue->rowCount() + 1);

		QTableWidgetItem *cell;
		int col = 0;

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, params.graphName);
		cell->setData(Qt::DisplayRole, params.graphName);
		ui->tableExperimentQueue->setItem(ui->tableExperimentQueue->rowCount() - 1, col, cell);
		col++;

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, params.experimentSuffix);
		cell->setData(Qt::DisplayRole, params.experimentSuffix);
		ui->tableExperimentQueue->setItem(ui->tableExperimentQueue->rowCount() - 1, col, cell);
		col++;

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, params.estimatedDuration * 1.0e-9);
		cell->setData(Qt::DisplayRole, params.estimatedDuration * 1.0e-9);
		ui->tableExperimentQueue->setItem(ui->tableExperimentQueue->rowCount() - 1, col, cell);
		col++;

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, params.intervalSize * 1.0e-9);
		cell->setData(Qt::DisplayRole, params.intervalSize * 1.0e-9);
		ui->tableExperimentQueue->setItem(ui->tableExperimentQueue->rowCount() - 1, col, cell);
		col++;
	}
	ui->tableExperimentQueue->resizeColumnsToContents();
	emit tabChanged(ui->tabQueue);
	qreal totalDuration = 0;
	foreach (RunParams p, experimentQueue) {
		totalDuration += p.estimatedDuration * 1.0e-9 + 250;
	}
	emit logInformation(ui->txtExperimentQueue, QString("Queue duration: %1 seconds. ETA: %2").
						arg(totalDuration).
						arg(QDateTime::currentDateTime().addSecs(totalDuration).toString()));
    ui->btnEnqueueAndStart->setEnabled(experimentQueue.isEmpty());
}

void MainWindow::on_btnQueueClear_clicked()
{
	experimentQueue.clear();
	saveExperimentQueue();
	updateQueueTable();
	emit logInformation(ui->txtExperimentQueue, QString("All experiments removed from the queue."));
}

void MainWindow::on_btnQueueRemove_clicked()
{
	QList<int> selectedIndices;
	foreach (QTableWidgetSelectionRange range, ui->tableExperimentQueue->selectedRanges()) {
		for (int i = range.topRow(); i <= range.bottomRow(); i++) {
			selectedIndices << i;
		}
	}
	qSort(selectedIndices);
	while (!selectedIndices.isEmpty()) {
		int i = selectedIndices.takeLast();
		if (i >= 0 && i < experimentQueue.count()) {
			experimentQueue.removeAt(i);
		}
		ui->tableExperimentQueue->removeRow(i);
	}
}

void MainWindow::on_btnQueueStartAll_clicked()
{
	if (ui->cmbSimulation->currentIndex() != 0) {
		ui->cmbSimulation->setCurrentIndex(0);
		 QTimer::singleShot(1000, this, SLOT(on_btnQueueStartAll_clicked()));
		 return;
	}

	runningInQueue = true;
	QString testId;
	while (!mustStop) {
		if (experimentQueue.isEmpty()) {
			emit logInformation(ui->txtExperimentQueue, QString("Experiment queue is empty."));
			runningInQueue = false;
			emit experimentQueueFinished();
			break;
		}

        if (ui->checkQueuePaused->isChecked()) {
            QApplication::processEvents();
            usleep(50 * 1000);
            blinkTaskbar();
            continue;
        }

		RunParams params = experimentQueue.takeFirst();
		QString paramsFileName = QString("run-params-%1.data").arg(getCurrentTimeNanosec());
		saveRunParams(params, paramsFileName);

		if (params.fakeEmulation) {
			if (experimentQueue.count() % 100 == 0) {
				saveExperimentQueue();
				updateQueueTable();
			}
		} else {
			saveExperimentQueue();
			updateQueueTable();
		}

		ui->txtBatch->clear();

		testId = params.workingDir.split('/', QString::SkipEmptyParts).last();

		// Start the computation.
		blockingOperationStarting();
		{
			if (lineRunner(QStringList()
						   << "--log-dir" << testId
						   << "--run" << paramsFileName,
						   ui->txtBatch)) {
				emit logSuccess(ui->txtBatch, "Experiment finished successfully.");
			} else {
				emit logError(ui->txtBatch, "line-runner failed!");
			}
		}
		onBtnGenericFinished(testId);
        blockingOperationFinished(false);

		QDir(".").remove(paramsFileName);

		QApplication::processEvents();
	}
    blinkTaskbar();
	emit reloadSimulationList(testId);
	emit experimentQueueFinished();
}

void MainWindow::on_btnQueueStop_clicked()
{
	mustStop = true;
}

/////

void MainWindow::onBtnGenericFinished(QString testId)
{
	QString reason;
	if (!experimentOk(testId, reason)) {
		emit logError(ui->txtBatch, reason);
		emit logError(ui->txtExperimentQueue, QString("Experiment failed: %1").arg(reason));
	} else {
        if (ui->checkCapture->isChecked()) {
			safeLogClear(ui->txtCapture);
			startRedirectingQDebug(ui->txtCapture);
            int packetCount, queueEventCount;
			if (!processCapture(testId, packetCount, queueEventCount)) {
                QMessageBox::critical(this, "Processing capture failed!", "See console output for details.");
            } else {
                if (packetCount >= ui->spinCapturePacketCount->value() || queueEventCount >= ui->spinCaptureEventCount->value()) {
                    QMessageBox::warning(this, "Capture overflowed", QString("Capture overflowed (packet/event limit reached). Pcap files are probably incomplete.\n"
                                                                             "Packet count: %1 out of %2\n"
                                                                             "Queue events count: %3 out of %4\n").arg(packetCount).arg(ui->spinCapturePacketCount->value()).
                                         arg(queueEventCount).arg(ui->spinCaptureEventCount->value()));
                } else {
                    emit logInformation(ui->txtBatch, QString("Capture processing:\n"
                                                              "Packet count: %1 out of %2\n"
                                                              "Queue events count: %3 out of %4\n").arg(packetCount).arg(ui->spinCapturePacketCount->value()).
                          arg(queueEventCount).arg(ui->spinCaptureEventCount->value()));
                }
            }
			stopRedirectingQDebug();
        } else {
            emit logInformation(ui->txtBatch, QString("No capture processing (disabled).\n"));
        }
	}
}

void MainWindow::on_checkGenericTimeout_toggled(bool checked)
{
	ui->txtGenericTimeout->setEnabled(checked);
}

void MainWindow::on_txtGenericTimeout_textChanged(const QString &)
{
	bool accepted;
	quint64 value;
	updateTimeBox(ui->txtGenericTimeout, accepted, value);
}

void MainWindow::on_txtIntervalSize_textChanged(const QString &)
{
    bool accepted;
    quint64 value;
    updateTimeBox(ui->txtIntervalSize, accepted, value);
}

void MainWindow::on_checkCapture_toggled(bool)
{
	ui->spinCapturePacketCount->setEnabled(ui->checkCapture->isChecked());
	ui->spinCaptureEventCount->setEnabled(ui->checkCapture->isChecked());
}

void MainWindow::on_checkTcpReceiveWin_toggled(bool)
{
	ui->checkTcpReceiveWinFixed->setEnabled(ui->checkTcpReceiveWin->isChecked());
	ui->spinTcpReceiveWinFixed->setEnabled(ui->checkTcpReceiveWin->isChecked() && ui->checkTcpReceiveWinFixed->isChecked());
	ui->checkTcpReceiveWinAuto->setEnabled(ui->checkTcpReceiveWin->isChecked());
	ui->spinTcpReceiveWinScaleFactor->setEnabled(ui->checkTcpReceiveWin->isChecked() && ui->checkTcpReceiveWinAuto->isChecked());
}

void MainWindow::on_checkTcpReceiveWinFixed_toggled(bool)
{
	on_checkTcpReceiveWin_toggled(ui->checkTcpReceiveWin->isChecked());
	ui->checkTcpReceiveWinAuto->setChecked(!ui->checkTcpReceiveWinFixed->isChecked());
}

void MainWindow::on_checkTcpReceiveWinAuto_toggled(bool)
{
	on_checkTcpReceiveWin_toggled(ui->checkTcpReceiveWin->isChecked());
	ui->checkTcpReceiveWinFixed->setChecked(!ui->checkTcpReceiveWinAuto->isChecked());
}

void MainWindow::on_btnReadjustQueuesNow_clicked()
{
	emit logInformation(ui->txtDeploy, "Computing routes...");
	editor->graph()->computeRoutes();

	emit logInformation(ui->txtDeploy, "Readjusting queues...");
	if (ui->cmbBufferSize->currentText() == "large") {
		if (!editor->graph()->readjustQueues()) {
			emit logError(ui->txtDeploy, "Could not readjust all queues!");
		}
	} else if (ui->cmbBufferSize->currentText() == "small") {
		if (!editor->graph()->readjustQueuesSmall()) {
			emit logError(ui->txtDeploy, "Could not readjust all queues!");
		}
	} else {
		emit logError(ui->txtDeploy, "Could not readjust queues!");
	}
}

void MainWindow::on_btnGenericStop_clicked()
{
	mustStop = true;
}

bool MainWindow::experimentOk(QString testId, QString &reason, double delayErrorLimit)
{
	QString out, err;
	if (!readFile(testId + "/" + "emulator.out", out)) {
		reason = QString("Cannot open file %1").arg(testId + "/" + "emulator.out");
		return false;
	}
	if (!readFile(testId + "/" + "emulator.err", err)) {
		reason = QString("Cannot open file %1").arg(testId + "/" + "emulator.err");
		return false;
	}

	if (out == "fake" || err == "fake" ||
		out == "real" || err == "real") {
		reason = "All fine.";
		return true;
	}

	QString allout = out + "\n" + err;

	qreal packetsReceived = -1;
	qreal packetsDropped = -1;
	qreal maxDelayError = -1;
	qreal eventInversions = -1;
	foreach (QString line, allout.split('\n')) {
		if (line.startsWith("Absolute Stats:")) {
			line = line.replace("Absolute Stats:", "").replace("[", " ").replace("]", " ");
			QStringList tokens = line.split(' ', QString::SkipEmptyParts);
			if (tokens.count() != 6) {
				reason = QString("Cannot parse packet stats from: %1").arg(line);
				return false;
			}
			bool ok;
			packetsReceived = tokens.at(0).toDouble(&ok);
			if (!ok) {
				reason = QString("Cannot parse packet stats from: %1").arg(line);
				return false;
			}
			packetsDropped = tokens.at(3).toDouble(&ok);
			if (!ok) {
				reason = QString("Cannot parse packet stats from: %1").arg(line);
				return false;
			}
		} else if (line.startsWith("Average relative delay error:")) {
			line = line.replace("Average relative delay error:", "").replace(", Maximum relative delay error:", " ").replace("%", " ");
			QStringList tokens = line.split(' ', QString::SkipEmptyParts);
			if (tokens.count() != 2) {
				reason = QString("Cannot parse delay stats from: %1").arg(line);
				return false;
			}
			bool ok;
			maxDelayError = tokens.at(1).toDouble(&ok) * 0.01;
			if (!ok) {
				reason = QString("Cannot parse delay stats from: %1").arg(line);
				return false;
			}
		} else if (line.startsWith("Event inversions:")) {
			line = line.replace("Event inversions:", "");
			bool ok;
			eventInversions = line.toDouble(&ok);
			if (!ok) {
				reason = QString("Cannot parse event inversion stats from: %1").arg(line);
				return false;
			}
		}
	}
	if (packetsReceived < 0) {
		reason = QString("Packet stats seem to be missing, packetsReceived = %1").arg(packetsReceived);
		return false;
	}
	if (packetsReceived == 0) {
		reason = QString("No packets reached the emulator");
		return false;
	}
	if (packetsDropped < 0) {
		reason = QString("Packet stats seem to be missing, packetsDropped = %1").arg(packetsDropped);
		return false;
	}
	if (packetsDropped > 0) {
		reason = QString("Dropped packets during capture: %1").arg(packetsDropped);
		return false;
	}
	if (maxDelayError < 0) {
		reason = QString("Packet delay stats seem to be missing, maxDelayError = %1").arg(maxDelayError);
		return false;
	}
	if (maxDelayError > delayErrorLimit) {
		reason = QString("Maximum delay error too high: %1, limit = %2").arg(maxDelayError).arg(delayErrorLimit);
		return false;
	}
	if (eventInversions > 0) {
		reason = QString("Event inversions occured: %1, limit = %2").arg(eventInversions).arg(0);
		return false;
	}
	reason = "All fine.";
	return true;
}

void MainWindow::generateSimpleTopology(int pairs)
{
	// generate topology
	emit logInformation(ui->txtBatch, QString("Generating topology with 2 hosts"));

	double bw_KBps = simple_bw_KBps;
	int delay_ms = simple_delay_ms;
	int queueSize = simple_bufferbloat_factor * NetGraph::optimalQueueLength(bw_KBps, delay_ms);
	simple_queueSize = queueSize;

	editor->graph()->clear();
	int gateway = editor->graph()->addNode(NETGRAPH_NODE_GATEWAY, QPointF(0, 0));
	const double radiusMin = 300;
	double radius = pairs/2.0 * 3.0 * NETGRAPH_NODE_RADIUS/M_PI * 3.0;
	if (radius < radiusMin)
		radius = radiusMin;
	// minimun 2.0 * NETGRAPH_NODE_RADIUS / radius, anything higher adds spacing
	double alpha = 4.0 * NETGRAPH_NODE_RADIUS / radius;
	for (int pairIndex = 0; pairIndex < pairs; pairIndex++) {
		double angle = M_PI - (pairs / 2) * alpha + pairIndex * alpha + (1 - pairs % 2) * alpha / 2.0;
		int first = editor->graph()->addNode(NETGRAPH_NODE_HOST, QPointF(radius * cos(angle), -radius * sin(angle)));
		angle = M_PI - angle;
		int second = editor->graph()->addNode(NETGRAPH_NODE_HOST, QPointF(radius * cos(angle), -radius * sin(angle)));
		editor->graph()->addEdge(first, gateway, bw_KBps, delay_ms, 0, queueSize);
		editor->graph()->addEdge(gateway, first, bw_KBps, delay_ms, 0, queueSize);
		editor->graph()->addEdge(gateway, second, bw_KBps, delay_ms, 0, queueSize);
		editor->graph()->addEdge(second, gateway, bw_KBps, delay_ms, 0, queueSize);
	}
	editor->graph()->setFileName(QString("simple_%1KBps_%2ms_q%3.graph").arg(bw_KBps).arg(delay_ms).arg(queueSize));
	editor->graph()->saveToFile();
	emit reloadScene();
	emit saveGraph();
	emit logSuccess(ui->txtBatch, "OK.");
}

void MainWindow::on_btnQueueSave_clicked()
{
	saveExperimentQueue();
}

void MainWindow::saveExperimentQueue() {
	QFile file("queue.data");
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		Q_ASSERT_FORCE(false);
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	QByteArray buffer;
	{
		QDataStream raw(&buffer, QIODevice::WriteOnly);
		raw << experimentQueue;
	}
	buffer = qCompress(buffer);
	out << buffer;
}

void MainWindow::loadExperimentQueue() {
	QFile file("queue.data");
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		experimentQueue.clear();
		return;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	QByteArray buffer;
	in >> buffer;
	if (in.status() != QDataStream::Ok) {
		qDebug() << __FILE__ << __LINE__ << "Read error:" << file.fileName();
		Q_ASSERT_FORCE(false);
	}
	buffer = qUncompress(buffer);
	{
		QDataStream raw(&buffer, QIODevice::ReadOnly);
		raw >> experimentQueue;
	}
	updateQueueTable();
}

bool MainWindow::runLongCommand(QString path, QStringList args, QPlainTextEdit *log)
{
	mustStop = false;
	emit logInformation(log, QString("Starting %1 %2").arg(path).arg(args.join(" ")));
	QProcess runner;
	runner.setProcessChannelMode(QProcess::MergedChannels);
	runner.start(path, QStringList() << args);
	if (!runner.waitForStarted()) {
		emit logError(log, QString("Could not start %1!").arg(path));
		return false;
	}

	while (true) {
		if (mustStop) {
			emit logInformation(log, QString("Sending SIGINT to %1...").arg(path));
			kill(runner.pid(), SIGINT);
			if (!runner.waitForFinished(30000)) {
				emit logInformation(log, QString("Sending SIGTERM to %1...").arg(path));
				runner.terminate();
				if (!runner.waitForFinished(30000)) {
					emit logInformation(log, QString("Sending SIGKILL to %1...").arg(path));
					runner.kill();
				}
			}
		}
		bool finished = runner.waitForFinished(100);
		QString output = runner.readAll();
		if (!output.isEmpty()) {
			emit logOutput(log, output);
		}
		if (finished || runner.state() != QProcess::Running || mustStop)
			break;
		QApplication::processEvents();
	}
	int exitCode = runner.exitCode();
	emit logInformation(log, QString("%1 finished with exit code %2").arg(path).arg(exitCode));
	return exitCode == 0;
}

bool MainWindow::lineRunner(QStringList args, QPlainTextEdit *log)
{
    return runLongCommand(QString("%1/../build-line-runner/line-runner").arg(qApp->applicationDirPath()),
						  args, log);
}

