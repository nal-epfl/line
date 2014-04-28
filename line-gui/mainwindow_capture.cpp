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

bool MainWindow::processCapture(QString expPath, int &packetCount, int &queueEventCount)
{
	if (processLineRecord(rootPath, expPath, packetCount, queueEventCount)) {
		openCapture(expPath);
		return true;
	}
	return false;
}

void MainWindow::openCapture(QString expPath)
{
	// QDesktopServices::openUrl(QString("http://localhost:8000/www/index.html?sim=%1").arg(expPath));
	QDir cd;
	QDesktopServices::openUrl(QString("file://%1/%2/index.html").arg(cd.absolutePath()).arg(expPath));
}

QString MainWindow::getCurrentSimulation()
{
	if (currentSimulation <= 0) {
		return QString();
	}
	return simulations[currentSimulation].dir;
}

void MainWindow::on_btnCaptureProcess_clicked()
{
	ui->txtCapture->clear();
	QString expPath = getCurrentSimulation();
	if (expPath.isEmpty()) {
		safeLogError(ui->txtCapture, "No simulation selected!");
		return;
	}

	startRedirectingQDebug(ui->txtCapture);
	int pc, ec;
	processCapture(expPath, pc, ec);
	stopRedirectingQDebug();
}

void MainWindow::on_btnOpenCapturePlots_clicked()
{
	QString expPath = getCurrentSimulation();
	if (expPath.isEmpty()) {
		safeLogError(ui->txtCapture, "No simulation selected!");
		return;
	}

	startRedirectingQDebug(ui->txtCapture);
	openCapture(expPath);
	stopRedirectingQDebug();
}
