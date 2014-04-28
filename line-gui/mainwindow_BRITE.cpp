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

#include "../util/qswiftprogressdialog.h"

void MainWindow::on_btnBriteImport_clicked()
{
	QDir briteDir ("../brite/BRITE/");
	QDir topoDir (".");
	QString fromFileName = QFileDialog::getOpenFileName(this, "Open BRITE topology", briteDir.absolutePath(), "BRITE topology file (*.brite)");
	if (fromFileName.isEmpty())
		return;

	QString toFileName;
	toFileName = fromFileName.split('/', QString::SkipEmptyParts).last().replace(".brite", ".graph", Qt::CaseInsensitive);
	toFileName = QFileDialog::getSaveFileName(this, "Save network graph", topoDir.absolutePath() + "/" + toFileName, "Network graph file (*.graph)");
	if (toFileName.isEmpty())
		return;

	ui->txtBrite->clear();
	// Start the load
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::importBrite, fromFileName, toFileName);
	voidWatcher.setFuture(future);
}

void MainWindow::on_btnBRITEBatchImport_clicked()
{
	if (QMessageBox::question(this, "Batch import", "The batch import tool can import multiple files.\n"
							  "It generates file names automatically (the '.brite' extension is converted into '.graph')."
							  " Any existing files will be overwritten without warning. Accept?",
							  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok) {
		//
		QFileDialog dialog(this);
		dialog.setFileMode(QFileDialog::ExistingFiles);
		dialog.setAcceptMode(QFileDialog::AcceptOpen);
		dialog.setDirectory("../brite/BRITE/");
		dialog.setNameFilter("BRITE topology file (*.brite)");
		if (dialog.exec()) {
			 QStringList fileNames = dialog.selectedFiles();
			 QStringList toFileNames;
			 foreach (QString s, fileNames) {
				 s = s.split('/', QString::SkipEmptyParts).last().replace(".brite", ".graph", Qt::CaseInsensitive);
				 toFileNames << "./" + s;
			 }
			 ui->txtBrite->clear();
			 // Start the load
			 blockingOperationStarting();
			 QFuture<void> future = QtConcurrent::run(this, &MainWindow::importBriteMulti, fileNames, toFileNames);
			 voidWatcher.setFuture(future);
		}
	}
}

void MainWindow::importBrite(QString fromFileName, QString toFileName)
{
	if (!briteImporter.import(fromFileName, toFileName))
		return;

	emit importFinished(toFileName);
}

void MainWindow::importBriteMulti(QStringList fromFileNames, QStringList toFileNames)
{
	//QMutexLockerDbg locker(editor->graph()->mutex, __FUNCTION__); Q_UNUSED(locker);
	for (int i = 0; i < fromFileNames.count(); i++) {
		QString fromFileName = fromFileNames[i];
		QString toFileName = toFileNames[i];

		emit logInformation(ui->txtBrite, QString("Loading %1 (%2/%3)").arg(fromFileName).arg(i+1).arg(fromFileNames.count()));

		if (!briteImporter.import(fromFileName, toFileName))
			return;

		emit logInformation(ui->txtBrite, "Import finished.");
		editor->graph()->setFileName(toFileName);
		if (!editor->graph()->loadFromFile()) {
			emit logError(ui->txtBrite, QString() + "Could not load file " + editor->graph()->fileName);
			continue;
		}

		emit logInformation(ui->txtBrite, "Adding host nodes...");
		foreach (NetGraphNode n, editor->graph()->nodes) {
			if (n.nodeType == NETGRAPH_NODE_GATEWAY) {
				int host = editor->graph()->addNode(NETGRAPH_NODE_HOST, QPointF(), n.ASNumber);
				editor->graph()->addEdgeSym(host, n.index, 300, 1, 0, 20);
			}
		}

		emit logInformation(ui->txtBrite, "Layout with Gephi...");
        editor->graph()->layoutGephi();

		emit logInformation(ui->txtBrite, "Computing AS hulls...");
		editor->graph()->computeASHulls();

		emit logInformation(ui->txtBrite, "Saving...");
		editor->graph()->saveToFile();

		emit logInformation(ui->txtBrite, "Graph done.");
	}
	emit logInformation(ui->txtBrite, "All done.");
}

/* GraphML */

void MainWindow::on_btnGraphMLImport_clicked()
{
	QDir sourceDir ("../zoo/archive/");
	QDir topoDir (".");
	QString fromFileName = QFileDialog::getOpenFileName(this, "Open GraphML topology", sourceDir.absolutePath(), "GraphML topology file (*.graphml)");
	if (fromFileName.isEmpty())
		return;

	QString toFileName;
	toFileName = fromFileName.split('/', QString::SkipEmptyParts).last().replace(".graphml", ".graph", Qt::CaseInsensitive);
	toFileName = QFileDialog::getSaveFileName(this, "Save network graph", topoDir.absolutePath() + "/" + toFileName, "Network graph file (*.graph)");
	if (toFileName.isEmpty())
		return;

	ui->txtBrite->clear();
	// Start the load
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::importGraphML, fromFileName, toFileName);
	voidWatcher.setFuture(future);
}

