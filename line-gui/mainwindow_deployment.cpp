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

#include "../util/chronometer.h"

void MainWindow::on_btnDeploy_clicked()
{
    ui->txtDeploy->clear();

    // Make sure we saved
	if (!editor->save())
        return;

	RunParams params;
	if (!getRunParams(params, ui->txtDeploy))
		return;

	QString paramsFileName = QString("deploy-%1.data").arg(getCurrentTimeNanosec());
	saveRunParams(params, paramsFileName);

    // Start the computation.
    blockingOperationStarting();
	{
		if (lineRunner(QStringList() << "--deploy" << paramsFileName, ui->txtDeploy)) {
			emit logSuccess(ui->txtDeploy, "Deployment finished successfully.");
		} else {
			emit logError(ui->txtDeploy, "line-runner failed!");
		}
	}
	blockingOperationFinished();

	QDir(".").remove(paramsFileName);
}

bool MainWindow::computeRoutes(NetGraph *netGraph)
{
	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
    QTime t;
    emit logInformation(ui->txtDeploy, "Computing routes");
    t.start();
	bool result = netGraph->computeRoutes();
    emit logInformation(ui->txtDeploy, QString("Routing finished after %1 ms").arg(t.elapsed()));

    emit logSuccess(ui->txtDeploy, "OK.");
	if (netGraph->hasGUI) {
		if (!netGraph->saveToFile()) {
            emit logError(ui->txtDeploy, "Could not save graph.");
            return false;
        }

        emit routingChanged();
        emit usedChanged();
    }
    return result;
}

bool MainWindow::computeFullRoutes(NetGraph *netGraph)
{
#if 1
	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);
	bool result;
	{
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		QTime t;
		emit logInformation(ui->txtDeploy, "Computing full routes");
		t.start();
		result = netGraph->computeFullRoutes();
		emit logInformation(ui->txtDeploy, QString("Routing finished after %1 ms").arg(t.elapsed()));

		emit logSuccess(ui->txtDeploy, "OK.");
		if (netGraph->hasGUI) {
			if (!netGraph->saveToFile()) {
				emit logError(ui->txtDeploy, "Could not save graph.");
				return false;
			}

			emit routingChanged();
			emit usedChanged();
		}
	}
	return result;
#else
	Q_UNUSED(netGraph);
	return true;
#endif
}

void MainWindow::on_btnDeployStop_clicked()
{
	mustStop = true;
}