void MainWindow::on_btnGraphMLBatchImport_clicked()
{
	if (QMessageBox::question(this, "Batch import", "The batch import tool can import multiple files.\n"
							  "It generates file names automatically (the '.graphml' extension is converted into '.graph')."
							  " Any existing files will be overwritten without warning. Accept?",
							  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok) {
		//
		QFileDialog dialog(this);
		dialog.setFileMode(QFileDialog::ExistingFiles);
		dialog.setAcceptMode(QFileDialog::AcceptOpen);
		dialog.setDirectory("../zoo/archive/");
		dialog.setNameFilter("GraphML topology file (*.graphml)");
		if (dialog.exec()) {
			 QStringList fileNames = dialog.selectedFiles();
			 QStringList toFileNames;
			 foreach (QString s, fileNames) {
				 s = s.split('/', QString::SkipEmptyParts).last().replace(".graphml", ".graph", Qt::CaseInsensitive);
				 toFileNames << "./" + s;
			 }
			 ui->txtBrite->clear();
			 // Start the load
			 blockingOperationStarting();
			 QFuture<void> future = QtConcurrent::run(this, &MainWindow::importGraphMLMulti, fileNames, toFileNames);
			 voidWatcher.setFuture(future);
		}
	}
}

void MainWindow::importGraphML(QString fromFileName, QString toFileName)
{
	if (!graphMLImporter.import(fromFileName, toFileName))
		return;

	emit importFinished(toFileName);
}

void MainWindow::importGraphMLMulti(QStringList fromFileNames, QStringList toFileNames)
{
	//QMutexLockerDbg locker(editor->graph()->mutex, __FUNCTION__); Q_UNUSED(locker);
	for (int i = 0; i < fromFileNames.count(); i++) {
		QString fromFileName = fromFileNames[i];
		QString toFileName = toFileNames[i];

		emit logInformation(ui->txtBrite, QString("Loading %1 (%2/%3)").arg(fromFileName).arg(i+1).arg(fromFileNames.count()));

		if (!graphMLImporter.import(fromFileName, toFileName)) {
			emit logError(ui->txtBrite, QString("Import failed for %1").arg(fromFileName));
			continue;
		}

		emit logInformation(ui->txtBrite, "Import finished.");
		editor->graph()->setFileName(toFileName);
		if (!editor->graph()->loadFromFile()) {
			emit logError(ui->txtBrite, QString() + "Could not load file " + editor->graph()->fileName);
			continue;
		}

		if (ui->checkImportAddHosts->isChecked()) {
			emit logInformation(ui->txtBrite, "Adding host nodes...");
			foreach (NetGraphNode n, editor->graph()->nodes) {
				if (n.nodeType == NETGRAPH_NODE_GATEWAY) {
					int host = editor->graph()->addNode(NETGRAPH_NODE_HOST, QPointF(), n.ASNumber);
					editor->graph()->addEdgeSym(host, n.index, 300, 1, 0, 20);
				}
			}
		}

		emit logInformation(ui->txtBrite, "Layout with Gephi...");
		editor->graph()->layoutGephi();

		emit logInformation(ui->txtBrite, "Computing AS hulls...");
		editor->graph()->computeASHulls();

		emit logInformation(ui->txtBrite, "Saving...");
		editor->graph()->saveToFile();

		emit logInformation(ui->txtBrite, "Graph done.");
	}
	emit logInformation(ui->txtBrite, "All done.");
}

/* Utility functions */
QSwiftProgressDialog *layoutProgressDialog = NULL;

void layoutProgressCallback(int currentStep, int maxSteps, QString text)
{
	layoutProgressDialog->setRange(0, maxSteps);
	layoutProgressDialog->setValue(currentStep);
	layoutProgressDialog->setText(text);
}

void MainWindow::layoutGraph()
{
	QSwiftProgressDialog progress("Layout graph...", "", 0, 100, this);
	layoutProgressDialog = &progress;

	editor->graph()->layoutGephi(&layoutProgressCallback);
	editor->graph()->computeASHulls();
	editor->graph()->saveToFile();
	emit layoutFinished();

	layoutProgressDialog = NULL;
}

void MainWindow::layoutGraphUsed()
{
	editor->graph()->layoutGephiUsed();
	editor->graph()->computeASHulls();
	editor->graph()->saveToFile();
	emit layoutFinished();
}

void MainWindow::onImportFinished(QString fileName)
{
	//QMutexLockerDbg locker(editor->graph()->mutex, __FUNCTION__); Q_UNUSED(locker);
	doLogBriteInfo("Import finished.");
	editor->graph()->setFileName(fileName);
	if (!editor->graph()->loadFromFile()) {
		QMessageBox::critical(this, "Open error", QString() + "Could not load file " + editor->graph()->fileName, QMessageBox::Ok);
		return;
	}
	setWindowTitle(QString("line-gui - %1").arg(getGraphName(editor->graph())));

	if (ui->checkImportAddHosts->isChecked()) {
		doLogBriteInfo("Adding host nodes...");
		// Add host nodes
		foreach (NetGraphNode n, editor->graph()->nodes) {
			if (n.nodeType == NETGRAPH_NODE_GATEWAY) {
				int host = editor->graph()->addNode(NETGRAPH_NODE_HOST, QPointF(), n.ASNumber);
				editor->graph()->addEdgeSym(host, n.index, 300, 1, 0, 20);
			}
		}
	}

	doLogBriteInfo("Computing routes...");
	doRecomputeRoutes();
}

void MainWindow::doLayoutGraph()
{
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::layoutGraph);
	voidWatcher.setFuture(future);
}

void MainWindow::onLayoutFinished()
{
	doLogBriteInfo("Layout finished.");
	doLogBriteInfo("Loading scene and drawing.");
	editor->resetScene();
}

void MainWindow::doRecomputeRoutes()
{
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::recomputeRoutes);
	voidWatcher.setFuture(future);
}

void MainWindow::recomputeRoutes()
{
	//QMutexLockerDbg locker(editor->graph()->mutex, __FUNCTION__); Q_UNUSED(locker);
	editor->graph()->computeRoutes();
	editor->graph()->updateUsed();
	emit routingFinished();
}

void MainWindow::onRoutingFinished()
{
	doLogBriteInfo("Routing finished.");
	doLogBriteInfo("Layout graph via GESHI...");
	// Layout
	doLayoutGraph();
}
