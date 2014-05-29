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

#include "netgrapheditor.h"
#include "ui_netgrapheditor.h"

#include "qdisclosure.h"
#include "qaccordion.h"
#include "flowlayout.h"
#include "qgraphicsviewgood.h"
#include "../util/qswiftprogressdialog.h"
#include "../util/chronometer.h"
#include "../tomo/tomodata.h"
#include "util.h"
#include "qoplot.h"

QSwiftProgressDialog *progressDialog = NULL;

void updateTimeBox(QLineEdit *txt, bool &accepted, quint64 &value);

void adjustTableToContents(QTableWidget *table)
{
	// http://stackoverflow.com/questions/8766633/how-to-determine-the-correct-size-of-a-qtablewidget
	table->resizeColumnsToContents();
	table->resizeRowsToContents();

	int w = 0;
	w += 2;
	w += table->verticalHeader()->width();
	for (int i = 0; i < table->columnCount(); i++) {
		w += table->columnWidth(i);
	}

	int h = 0;
	h += 2;
	h += table->horizontalHeader()->height();
	for (int i = 0; i < table->rowCount(); i++) {
		h += table->rowHeight(i);
	}
	QSize bestSize(w, h);
	table->setMaximumSize(bestSize);
	table->setMinimumSize(bestSize);
}

NetGraphEditor::NetGraphEditor(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::NetGraphEditor)
{
	nFlowsPlot = NULL;
	lossPlot = NULL;
	ui->setupUi(this);

    changesNotSaved = false;

	scene = new NetGraphScene(this);
	nonPhasedGraph = scene->netGraph;

	ui->graphicsView->setScene(scene);

	adjustTableToContents(ui->tableQueueWeights);
	adjustTableToContents(ui->tablePolicerWeights);

	QButtonGroup *btnGroup = new QButtonGroup(this);
	btnGroup->addButton(ui->btnAddHost);
    btnGroup->addButton(ui->btnEditHost);
	btnGroup->addButton(ui->btnAddGateway);
	btnGroup->addButton(ui->btnAddRouter);
	btnGroup->addButton(ui->btnAddBorderRouter);
	btnGroup->addButton(ui->btnAddEdge);
	btnGroup->addButton(ui->btnHighlightEdges);
	btnGroup->addButton(ui->btnAddConnection);
	btnGroup->addButton(ui->btnDelConnection);
	btnGroup->addButton(ui->btnMoveNode);
	btnGroup->addButton(ui->btnMoveNodeArea);
	btnGroup->addButton(ui->btnEditEdge);
	btnGroup->addButton(ui->btnDelNode);
	btnGroup->addButton(ui->btnDelNodeArea);
	btnGroup->addButton(ui->btnDelEdge);
	btnGroup->addButton(ui->btnDrag);
	btnGroup->addButton(ui->btnShowRoutes);
	btnGroup->addButton(ui->btnAdjustConnections);
	ui->btnDrag->setChecked(true);

	{
		QRegExp rx("[1-9][0-9]*(s|ms|us|ns)");
		QValidator *validator = new QRegExpValidator(rx, this);
		ui->txtSamplingPeriod->setValidator(validator);
	}

    connect(scene, SIGNAL(hostSelected(NetGraphNode)), SLOT(hostSelected(NetGraphNode)));
	connect(scene, SIGNAL(edgeSelected(NetGraphEdge)), SLOT(edgeSelected(NetGraphEdge)));
	connect(scene, SIGNAL(wheelEventFired(QGraphicsSceneWheelEvent*)), SLOT(onWheelEventFired(QGraphicsSceneWheelEvent*)));
	connect(scene, SIGNAL(viewportChanged()), SLOT(updateZoom()), Qt::QueuedConnection);
	connect(scene, SIGNAL(graphChanged()), SLOT(onGraphChanged()), Qt::QueuedConnection);
	on_spinQueueLength_valueChanged(ui->spinQueueLength->value());
	on_spinLoss_valueChanged(ui->spinLoss->value());
	on_spinBandwidth_valueChanged(ui->spinBandwidth->value());
	on_spinDelay_valueChanged(ui->spinDelay->value());
	on_txtSamplingPeriod_textChanged(ui->txtSamplingPeriod->text());
	on_checkSampledTimeline_toggled(ui->checkSampledTimeline->isChecked());
    on_txtConnectionType_textChanged(ui->txtConnectionType->text());
    on_spinConnectionTrafficClass_valueChanged(ui->spinConnectionTrafficClass->value());
    on_checkConnectionOnOff_toggled(ui->checkConnectionOnOff->isChecked());
	on_cmbConnectionTypeBase_currentIndexChanged(ui->cmbConnectionTypeBase->currentText());

	on_checkHideEdges_toggled(ui->checkHideEdges->isChecked());
	on_checkUnusedHidden_toggled(ui->checkUnusedHidden->isChecked());
	on_checkHideConnections_toggled(ui->checkHideConnections->isChecked());
	on_checkHideFlows_toggled(ui->checkHideFlows->isChecked());
	on_checkDomains_toggled(ui->checkDomains->isChecked());
	on_spinEdgeQueues_valueChanged(ui->spinEdgeQueues->value());
	on_spinEdgePolicers_valueChanged(ui->spinEdgePolicers->value());

	resetScene();
}

NetGraphEditor::~NetGraphEditor()
{
    askSave();
	delete ui;
}

NetGraph *NetGraphEditor::graph()
{
	return nonPhasedGraph.data();
}

void NetGraphEditor::addPhaseGroup(NetGraphPhaseGroup group)
{
	phaseGroups << group;
	ui->cmbPhaseGroup->addItem(group.title);
}

void NetGraphEditor::clearPhaseGroups()
{
	while (ui->cmbPhaseGroup->count() > 1) {
		ui->cmbPhaseGroup->removeItem(1);
	}
	phaseGroups.clear();
	on_cmbPhaseGroup_currentIndexChanged(ui->cmbPhaseGroup->currentIndex());
}

void NetGraphEditor::clear()
{
	on_btnNew_clicked();
}

bool NetGraphEditor::save()
{
    if (!scene->netGraph->fileName.isEmpty()) {
        // save data to file
        if (!scene->netGraph->saveToFile()) {
            QMessageBox::critical(this, "Save error", QString() + "Could not save file " + scene->netGraph->fileName, QMessageBox::Ok);
            return false;
        }
        changesNotSaved = false;
        return true;
    } else {
        return on_btnSaveAs_clicked();
    }
}

bool NetGraphEditor::saveNoGUI()
{
	if (!scene->netGraph->fileName.isEmpty()) {
		// save data to file
		if (!scene->netGraph->saveToFile()) {
			return false;
		}
		changesNotSaved = false;
		return true;
	} else {
		return false;
    }
}

void NetGraphEditor::askSave()
{
    if (changesNotSaved) {
        QMessageBox msgBox;
        msgBox.setText(QString("The graph %1 has been modified.").arg(scene->netGraph->fileName));
        msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        if (ret == QMessageBox::Save) {
            save();
        } else if (ret == QMessageBox::Discard) {
            // do nothing
        }
    }
}

bool NetGraphEditor::saveAs()
{
    QFileDialog fileDialog(this, "Save network graph");
    fileDialog.setFilter("Network graph file (*.graph)");
    fileDialog.setDefaultSuffix("graph");
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setDirectory(QDir::current().path());
	fileDialog.selectFile(QDir::current().path() + "/" + graph()->fileName);

    QString fileName;
    while (fileDialog.exec()) {
        QStringList fileNames = fileDialog.selectedFiles();
        if (!fileNames.isEmpty()) {
            fileName = fileNames.first();
            if (!fileInDirectory(fileName, ".")) {
                QMessageBox::warning(this, "Incorrect save location",
                                     QString("Please choose a location in the current working directory (%1). You picked '%2'.").arg(QDir(".").absolutePath()).arg(QDir(fileName).canonicalPath()));
                fileName = QString();
                continue;
            }
        }
        break;
    }

    if (!fileName.isEmpty()) {
        scene->netGraph->setFileName(fileName);
        if (on_btnSave_clicked()) {
            emit renamed(fileName);
            changesNotSaved = false;
            return true;
        }
    }
    return false;
}

void NetGraphEditor::setReadOnly(bool value)
{
    ui->btnNew->setEnabled(!value);
    ui->btnOpen->setEnabled(!value);
    ui->btnSave->setEnabled(!value);
    ui->btnLayout->setEnabled(!value);
    ui->btnSCCDomains->setEnabled(!value);

    ui->btnAddHost->setEnabled(!value);
    ui->btnEditHost->setEnabled(!value);
	ui->btnAddGateway->setEnabled(!value);
	ui->btnAddRouter->setEnabled(!value);
	ui->btnAddBorderRouter->setEnabled(!value);
    ui->btnMoveNode->setEnabled(!value);
    ui->btnMoveNodeArea->setEnabled(!value);
    ui->btnDelNode->setEnabled(!value);
    ui->btnDelNodeArea->setEnabled(!value);

	ui->btnAddEdge->setEnabled(!value);
    ui->btnEditEdge->setEnabled(!value);
    ui->btnDelEdge->setEnabled(!value);
    ui->btnHighlightEdges->setEnabled(!value);

	ui->btnAddConnection->setEnabled(!value);
	ui->btnDelConnection->setEnabled(!value);
    ui->btnAddConnections10->setEnabled(!value);
    ui->btnAddConnections100->setEnabled(!value);
    ui->btnAddConnectionsF->setEnabled(!value);
    ui->btnAddConnectionsClear->setEnabled(!value);
    ui->btnAdjustConnections->setEnabled(!value);

    ui->btnShowRoutes->setEnabled(!value);
    ui->btnComputeRoutes->setEnabled(!value);
    ui->btnComputeAllRoutes->setEnabled(!value);

    ui->pageAddEditNode->setEnabled(!value);
	ui->pageAddEditLink->setEnabled(!value);
	ui->pageAddConnection->setEnabled(!value);
}

bool NetGraphEditor::saveSVG(QString fileName)
{
	QSvgGenerator generator;
	generator.setFileName(fileName);

	generator.setSize(QSize(ui->graphicsView->size()));
	generator.setViewBox(QRect(0, 0, ui->graphicsView->width(), ui->graphicsView->height()));
	generator.setTitle(graph()->fileName.isEmpty() ? "Untitled" : graph()->fileName.split('/', QString::SkipEmptyParts).last().replace(".graph", ""));
	generator.setDescription("Created by the LINE emulator.");

	QRectF viewRect(0, 0, ui->graphicsView->width(), ui->graphicsView->height());
	qreal w_screen, h_screen;
	w_screen = ui->graphicsView->width();
	h_screen = ui->graphicsView->height();
	qreal w_world, h_world;
	w_world = w_screen / scene->graph()->viewportZoom;
	h_world = h_screen / scene->graph()->viewportZoom;
	qreal left, top;
	left = scene->graph()->viewportCenter.x() - w_world/2;
	top = scene->graph()->viewportCenter.y() - h_world/2;
	QRectF sceneRect (left, top, w_world, h_world);

	QPainter painter;
	painter.begin(&generator);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
	painter.fillRect(viewRect.toRect(), Qt::white);
	scene->render(&painter, viewRect, sceneRect, Qt::IgnoreAspectRatio);
	painter.end();

	return true;
}

void progressCallback(int currentStep, int maxSteps, QString text)
{
	progressDialog->setRange(0, maxSteps);
	progressDialog->setValue(currentStep);
	progressDialog->setText(text);
}

void NetGraphEditor::on_btnShowRightPanel_clicked()
{
    if (ui->rightContainer->maximumWidth() == 0) {
        ui->rightContainer->setMaximumWidth(350);
		ui->btnShowRightPanel->setIcon(QIcon(":/icons/extra-resources/stylesheet-branch-closed.png"));
	} else {
        ui->rightContainer->setMaximumWidth(0);
		ui->btnShowRightPanel->setIcon(QIcon(":/icons/extra-resources/stylesheet-branch-left.png"));
	}
}

void NetGraphEditor::on_btnNew_clicked()
{
	scene->netGraph->clear();
	scene->reload();
    updateStats();
}

void NetGraphEditor::on_btnOpen_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this,
													"Open network graph", scene->netGraph->fileName, "Network graph file (*.graph)");

	if (!fileName.isEmpty()) {
		if (!fileInDirectory(fileName, ".")) {
			if (QMessageBox::question(this, "Import network graph",
									  QString("The network graph '%1' is not part of the working directory (%2).\n"
										  "Would you like to import it?").arg(fileName).arg(QDir().absolutePath()),
									  QMessageBox::Yes, QMessageBox::Abort) == QMessageBox::Yes) {
				QString newName = fileName.isEmpty() ? QString() : fileName.split("/", QString::SkipEmptyParts).last();
				if (QFile(newName).exists()) {
					QFileDialog fileDialog(this, "Save network graph");
					fileDialog.setFilter("Network graph file (*.graph)");
					fileDialog.setDefaultSuffix("graph");
					fileDialog.setFileMode(QFileDialog::AnyFile);
					fileDialog.setAcceptMode(QFileDialog::AcceptSave);
					newName = QString();

					while (fileDialog.exec()) {
						QStringList fileNames = fileDialog.selectedFiles();
						if (!fileNames.isEmpty()) {
							newName = fileNames.first();
							if (!fileInDirectory(newName, ".")) {
								QMessageBox::warning(this, "Incorrect save location",
													 QString("Please choose a location in the current working directory (%1). You picked '%2'.").arg(QDir(".").absolutePath()).arg(newName));
								newName = QString();
								continue;
							}
						}
						break;
					}
				}
				if (newName.isEmpty())
					return;
				QFile::copy(fileName, newName);
				fileName = newName;
			} else {
				return;
			}
		}
		clearPhaseGroups();
		loadGraph(fileName);
	}
}

bool NetGraphEditor::loadGraph(QString fileName)
{
    askSave();
	QString oldName = scene->netGraph->fileName;
	if (!fileName.endsWith(".graph"))
		fileName += ".graph";
	scene->netGraph->setFileName(fileName);
	if (!scene->netGraph->loadFromFile()) {
		QMessageBox::critical(this, "Open error", QString() + "Could not load file " + scene->netGraph->fileName, QMessageBox::Ok);
		scene->netGraph->setFileName(oldName);
		return false;
	}
    changesNotSaved = false;

	resetScene();
    updateStats();
	on_btnFit_clicked();
	return true;
}

void NetGraphEditor::loadGraph(NetGraph *graph)
{
	clearPhaseGroups();
	*scene->netGraph.data() = *graph;
	resetScene();
}

void NetGraphEditor::resetScene()
{
	scene->reload();
	ui->spinZoom->setValue(scene->graph()->viewportZoom * 100.0);
	updateZoom();
    updateStats();
}

void NetGraphEditor::reloadScene()
{
	scene->reload();
	ui->spinZoom->setValue(scene->graph()->viewportZoom * 100.0);
	updateZoom();
	updateStats();
}

void NetGraphEditor::setModified()
{
	changesNotSaved = true;
}

bool NetGraphEditor::on_btnSave_clicked()
{
    return save();
}

bool NetGraphEditor::on_btnSaveAs_clicked()
{
    return saveAs();
}

void NetGraphEditor::on_btnLayout_clicked()
{
	QSwiftProgressDialog progress("Layout graph...", "", 0, 100, this);
	progressDialog = &progress;

	scene->netGraph->layoutGephi(&progressCallback);
	scene->netGraph->computeASHulls();
	scene->netGraph->saveToFile();
	resetScene();
	on_btnFit_clicked();

	progressDialog = NULL;
}

void NetGraphEditor::on_btnSCCDomains_clicked()
{
	scene->netGraph->computeDomainsFromSCC();
	scene->netGraph->saveToFile();
	resetScene();
}

void NetGraphEditor::on_btnAddHost_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::InsertHost);
		setRightPanelPage(ui->pageAddEditNode);
        ui->boxHostProperties->setEnabled(true);
	}
}

void NetGraphEditor::on_btnEditHost_toggled(bool checked)
{
    if (checked) {
        scene->setMode(NetGraphScene::EditHost);
		setRightPanelPage(ui->pageAddEditNode);
    }
}

void NetGraphEditor::on_btnAddGateway_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::InsertGateway);
		setRightPanelPage(ui->pageAddEditNode);
	}
}

void NetGraphEditor::on_btnAddRouter_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::InsertRouter);
		setRightPanelPage(ui->pageAddEditNode);
	}
}

void NetGraphEditor::on_btnAddBorderRouter_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::InsertBorderRouter);
		setRightPanelPage(ui->pageAddEditNode);
	}
}

void NetGraphEditor::on_btnMoveNode_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::MoveNode);
		setRightPanelPage(ui->pageMoveNode);
	}
}

void NetGraphEditor::on_btnMoveNodeArea_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::MoveNodeArea);
		setRightPanelPage(ui->pageMoveNode);
	}
}

void NetGraphEditor::on_btnDelNode_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::DelNode);
		setRightPanelPage(ui->pageDeleteNode);
	}
}

void NetGraphEditor::on_btnDelNodeArea_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::DelNodeArea);
		setRightPanelPage(ui->pageDelNodeArea);
	}
}

void NetGraphEditor::on_btnAddEdge_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::InsertEdgeStart);
		setRightPanelPage(ui->pageAddEditLink);
	}
}

void NetGraphEditor::on_btnEditEdge_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::EditEdge);
		setRightPanelPage(ui->pageAddEditLink);
	}
}

void NetGraphEditor::on_btnDelEdge_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::DelEdge);
		setRightPanelPage(ui->pageDeleteLink);
	}
}

void NetGraphEditor::on_btnAddConnection_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::InsertConnectionStart);
		setRightPanelPage(ui->pageAddConnection);
	}
}

void NetGraphEditor::on_btnDelConnection_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::DelConnection);
		setRightPanelPage(ui->pageDeleteConnection);
	}
}

void NetGraphEditor::on_btnShowRoutes_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::ShowRoutes);
		setRightPanelPage(ui->pageShowRoutes);
	}
}

void NetGraphEditor::on_btnHighlightEdges_toggled(bool checked)
{
	if (checked) {
		setRightPanelPage(ui->pageHighlightLinks);
	}
}

void NetGraphEditor::on_btnComputeRoutes_clicked()
{
	computeRoutes(scene->graph(), false);
}

void NetGraphEditor::on_btnComputeAllRoutes_clicked()
{
	computeRoutes(scene->graph(), true);
}

void NetGraphEditor::hostSelected(NetGraphNode host)
{
    ui->boxHostProperties->setEnabled(scene->editMode == NetGraphScene::InsertHost ||
                (scene->editMode == NetGraphScene::EditHost && host.index >= 0));
    ui->checkHostWeb->setChecked(host.web);
    ui->checkHostVoiceVideo->setChecked(host.vvoip);
    ui->checkHostP2P->setChecked(host.p2p);
    ui->checkHostTv->setChecked(host.iptv);
	ui->checkHostServer->setChecked(host.server);
	ui->checkHostHeavy->setChecked(host.heavy);
}

void NetGraphEditor::edgeSelected(NetGraphEdge edge)
{
	ui->spinDelay->setValue(edge.delay_ms);
	ui->spinBandwidth->setValue(edge.bandwidth * 8.0 / 1.0e3);
	ui->spinLoss->setValue(edge.lossBernoulli);
	ui->spinQueueLength->setValue(edge.queueLength);
	ui->checkSampledTimeline->setChecked(edge.recordSampledTimeline);
	ui->txtSamplingPeriod->setText(timeToString(edge.timelineSamplingPeriod));

    ui->spinEdgeQueues->setValue(edge.queueCount);
	ui->tableQueueWeights->setRowCount(edge.queueCount);
	for (int queue = 0; queue < edge.queueCount; queue++) {
		QTableWidgetItem *item = new QTableWidgetItem(QString("%1").arg(queue));
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		ui->tableQueueWeights->setItem(queue, 0, item);
		QDoubleSpinBox *spin = new QDoubleSpinBox(ui->tableQueueWeights);
		spin->setMinimum(0);
		spin->setMaximum(1.0);
		spin->setSingleStep(0.1);
		spin->setValue(edge.queueWeights[queue]);
		connect(spin, SIGNAL(valueChanged(double)), this, SLOT(onSpinQueueWeight_valueChanged(double)));
		ui->tableQueueWeights->setCellWidget(queue, 1, spin);
	}
	adjustTableToContents(ui->tableQueueWeights);

	ui->spinEdgePolicers->setValue(edge.policerCount);
	ui->tablePolicerWeights->setRowCount(edge.policerCount);
	for (int policer = 0; policer < edge.policerCount; policer++) {
		QTableWidgetItem *item = new QTableWidgetItem(QString("%1").arg(policer));
		item->setFlags(item->flags() ^ Qt::ItemIsEditable);
		ui->tablePolicerWeights->setItem(policer, 0, item);
		QDoubleSpinBox *spin = new QDoubleSpinBox(ui->tablePolicerWeights);
		spin->setMinimum(0);
		spin->setMaximum(1.0);
		spin->setSingleStep(0.1);
		spin->setValue(edge.policerWeights[policer]);
		connect(spin, SIGNAL(valueChanged(double)), this, SLOT(onSpinPolicerWeight_valueChanged(double)));
		ui->tablePolicerWeights->setCellWidget(policer, 1, spin);
	}
	adjustTableToContents(ui->tablePolicerWeights);
}

bool NetGraphEditor::computeRoutes(NetGraph *netGraph, bool full)
{
	// Make sure we saved
	if (!on_btnSave_clicked())
		return false;

	Chronometer chrono(__FUNCTION__); Q_UNUSED(chrono);
	QTime t;
	t.start();
	bool result = full ? netGraph->computeFullRoutes() : netGraph->computeRoutes();

	if (!netGraph->saveToFile()) {
		return false;
	}

	scene->showFlows();
	scene->usedChanged();

    updateStats();

	return result;
}

void NetGraphEditor::on_btnDrag_toggled(bool checked)
{
	if (checked) {
		scene->setMode(NetGraphScene::Drag);
		setRightPanelPage(ui->pageDrag);
	}
}

void NetGraphEditor::on_checkUnusedHidden_toggled(bool checked)
{
	scene->setUnusedHidden(!checked);
}

void NetGraphEditor::on_checkHideConnections_toggled(bool checked)
{
	scene->setHideConnections(!checked);
}

void NetGraphEditor::on_checkHideFlows_toggled(bool checked)
{
	scene->setHideFlows(!checked);
}

void NetGraphEditor::on_checkHideEdges_toggled(bool checked)
{
	scene->setHideEdges(!checked);
}

void NetGraphEditor::on_checkDomains_toggled(bool checked)
{
	scene->setHideDomains(!checked);
}

void NetGraphEditor::on_cmbFastMode_currentIndexChanged(const QString &arg1)
{
	if (arg1 == "Auto") {
		scene->setFastMode(true);
	} else if (arg1 == "Fast") {
		scene->setFastMode(false, true);
	} else if (arg1 == "Full") {
		scene->setFastMode(false, false);
	}
}

void NetGraphEditor::on_checkHostWeb_toggled(bool)
{
	setSceneTrafficFlags();
}

void NetGraphEditor::on_checkHostP2P_toggled(bool)
{
	setSceneTrafficFlags();
}

void NetGraphEditor::on_checkHostVoiceVideo_toggled(bool)
{
	setSceneTrafficFlags();
}

void NetGraphEditor::on_checkHostTv_toggled(bool)
{
	setSceneTrafficFlags();
}

void NetGraphEditor::on_checkHostServer_toggled(bool )
{
	setSceneTrafficFlags();
}

void NetGraphEditor::on_checkHostHeavy_toggled(bool)
{
	setSceneTrafficFlags();
}

void NetGraphEditor::setSceneTrafficFlags()
{
	scene->hostTrafficFlagsChanged(ui->checkHostWeb->isChecked(),
								   ui->checkHostVoiceVideo->isChecked(),
								   ui->checkHostP2P->isChecked(),
								   ui->checkHostTv->isChecked(),
								   ui->checkHostServer->isChecked(),
								   ui->checkHostHeavy->isChecked());
}

void NetGraphEditor::on_spinBandwidth_valueChanged(double val)
{
	scene->bandwidthChanged(val * 1.0e3 / 8.0);
	if (ui->checkAutoQueue->isChecked())
		autoAdjustQueue();
}

void NetGraphEditor::on_spinEdgeQueues_valueChanged(int val)
{
    scene->queueCountChanged(val);
}

void NetGraphEditor::on_spinEdgePolicers_valueChanged(int val)
{
	scene->policerCountChanged(val);
}

void NetGraphEditor::autoAdjustQueue()
{
	ui->spinQueueLength->setValue(NetGraph::optimalQueueLength(ui->spinBandwidth->value() * 1.0e3 / 8.0, ui->spinDelay->value()));
}

void NetGraphEditor::on_spinDelay_valueChanged(int val)
{
	scene->delayChanged(val);
	if (ui->checkAutoQueue->isChecked())
		autoAdjustQueue();
}

void NetGraphEditor::on_spinLoss_valueChanged(double val)
{
	scene->lossRateChanged(val);
}

void NetGraphEditor::onSpinQueueWeight_valueChanged(double)
{
	QDoubleSpinBox *spin = dynamic_cast<QDoubleSpinBox*>(sender());
	if (spin) {
		for (int r = 0; r < ui->tableQueueWeights->rowCount(); r++) {
			QWidget *w = ui->tableQueueWeights->cellWidget(r, 1);
			if (spin == w) {
				scene->queueWeightChanged(r, spin->value());
			}
		}
	}
}

void NetGraphEditor::onSpinPolicerWeight_valueChanged(double)
{
	QDoubleSpinBox *spin = dynamic_cast<QDoubleSpinBox*>(sender());
	if (spin) {
		for (int r = 0; r < ui->tablePolicerWeights->rowCount(); r++) {
			QWidget *w = ui->tablePolicerWeights->cellWidget(r, 1);
			if (spin == w) {
				scene->policerWeightChanged(r, spin->value());
			}
		}
	}
}

void NetGraphEditor::on_spinQueueLength_valueChanged(int val)
{
	scene->queueLenghtChanged(val);
}

void NetGraphEditor::on_txtSamplingPeriod_textChanged(const QString &)
{
	bool accepted;
	quint64 value;
	updateTimeBox(ui->txtSamplingPeriod, accepted, value);
	if (accepted)
		onSamplingPeriodChanged(value);
}

void updateTimeBox(QLineEdit *txt, bool &accepted, quint64 &value)
{
	accepted = false;
	const QValidator *validator = txt->validator();

	if (validator) {
		QString text = txt->text();
		int pos = txt->cursorPosition();
		QValidator::State state = validator->validate(text, pos);
		if (state == QValidator::Acceptable) {
			txt->setStyleSheet("color:black");
			value = 0;
			quint64 multiplier = 1;
			if (text.endsWith("ns")) {
				text = text.replace("ns", "");
			} else if (text.endsWith("us")) {
				text = text.replace("us", "");
				multiplier = 1000;
			} else if (text.endsWith("ms")) {
				text = text.replace("ms", "");
				multiplier = 1000000;
			} else if (text.endsWith("s")) {
				text = text.replace("s", "");
				multiplier = 1000000000;
			}
			value = (quint64)text.toLongLong() * multiplier;
			accepted = true;
		} else if (state == QValidator::Intermediate) {
			txt->setStyleSheet("color:blue");
		} else if (state == QValidator::Invalid) {
			txt->setStyleSheet("color:red");
		}
	}
}

void NetGraphEditor::on_checkSampledTimeline_toggled(bool checked)
{
	scene->samplingChanged(checked);
}

void NetGraphEditor::on_checkAutoQueue_toggled(bool checked)
{
	if (checked) {
		autoAdjustQueue();
	}
}

void NetGraphEditor::on_spinASNumber_valueChanged(int val)
{
	scene->ASNumberChanged(val);
}

void NetGraphEditor::on_txtConnectionType_textChanged(const QString &val)
{
	scene->connectionTypeChanged(val);
}

void NetGraphEditor::on_spinConnectionTrafficClass_valueChanged(int)
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_checkConnectionOnOff_toggled(bool checked)
{
	updateTxtConnectionType();
	ui->labelOnDuration->setEnabled(checked);
	ui->spinOnDurationMin->setEnabled(checked);
	ui->spinOnDurationMax->setEnabled(checked);
	ui->labelOffDuration->setEnabled(checked);
	ui->spinOffDurationMin->setEnabled(checked);
	ui->spinOffDurationMax->setEnabled(checked);
}

void NetGraphEditor::on_spinOnDurationMin_valueChanged(double)
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinOnDurationMax_valueChanged(double)
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinOffDurationMin_valueChanged(double)
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinOffDurationMax_valueChanged(double)
{
	updateTxtConnectionType();
}

void NetGraphEditor::onSamplingPeriodChanged(quint64 val)
{
    scene->samplingPeriodChanged(val);
}

void NetGraphEditor::onGraphChanged()
{
	changesNotSaved = true;
    updateStats();
}

void NetGraphEditor::setRightPanelPage(QWidget *widget)
{
	foreach (QObject *child, ui->rightPanel->children()) {
		QWidget *childWidget = dynamic_cast<QWidget*>(child);
		if (childWidget) {
			childWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
		}
	}
	ui->rightPanel->setCurrentWidget(widget);
	ui->rightPanel->currentWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	ui->rightPanel->currentWidget()->layout()->setAlignment(Qt::AlignTop);
	ui->rightPanel->adjustSize();
	ui->scrollArea_3->adjustSize();
}

void NetGraphEditor::onWheelEventFired(QGraphicsSceneWheelEvent *event)
{
	QPointF oldPointedPos = event->scenePos();
	QPointF oldCenter = scene->graph()->viewportCenter;
	qreal oldScale = scene->graph()->viewportZoom;

	float steps = event->delta()/8.0/15.0;
	ui->spinZoom->setValue(scene->graph()->viewportZoom * pow(1.5, steps) * 100.0);

	qreal newScale = scene->graph()->viewportZoom;
	qreal x = (oldCenter.x() - oldPointedPos.x() * (1.0 - newScale / oldScale)) * oldScale / newScale;
	qreal y = (oldCenter.y() - oldPointedPos.y() * (1.0 - newScale / oldScale)) * oldScale / newScale;
	scene->graph()->viewportCenter = QPointF(x, y);

    updateZoom();
}

void NetGraphEditor::updateZoom()
{
#if 0
	qreal scale1 = 1.1 * qMax(scene->root->childrenBoundingRect().width() / ui->graphicsView->viewport()->width(),
					   scene->root->childrenBoundingRect().height() / ui->graphicsView->viewport()->height());
	qreal targetScale = ui->spinZoom->value() / 100.0 * (1/scale1);
	qreal scaleFactor = targetScale / ui->graphicsView->transform().m11();

	qDebug() << "scene->itemsBoundingRect().width()" << scene->itemsBoundingRect().width();
	qDebug() << "scene->itemsBoundingRect().height()" << scene->itemsBoundingRect().height();
	qDebug() << "ui->graphicsView->viewport()->width()" << ui->graphicsView->viewport()->width();
	qDebug() << "ui->graphicsView->viewport()->height()" << ui->graphicsView->viewport()->height();

	if (isnan(scaleFactor))
		return;

	ui->graphicsView->scale(scaleFactor, scaleFactor);
#else
	ui->graphicsView->updateTransform();
#endif
}

void NetGraphEditor::on_spinZoom_valueChanged(double)
{
	scene->graph()->viewportZoom = ui->spinZoom->value() / 100.0;
	updateZoom();
}

void NetGraphEditor::on_cmbPhaseGroup_currentIndexChanged(int index)
{
	if (index <= 0) {
		// No phasing
		ui->groupBoxPhasing->setTitle("No phasing");
		ui->labelPhase->setText("");
		ui->labelPhaseGroup->setText("");
		ui->listPhases->clear();

		scene->netGraph = nonPhasedGraph;
		reloadScene();
	} else {
		int phaseGroupIndex = ui->cmbPhaseGroup->currentIndex() - 1;
		if (phaseGroupIndex < 0 || phaseGroupIndex >= phaseGroups.count())
			return;
		NetGraphPhaseGroup &phaseGroup = phaseGroups[phaseGroupIndex];

		ui->groupBoxPhasing->setTitle(phaseGroup.title);
		ui->labelPhaseGroup->setText(phaseGroup.text);
		ui->listPhases->clear();
		for (int i = 0; i < phaseGroup.phases.count(); i++) {
			ui->listPhases->addItem(phaseGroup.phases[i].title);
		}
		if (ui->listPhases->count() > 0) {
			ui->listPhases->setCurrentRow(0);
		}

		on_btnPhasing_clicked();
	}
}

void NetGraphEditor::on_listPhases_currentItemChanged(QListWidgetItem *, QListWidgetItem *)
{
	int phaseGroupIndex = ui->cmbPhaseGroup->currentIndex() - 1;
	if (phaseGroupIndex < 0 || phaseGroupIndex >= phaseGroups.count())
		return;
	NetGraphPhaseGroup &phaseGroup = phaseGroups[phaseGroupIndex];

	int phaseIndex = ui->listPhases->currentIndex().row();
	if (phaseIndex < 0 || phaseIndex >= phaseGroup.phases.count())
		return;
	NetGraphPhase &phase = phaseGroup.phases[phaseIndex];
	ui->labelPhase->setText(phase.text);
	if (ui->labelPhase->text().isEmpty()) {
		ui->labelPhase->hide();
	} else {
		ui->labelPhase->show();
	}
	scene->netGraph = phase.netGraph;
	reloadScene();
}

void NetGraphEditor::on_btnPhasing_clicked()
{
	setRightPanelPage(ui->pagePhasing);
}

void NetGraphEditor::on_btnFit_clicked()
{
	qreal scale1 = 1.1 * qMax(scene->root->childrenBoundingRect().width() / ui->graphicsView->viewport()->width(),
					   scene->root->childrenBoundingRect().height() / ui->graphicsView->viewport()->height());
	qreal zoom = 1/scale1;

	if (isnan(zoom))
		return;

	scene->graph()->viewportZoom = 1/scale1;
	scene->graph()->viewportCenter = scene->root->childrenBoundingRect().center();
	updateZoom();
	ui->spinZoom->setValue(scene->graph()->viewportZoom * 100.0);
}

void NetGraphEditor::on_btnAddConnections10_clicked()
{
	QList<NetGraphNode> hosts = graph()->getHostNodes();
	if (hosts.count() < 2)
		return;

	for (int i = 0; i < 10; i++) {
		QPair<qint32, qint32> c;
		while (c.first == c.second) {
			c.first = randInt(0, hosts.count() - 1);
			c.second = randInt(0, hosts.count() - 1);
		}
		graph()->addConnection(NetGraphConnection(hosts[c.first].index,
												  hosts[c.second].index,
												  ui->txtConnectionType->text(),
												  ""));
	}
	reloadScene();
}

void NetGraphEditor::on_btnAddConnections100_clicked()
{
    QList<NetGraphNode> hosts = graph()->getHostNodes();
    if (hosts.count() < 2)
        return;

    for (int i = 0; i < 100; i++) {
        QPair<qint32, qint32> c;
        while (c.first == c.second) {
            c.first = randInt(0, hosts.count() - 1);
            c.second = randInt(0, hosts.count() - 1);
        }
		graph()->addConnection(NetGraphConnection(hosts[c.first].index,
												  hosts[c.second].index,
												  ui->txtConnectionType->text(),
												  ""));
    }
    reloadScene();
}

void NetGraphEditor::on_btnAddConnectionsBatch_clicked()
{
	QList<NetGraphNode> hosts = graph()->getHostNodes();
	if (hosts.count() < 2)
		return;

    QSet<QPair<qint32, qint32> > existingConnections;
    foreach (NetGraphConnection c, graph()->connections) {
        existingConnections.insert(QPair<qint32, qint32>(c.source, c.dest));
    }
    // Forbid connections between two hosts connected to the same gateway
    {
        QHash<qint32, QSet<qint32> > gateway2hosts;
        foreach (NetGraphEdge e, graph()->edges) {
            if (graph()->nodes[e.source].nodeType == NETGRAPH_NODE_HOST &&
                graph()->nodes[e.dest].nodeType != NETGRAPH_NODE_HOST) {
                gateway2hosts[e.dest].insert(e.source);
            } else if (graph()->nodes[e.dest].nodeType == NETGRAPH_NODE_HOST &&
                       graph()->nodes[e.source].nodeType != NETGRAPH_NODE_HOST) {
                gateway2hosts[e.source].insert(e.dest);
            }
        }
        foreach (int gateway, gateway2hosts.keys()) {
            QList<qint32> hostList = gateway2hosts[gateway].toList();
            for (int i = 0; i < hostList.count(); i++) {
                for (int j = 0; j < hostList.count(); j++) {
                    if (i != j) {
                        existingConnections.insert(QPair<qint32, qint32>(hostList[i], hostList[j]));
                    }
                }
            }
        }
    }

	int numConnections = ui->spinNumConnectionsBatch->value();

	if (ui->cmbTypeConnectionsBatch->currentIndex() == 0) {
		// Background
		// Random connections between any two nodes
		// Either continuous or on-off traffic (user configurable)
        QList<QPair<qint32, qint32> > possibleConnections;
        for (int i = 0; i < hosts.count(); i++) {
            for (int j = 0; j < hosts.count(); j++) {
                if (i == j)
                    continue;
                QPair<qint32, qint32> c = QPair<qint32, qint32>(hosts[i].index, hosts[j].index);
                if (existingConnections.contains(c))
                    continue;
                possibleConnections << c;
            }
        }
        qShuffle(possibleConnections);
        for (int i = 0; i < numConnections && !possibleConnections.isEmpty(); i++) {
            QPair<qint32, qint32> c = possibleConnections.takeLast();
			graph()->addConnection(NetGraphConnection(c.first, c.second, ui->txtConnectionType->text(), ""));
		}
	} else if (ui->cmbTypeConnectionsBatch->currentIndex() == 1) {
		// Web
		// 90% are download flows (server->client)
		// 10% are uploads (client->server)
		// Either continuous or on-off traffic (user configurable)
		QList<NetGraphNode> servers;
		QList<NetGraphNode> clients;
		foreach (NetGraphNode n, hosts) {
			if (!n.web)
				continue;
			if (n.server) {
				servers << n;
			} else {
				clients << n;
			}
		}
		if (servers.isEmpty() || clients.isEmpty())
			return;
        QList<QPair<qint32, qint32> > possibleUploadConnections;
        QList<QPair<qint32, qint32> > possibleDownloadConnections;
        for (int i = 0; i < servers.count(); i++) {
            for (int j = 0; j < clients.count(); j++) {
                QPair<qint32, qint32> cDownload = QPair<qint32, qint32>(servers[i].index, clients[j].index);
                if (!existingConnections.contains(cDownload)) {
                    possibleDownloadConnections << cDownload;
                }
                QPair<qint32, qint32> cUpload = QPair<qint32, qint32>(clients[j].index, servers[i].index);
                if (!existingConnections.contains(cUpload)) {
                    possibleUploadConnections << cUpload;
                }
            }
        }
        qShuffle(possibleUploadConnections);
        qShuffle(possibleDownloadConnections);
		for (int i = 0; i < numConnections; i++) {
            if (frand() <= 0.9 && !possibleDownloadConnections.isEmpty()) {
                QPair<qint32, qint32> c = possibleDownloadConnections.takeLast();
				graph()->addConnection(NetGraphConnection(c.first, c.second, ui->txtConnectionType->text(), ""));
            } else if (!possibleUploadConnections.isEmpty()) {
                QPair<qint32, qint32> c = possibleUploadConnections.takeLast();
				graph()->addConnection(NetGraphConnection(c.first, c.second, ui->txtConnectionType->text(), ""));
			}
		}
	} else if (ui->cmbTypeConnectionsBatch->currentIndex() == 2) {
		// P2P
		// Random connections between peers (mesh)
		// Either continuous or on-off traffic (user configurable)
		QList<NetGraphNode> peers;
		foreach (NetGraphNode n, hosts) {
			if (!n.p2p)
				continue;
			peers << n;
		}
		if (peers.count() < 2)
			return;
        QList<QPair<qint32, qint32> > possibleConnections;
        for (int i = 0; i < peers.count(); i++) {
            for (int j = 0; j < peers.count(); j++) {
                if (i == j)
                    continue;
                QPair<qint32, qint32> c = QPair<qint32, qint32>(peers[i].index, peers[j].index);
                if (!existingConnections.contains(c)) {
                    possibleConnections << c;
                }
            }
        }
        qShuffle(possibleConnections);
        for (int i = 0; i < numConnections && !possibleConnections.isEmpty(); i++) {
            QPair<qint32, qint32> c = possibleConnections.takeLast();
			graph()->addConnection(NetGraphConnection(c.first, c.second, ui->txtConnectionType->text(), ""));
		}
	} else if (ui->cmbTypeConnectionsBatch->currentIndex() == 3) {
		// Voice and Video
		// Full meshes of 2 to 6 people.
		// 80% are 1:1, 20% are meshes of 3-6 users
		// Continuous traffic
		QList<NetGraphNode> chatters;
		foreach (NetGraphNode n, hosts) {
			if (!n.vvoip)
				continue;
			chatters << n;
		}
        // We limit the number of iterations so we do not hang if numConnections is larger than possible
        for (int iter = 0; iter < 10 && numConnections >= 2 && chatters.count() >= 2; iter++) {
			int numOneOnOne = numConnections >= 5 ? numConnections * 0.8 : numConnections;

			qShuffle(chatters);
            for (int i = 0; i < numOneOnOne && chatters.count() >= 2; i++) {
                QPair<qint32, qint32> c01 = QPair<qint32, qint32>(chatters[0].index, chatters[1].index);
                if (!existingConnections.contains(c01)) {
					graph()->addConnection(NetGraphConnection(chatters[0].index,
															  chatters[1].index,
															  ui->txtConnectionType->text(),
															  ""));
                    numConnections--;
                }
                QPair<qint32, qint32> c10 = QPair<qint32, qint32>(chatters[1].index, chatters[0].index);
                if (!existingConnections.contains(c10)) {
					graph()->addConnection(NetGraphConnection(chatters[1].index,
															  chatters[0].index,
															  ui->txtConnectionType->text(),
															  ""));
                    numConnections--;
                }
				chatters.removeFirst();
				chatters.removeFirst();
			}

			while (chatters.count() >= 2) {
				int meshSize = randInt(chatters.count() == 2 ? 2 : 3, qMin(6, chatters.count()));
				for (int i = 0; i < meshSize; i++) {
					for (int j = 0; j < meshSize; j++) {
						if (i == j)
							continue;
                        QPair<qint32, qint32> cij = QPair<qint32, qint32>(chatters[i].index, chatters[j].index);
                        if (!existingConnections.contains(cij)) {
							graph()->addConnection(NetGraphConnection(chatters[i].index,
																	  chatters[j].index,
																	  ui->txtConnectionType->text(),
																	  ""));
                            numConnections--;
                        }
					}
				}
				for (int i = 0; i < meshSize; i++) {
					chatters.removeFirst();
				}
			}
		}
	} else if (ui->cmbTypeConnectionsBatch->currentIndex() == 4) {
		// Television
		// Connections from servers to clients
		// A client connects to at most 2 servers
		// Continuous traffic
		QList<NetGraphNode> servers;
		QList<NetGraphNode> clients;
		foreach (NetGraphNode n, hosts) {
			if (!n.iptv)
				continue;
			if (n.server) {
				servers << n;
			} else {
				clients << n;
			}
		}
		if (servers.isEmpty() || clients.isEmpty())
			return;
        QList<QPair<qint32, qint32> > possibleConnections;
		QVector<qint32> nodeDegrees(graph()->nodes.count());
        for (int i = 0; i < servers.count(); i++) {
            for (int j = 0; j < clients.count(); j++) {
                QPair<qint32, qint32> c = QPair<qint32, qint32>(servers[i].index, clients[j].index);
                if (!existingConnections.contains(c)) {
                    possibleConnections << c;
				} else {
					nodeDegrees[c.second]++;
				}
            }
        }
        qShuffle(possibleConnections);
        for (int i = 0; i < numConnections && !possibleConnections.isEmpty(); i++) {
            QPair<qint32, qint32> c = possibleConnections.takeLast();
			if (nodeDegrees[c.second] >= 2) {
				i--;
				continue;
			}
			nodeDegrees[c.second]++;
			graph()->addConnection(NetGraphConnection(c.first, c.second, ui->txtConnectionType->text(), ""));
		}
	} else if (ui->cmbTypeConnectionsBatch->currentIndex() == 5) {
		// Heavy
		// Random connections between heavy nodes and any other nodes
		// Either continuous or on-off traffic (user configurable)
		// Both uploads or downloads (random)
		QList<QPair<qint32, qint32> > possibleConnections;
		for (int i = 0; i < hosts.count(); i++) {
			for (int j = 0; j < hosts.count(); j++) {
				if (i == j)
					continue;
				if ((hosts[i].heavy && !(hosts[j].web || hosts[j].iptv || hosts[j].p2p || hosts[j].vvoip)) ||
						(hosts[j].heavy && !(hosts[i].web || hosts[i].iptv || hosts[i].p2p || hosts[i].vvoip))) {
					QPair<qint32, qint32> c = QPair<qint32, qint32>(hosts[i].index, hosts[j].index);
					if (existingConnections.contains(c))
						continue;
					possibleConnections << c;
				}
			}
		}
		qShuffle(possibleConnections);
		for (int i = 0; i < numConnections && !possibleConnections.isEmpty(); i++) {
			QPair<qint32, qint32> c = possibleConnections.takeLast();
			graph()->addConnection(NetGraphConnection(c.first, c.second, ui->txtConnectionType->text(), ""));
		}
	}
	reloadScene();
}

void NetGraphEditor::updateStats()
{
    // nodes
    int numNodes = graph()->nodes.count();
    int numHosts = graph()->getHostNodes().count();
    int numRouters = numNodes - numHosts;
    ui->txtStatsNumNodes->setText(QString("%1 nodes, %2 routers, %3 hosts")
                                  .arg(numNodes).arg(numRouters).arg(numHosts));

    // links
    int numLinks = graph()->edges.count();
    int numQueues = 0;
    int numInternalLinks = 0;
	int numUsedInternalLinks = 0;
	int numUsedNonNeutralLinks = 0;
    foreach (NetGraphEdge e, graph()->edges) {
        numQueues += e.queueCount;
        if (graph()->nodes[e.source].nodeType != NETGRAPH_NODE_HOST &&
                graph()->nodes[e.dest].nodeType != NETGRAPH_NODE_HOST) {
            numInternalLinks++;
			if (e.used) {
				numUsedInternalLinks++;
				if (!e.isNeutral()) {
					numUsedNonNeutralLinks++;
				}
			}
        }
    }
	ui->txtStatsNumLinks->setText(QString("%1 links, %2 queues, %3 i.l., %4 u.i.l, %5% nn")
								  .arg(numLinks).arg(numQueues).arg(numInternalLinks).arg(numUsedInternalLinks).
								  arg((numUsedNonNeutralLinks * 100) / qMax(numUsedInternalLinks, 1)));

    // paths
    int numPaths = graph()->paths.count();
    qreal pathsPerEdgeRatio = graph()->edges.count() ? numPaths / qreal(numLinks) : 0.0;
    int fullMeshSize = numHosts * numHosts;
    qreal pathsPerFullMeshRatio = fullMeshSize ? numPaths / qreal(fullMeshSize) : 0.0;
    ui->txtStatsNumPaths->setText(QString("%1 paths, %2% (max. %3), %4 paths/link")
                                  .arg(numPaths).arg(int(100 * pathsPerFullMeshRatio)).arg(fullMeshSize)
                                  .arg(pathsPerEdgeRatio, 0, 'f', 1));

    // flows
    int numFlows = graph()->connections.count();
    qreal flowsPerEdgeRatio = graph()->edges.count() ? numFlows / qreal(numLinks) : 0.0;
    qreal flowsPerFullMeshRatio = fullMeshSize ? numFlows / qreal(fullMeshSize) : 0.0;
    ui->txtStatsNumConnections->setText(QString("%1 flows, %2% (f.m. %3), %4 flows/link")
                                        .arg(numFlows).arg(int(100 * flowsPerFullMeshRatio)).arg(fullMeshSize)
                                        .arg(flowsPerEdgeRatio, 0, 'f', 1));
}

void NetGraphEditor::on_btnAddConnectionsF_clicked()
{
	QList<NetGraphNode> hosts = graph()->getHostNodes();

	for (int i = 0; i < hosts.count(); i++) {
		for (int j = 0; j < hosts.count(); j++) {
			if (i != j) {
				graph()->addConnection(NetGraphConnection(hosts[i].index,
														  hosts[j].index,
														  ui->txtConnectionType->text(),
														  ""));
			}
		}
	}
	reloadScene();
}

void NetGraphEditor::on_btnAdjustConnections_toggled(bool checked)
{
	if (checked) {
		setRightPanelPage(ui->pageAdjustConnections);
	}
}

void NetGraphEditor::on_btnAddConnectionsClear_clicked()
{
	while (!graph()->connections.isEmpty()) {
		graph()->deleteConnection(graph()->connections.count() - 1);
	}
	reloadScene();
}

bool NetGraphEditor::colorEdges(QLineEdit *txtIds, QColor c)
{
	QString ids = txtIds->text();
	ids = ids.replace(",", " ");
	ids = ids.replace(".", " ");
	QStringList idList = ids.split(" ", QString::SkipEmptyParts);
	foreach (QString id, idList) {
		bool ok;
		int e = id.toInt(&ok) - 1;
		if (!ok) {
			txtIds->setStyleSheet("color:red") ;
			return false;
		}
		if (e < 0 || e >= graph()->edges.count())
			continue;
		graph()->edges[e].color = c.rgba();
	}
	txtIds->setStyleSheet("color:black") ;
	return true;
}

void NetGraphEditor::on_btnHighlight_clicked()
{
	colorEdges(ui->txtHighlightGray, QColor(160, 160, 160));
	colorEdges(ui->txtHighlightBlack, QColor(0, 0, 0));
	colorEdges(ui->txtHighlightRed, QColor(255, 0, 0));
	colorEdges(ui->txtHighlightGreen, QColor(0, 255, 0));
	colorEdges(ui->txtHighlightBlue, QColor(0, 0, 255));
	colorEdges(ui->txtHighlightYellow, QColor(255, 255, 0));
	scene->updateColors();
}

void NetGraphEditor::on_btnHighlightReset_clicked()
{
	foreach (NetGraphEdge e, graph()->edges) {
		graph()->edges[e.index].color = 0;
	}
	scene->updateColors();
}

void NetGraphEditor::on_btnHighlightGenAll_clicked()
{
	QStringList ids;
	for (int e = 0; e < graph()->edges.count(); e++) {
		ids << QString("%1").arg(e + 1);
	}
	ui->txtHighlightGeneratedIDs->setText(ids.join(" "));
}

void NetGraphEditor::on_btnHighlightGenLossy_clicked()
{
	QStringList ids;
	for (int e = 0; e < graph()->edges.count(); e++) {
		if (1.0 - graph()->edges[e].lossBernoulli < TomoData::goodLinkThresh()) {
			ids << QString("%1").arg(e + 1);
		}
	}
	ui->txtHighlightGeneratedIDs->setText(ids.join(" "));
}

void NetGraphEditor::on_btnHighlightGenGood_clicked()
{
	QStringList ids;
	for (int e = 0; e < graph()->edges.count(); e++) {
		if (1.0 - graph()->edges[e].lossBernoulli >= TomoData::goodLinkThresh()) {
			ids << QString("%1").arg(e + 1);
		}
	}
	ui->txtHighlightGeneratedIDs->setText(ids.join(" "));
}

void NetGraphEditor::on_btnNodeLabels_clicked()
{
	QSet<QString> keySet;
	for (int n = 0; n < graph()->nodes.count(); n++) {
		keySet.unite(graph()->nodes[n].tags.uniqueKeys().toSet());
	}
	if (keySet.isEmpty()) {
		QMessageBox::information(this, "Empty", "No labels found", QMessageBox::Ok);
		return;
	}

	QList<QString> keys = keySet.toList();
	qSort(keys);

	QDialog *form = new QDialog();
	QVBoxLayout *layout = new QVBoxLayout(form);

	QHash<QString, QCheckBox *> key2checkBox;
	foreach (QString key, keys) {
		QCheckBox *checkBox = new QCheckBox(key, form);
		layout->addWidget(checkBox);
		key2checkBox[key] = checkBox;
	}
	form->setLayout(layout);
	form->setWindowTitle("Select node labels to show");
	form->resize(400, 200);
	form->exec();

	// TODO
	QList<QString> selectedKeys;
	foreach (QString key, keys) {
		if (key2checkBox[key]->isChecked()) {
			selectedKeys << key;
		}
	}

	for (int n = 0; n < graph()->nodes.count(); n++) {
		QString label;
		foreach (QString key, selectedKeys) {
			if (graph()->nodes[n].tags.contains(key)) {
				label += key + ": " + graph()->nodes[n].tags[key];
			}
		}
		graph()->nodes[n].customLabel = label;
	}

	delete form;

	reloadScene();
}


void NetGraphEditor::on_btnSaveImage_clicked()
{
	QString imageName;
	{
		QFileDialog fileDialog(0, "Save graph as image");
		fileDialog.setFilter("PNG (*.png)");
		fileDialog.setDefaultSuffix("png");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty()) {
				imageName = fileNames.first();
			}
		}
	}
	if (imageName.isEmpty())
		return;
	QImage image(ui->graphicsView->width(), ui->graphicsView->height(), QImage::Format_RGB32);
	{
		QPainter painter(&image);

		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::TextAntialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
		painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

		QRectF viewRect(0, 0, image.width(), image.height());

		qreal w_screen, h_screen;
		w_screen = image.width();
		h_screen = image.height();

		qreal w_world, h_world;
		w_world = w_screen / scene->graph()->viewportZoom;
		h_world = h_screen / scene->graph()->viewportZoom;

		qreal left, top;
		left = scene->graph()->viewportCenter.x() - w_world/2;
		top = scene->graph()->viewportCenter.y() - h_world/2;

		QRectF sceneRect(left, top, w_world, h_world);

		painter.fillRect(viewRect.toRect(), Qt::white);

		scene->render(&painter, viewRect, sceneRect, Qt::IgnoreAspectRatio);
	}
	image.save(imageName);
}

void NetGraphEditor::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	adjustTableToContents(ui->tableQueueWeights);
	adjustTableToContents(ui->tablePolicerWeights);
}

void NetGraphEditor::on_btnTCPParetoMultiplier_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		if (c.basicType == "TCP-Poisson-Pareto") {
			graph()->connections[c.index].multiplier = ui->spinTCPParetoMultiplier->value();
			graph()->connections[c.index].setTypeFromParams();
		}
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTCPPoissonProb_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		if (c.basicType == "TCP-Poisson-Pareto") {
			graph()->connections[c.index].poissonRate = ui->spinTCPPoissonProb->value();
			graph()->connections[c.index].setTypeFromParams();
		}
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTCPParetoScale_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		if (c.basicType == "TCP-Poisson-Pareto") {
			graph()->connections[c.index].paretoScale_b = dataSizeWithUnit2bits(QString("%1%2").
																				arg(ui->spinTCPParetoScale->value()).
																				arg(ui->cmbTCPParetoScaleUnit->currentText()));
			graph()->connections[c.index].setTypeFromParams();
		}
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTCPParetoExp_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		if (c.basicType == "TCP-Poisson-Pareto") {
			graph()->connections[c.index].paretoAlpha = ui->spinTCPParetoExp->value();
			graph()->connections[c.index].setTypeFromParams();
		}
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTcpDashCongestionControl_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (c.basicType != "TCP-DASH")
            continue;
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        graph()->connections[c.index].tcpCongestionControl = ui->cmbTcpDashCongestionControl->currentText();
        graph()->connections[c.index].setTypeFromParams();
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnTCPDashMultiplier_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        if (c.basicType == "TCP-DASH") {
            graph()->connections[c.index].multiplier = ui->spinTCPDashMultiplier->value();
            graph()->connections[c.index].setTypeFromParams();
        }
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnTCPDashRate_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        if (c.basicType == "TCP-DASH") {
            graph()->connections[c.index].rate_Mbps = ui->spinTCPDashRate->value();
            graph()->connections[c.index].setTypeFromParams();
        }
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnTCPDashBufferingRate_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        if (c.basicType == "TCP-DASH") {
            graph()->connections[c.index].bufferingRate_Mbps = ui->spinTCPDashBufferingRate->value();
            graph()->connections[c.index].setTypeFromParams();
        }
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnTCPDashBufferingTime_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        if (c.basicType == "TCP-DASH") {
            graph()->connections[c.index].bufferingTime_s = ui->spinTCPDashBufferingTime->value();
            graph()->connections[c.index].setTypeFromParams();
        }
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnTCPDashStreamingPeriod_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        if (c.basicType == "TCP-DASH") {
            graph()->connections[c.index].streamingPeriod_s = ui->spinTCPDashStreamingPeriod->value();
            graph()->connections[c.index].setTypeFromParams();
        }
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_cmbConnectionTypeBase_currentIndexChanged(const QString &)
{
    ui->boxTCPParameters->setVisible(ui->cmbConnectionTypeBase->currentText() == "TCP");
	ui->boxTCPPoissonParetoParameters->setVisible(ui->cmbConnectionTypeBase->currentText() == "TCP-Poisson-Pareto");
    ui->boxTcpDashParameters->setVisible(ui->cmbConnectionTypeBase->currentText() == "TCP DASH");
	ui->boxUDPCBRParameters->setVisible(ui->cmbConnectionTypeBase->currentText() == "UDP CBR");
	ui->boxUDPVCBRParameters->setVisible(ui->cmbConnectionTypeBase->currentText() == "UDP VCBR");
	updateTxtConnectionType();
    if (ui->cmbConnectionTypeBase->currentText() == "TCP-Poisson-Pareto" ||
        ui->cmbConnectionTypeBase->currentText() == "TCP DASH") {
		ui->checkConnectionOnOff->setChecked(false);
		ui->checkConnectionOnOff->setEnabled(false);
	} else {
		ui->checkConnectionOnOff->setEnabled(true);
	}
}

void NetGraphEditor::updateTxtConnectionType()
{
	if (ui->cmbConnectionTypeBase->currentText() == "TCP") {
        ui->txtConnectionType->setText(QString("TCP cc %1").
                                       arg(ui->cmbTcpCc->currentText()));
    } else if (ui->cmbConnectionTypeBase->currentText() == "TCP-Poisson-Pareto") {
        ui->txtConnectionType->setText(QString("TCP-Poisson-Pareto %1 %2 %3%4 %5 cc %6").
									   arg(ui->spinPoissonRate->value()).
									   arg(ui->spinParetoExp->value()).
									   arg(ui->spinParetoScale->value()).
									   arg(ui->cmbParetoUnit->currentText()).
                                       arg(ui->checkPoissonSequential->isChecked() ? "sequential" : "").
                                       arg(ui->cmbTcpParetoCc->currentText()));
    } else if (ui->cmbConnectionTypeBase->currentText() == "TCP DASH") {
        ui->txtConnectionType->setText(QString("TCP-DASH %1 %2 %3 %4 cc %5").
                                       arg(ui->spinTcpDashRate->value()).
                                       arg(ui->spinTcpDashBufferingRate->value()).
                                       arg(ui->spinTcpDashBufferingTime->value()).
                                       arg(ui->spinTcpDashStreamingPeriod->value()).
                                       arg(ui->cmbTcpDashCc->currentText()));
    } else if (ui->cmbConnectionTypeBase->currentText() == "UDP CBR") {
		ui->txtConnectionType->setText(QString("UDP-CBR %1 %2").
									   arg(ui->spinUDPCBRRate->value()).
									   arg(ui->checkUDPCBRPoisson->isChecked() ? "poisson" : ""));
	} else if (ui->cmbConnectionTypeBase->currentText() == "UDP VBR") {
		ui->txtConnectionType->setText(QString("UDP VBR"));
	} else if (ui->cmbConnectionTypeBase->currentText() == "UDP VCBR") {
		ui->txtConnectionType->setText(QString("UDP-VCBR %1").
									   arg(ui->spinUDPVCBRRate->value()));
	}

	if (ui->spinConnectionTrafficClass->value() != 0) {
		ui->txtConnectionType->setText(QString("%1 class %2")
									   .arg(ui->txtConnectionType->text())
									   .arg(ui->spinConnectionTrafficClass->value()));
	}

	if (ui->checkConnectionOnOff->isChecked()) {
		ui->txtConnectionType->setText(QString("%1 on-off %2 %3 %4 %5")
									   .arg(ui->txtConnectionType->text())
									   .arg(ui->spinOnDurationMin->value())
									   .arg(ui->spinOnDurationMax->value())
									   .arg(ui->spinOffDurationMin->value())
									   .arg(ui->spinOffDurationMax->value()));
	}

	if (ui->spinConnectionMultiplier->value() > 1) {
		ui->txtConnectionType->setText(QString("%1 x %2")
									   .arg(ui->txtConnectionType->text())
									   .arg(ui->spinConnectionMultiplier->value()));
	}
}

void NetGraphEditor::on_cmbTcpCc_currentIndexChanged(const QString &)
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_cmbTcpParetoCc_currentIndexChanged(const QString &)
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_spinConnectionMultiplier_valueChanged(int)
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinPoissonRate_valueChanged(double )
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_checkPoissonSequential_toggled(bool )
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinParetoExp_valueChanged(double )
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinParetoScale_valueChanged(double )
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_cmbParetoUnit_currentIndexChanged(const QString &)
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_cmbTcpDashCc_currentIndexChanged(int )
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_spinTcpDashRate_valueChanged(double )
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_spinTcpDashBufferingRate_valueChanged(double )
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_spinTcpDashBufferingTime_valueChanged(double )
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_spinTcpDashStreamingPeriod_valueChanged(double )
{
    updateTxtConnectionType();
}

void NetGraphEditor::on_spinUDPCBRRate_valueChanged(double )
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_checkUDPCBRPoisson_toggled(bool )
{
	updateTxtConnectionType();
}

void NetGraphEditor::on_spinUDPVCBRRate_valueChanged(double )
{
	updateTxtConnectionType();
}

void NetGraphEditor::updateTCPPoissonParetoSimText()
{
	const bool debugTimeline = false;
	QStringList result;
	bool ok;
	// bits
	qreal paretoScale = dataSizeWithUnit2bits(QString("%1%2").
											  arg(ui->spinParetoScale->value()).
											  arg(ui->cmbParetoUnit->currentText()),
											  &ok);
	if (!ok) {
		result << "Failed to read params";
		result << "";
		ui->txtTestTCPPoissonParetoCon->setText(result.join("\n"));
		return;
	}

	qreal paretoExp = ui->spinParetoExp->value();
	qreal poissonRate = ui->spinPoissonRate->value();
	qreal bandwidth = ui->spinTestTCPPoissonParetoConBw->value() * 1.0e6; // bps
	bool sequential = ui->checkPoissonSequential->isChecked();

	const qreal tsMax = ui->spinTestTCPPoissonParetoConTime->value();
	const int maxFlows = 1000 * 1000;

	QList<qreal> tsFlowStart;
	QList<qreal> flowSize;

	// Timeline, tick at events
	// (time, value)
	QList<QPair<qreal, qreal> > activeFlowsEventTimeline;
	if (!sequential) {
		{
			bool firstTick = true;
			for (qreal t = frand(); t < tsMax;) {
				if (tsFlowStart.count() >= maxFlows) {
					result << "Maximum flow count exceeded!";
					break;
				}
				if (firstTick) {
					firstTick = false;
				} else {
					// create connection starting at time t
					qreal uniform = 1.0 - frandex();
					qreal transferSize = paretoScale / pow(uniform, (1.0/paretoExp));
					tsFlowStart << t;
					flowSize << transferSize;
					if (debugTimeline) qDebug() << QString("t = %1: flow %2 starts, size = %3").
												   arg(tsFlowStart.last()).
												   arg(flowSize.count()).
												   arg(flowSize.last());
				}
				// Delay for Poisson process
				qreal delay = -log(1.0 - frandex()) / poissonRate;
				t += delay;
			}
		}
		if (flowSize.isEmpty()) {
			result << "No flows!";
			result << "";
			ui->txtTestTCPPoissonParetoCon->setText(result.join("\n"));
			return;
		}

		QList<qreal> leftToTransfer = flowSize;
		for (qreal t = 0; t <= tsMax;) {
			// Compute the number of active flows at time t
			qreal numActiveFlows = 0;
			for (int i = 0; i < tsFlowStart.count(); i++) {
				if (tsFlowStart[i] <= t &&
					leftToTransfer[i] > 0) {
					numActiveFlows += 1.0;
				}
			}
			activeFlowsEventTimeline << QPair<qreal, qreal>(t, numActiveFlows);

			if (t == tsMax) {
				// We are done
				break;
			}

			// Invariant: t < tnext <= tsMax
			qreal tnext = tsMax;
			Q_ASSERT_FORCE(tnext > t);

			// Compute the available bandwidth per flow
			qreal bandwidthPerFlow = numActiveFlows > 0.5 ? bandwidth / numActiveFlows : bandwidth;

			// Compute the soonest next flow start time
			for (int i = 0; i < tsFlowStart.count(); i++) {
				if (tsFlowStart[i] > t) {
					tnext = qMin(tnext, tsFlowStart[i]);
					// tnext > t
					Q_ASSERT_FORCE(tnext > t);
				}
			}
			// Compute the soonest next active flow end time based on the current bandwidth
			for (int i = 0; i < tsFlowStart.count(); i++) {
				if (tsFlowStart[i] <= t &&
					leftToTransfer[i] > 0.5) {
					qreal duration = leftToTransfer[i] / bandwidthPerFlow;
					// duration is > 0
					qreal tend = t + duration;
					tnext = qMin(tnext, tend);
					// tnext > t
					Q_ASSERT_FORCE(tnext > t);
				}
			}

			// Drain active flows up to tnext
			for (int i = 0; i < tsFlowStart.count(); i++) {
				if (tsFlowStart[i] <= t &&
					leftToTransfer[i] > 0.5) {
					leftToTransfer[i] -= (tnext - t) * bandwidthPerFlow;
					if (leftToTransfer[i] < 0.5) {
						leftToTransfer[i] = 0;
					}
					if (leftToTransfer[i] == 0) {
						if (debugTimeline) qDebug() << QString("t = %1: flow %2 ends").
													   arg(tnext).
													   arg(i + 1);
					}
				}
			}
			// Jump to tnext
			t = tnext;
		}
	} else {
		QList<qreal> leftToTransfer;
		bool firstTick = true;
		for (qreal t = 0; t <= tsMax;) {
			// Compute the number of active flows at time t
			qreal numActiveFlows = 0;
			for (int i = 0; i < tsFlowStart.count(); i++) {
				if (tsFlowStart[i] <= t &&
					leftToTransfer[i] > 0) {
					numActiveFlows += 1.0;
				}
			}
			activeFlowsEventTimeline << QPair<qreal, qreal>(t, numActiveFlows);

			if (t == tsMax) {
				// We are done
				break;
			}

			if (firstTick) {
				firstTick = false;
				t = frand();
				continue;
			}

			// Invariant: t < tnext <= tsMax
			qreal tnext = tsMax;
			Q_ASSERT_FORCE(tnext > t);

			if (numActiveFlows == 0) {
				// Delay for Poisson process
				qreal delay = -log(1.0 - frandex()) / poissonRate;
				t += delay;

				if (t < tsMax) {
					// create connection starting at time t
					qreal uniform = 1.0 - frandex();
					qreal transferSize = paretoScale / pow(uniform, (1.0/paretoExp));
					tsFlowStart << t;
					flowSize << transferSize;
					leftToTransfer << flowSize.last();
					if (debugTimeline) qDebug() << QString("t = %1: flow %2 starts, size = %3").
												   arg(tsFlowStart.last()).
												   arg(flowSize.count()).
												   arg(flowSize.last());
				}
			} else {
				// Compute the available bandwidth per flow
				qreal bandwidthPerFlow = numActiveFlows > 0.5 ? bandwidth / numActiveFlows : bandwidth;

				// Compute the soonest next active flow end time based on the current bandwidth
				for (int i = 0; i < tsFlowStart.count(); i++) {
					if (tsFlowStart[i] <= t &&
						leftToTransfer[i] > 0.5) {
						qreal duration = leftToTransfer[i] / bandwidthPerFlow;
						// duration is > 0
						qreal tend = t + duration;
						tnext = qMin(tnext, tend);
						// tnext > t
						Q_ASSERT_FORCE(tnext > t);
					}
				}

				// Drain active flows up to tnext
				for (int i = 0; i < tsFlowStart.count(); i++) {
					if (tsFlowStart[i] <= t &&
						leftToTransfer[i] > 0.5) {
						leftToTransfer[i] -= (tnext - t) * bandwidthPerFlow;
						if (leftToTransfer[i] < 0.5) {
							leftToTransfer[i] = 0;
						}
						if (leftToTransfer[i] == 0) {
							if (debugTimeline) qDebug() << QString("t = %1: flow %2 ends").
														   arg(tnext).
														   arg(i + 1);
						}
					}
				}
				// Jump to tnext
				t = tnext;
			}
		}
	}
	if (debugTimeline) qDebug() << "activeFlowsEventTimeline" << activeFlowsEventTimeline;

	// Process the #active-flows event timeline to generate proper
	// timeline, tick at 1s
	// (time, value)
	QList<QPair<qreal, qreal> > activeFlowsTimeline;
	const qreal activeFlowsTickPeriod = ui->spinTestTCPPoissonParetoConInterval->value();
	activeFlowsTimeline << QPair<qreal, qreal>(0, 0);
	qreal lastValue = 0;
	foreach (RealRealPair event, activeFlowsEventTimeline) {
		qreal t = event.first;
		qreal delta = event.first - activeFlowsTimeline.last().first;
		if (delta >= activeFlowsTickPeriod) {
			delta -= activeFlowsTickPeriod;
			while (delta >= 0) {
				// Example 1:
				// last.t = 5.0, last.val = 777, period = 1.0, t = 8.3, val = 999, delta = 3.3
				// iteration: delta = 2.3, append tsampled = 6.0, val = 777
				// iteration: delta = 1.3, append tsampled = 7.0, val = 777
				// iteration: delta = 0.3, append tsampled = 8.0, val = 999
				// Example 2:
				// last.t = 5.0, last.val = 999, period = 1.0, t = 8.3, val = 777, delta = 3.3
				// iteration: delta = 2.3, append tsampled = 6.0, val = 999
				// iteration: delta = 1.3, append tsampled = 7.0, val = 999
				// iteration: delta = 0.3, append tsampled = 8.0, val = 777
				qreal tsampled = t - delta;
				activeFlowsTimeline << QPair<qreal, qreal>(tsampled, lastValue);
				delta -= activeFlowsTickPeriod;
			}
		}
		delta = event.first - activeFlowsTimeline.last().first;
		Q_ASSERT_FORCE(delta < activeFlowsTickPeriod);
		activeFlowsTimeline.last().second = qMax(activeFlowsTimeline.last().second, event.second);
		lastValue = event.second;
	}
	if (debugTimeline) qDebug() << "activeFlowsTimeline" << activeFlowsTimeline;

	qreal minSize = flowSize.first();
	foreach (qreal value, flowSize) {
		minSize = qMin(minSize, value);
	}

	qreal maxSize = flowSize.first();
	foreach (qreal value, flowSize) {
		maxSize = qMax(maxSize, value);
	}

	qreal avgSize = 0.0;
	foreach (qreal value, flowSize) {
		avgSize += value;
	}
	avgSize /= flowSize.count();

	qreal medianSize = 0.0;
	qreal q1Size = 0.0;
	qreal q3Size = 0.0;
	{
		QList<qreal> values = flowSize;
		qSort(values);
		medianSize = values[values.count() / 2];
		q1Size = values[values.count() / 4];
		q3Size = values[(3 * values.count()) / 4];
	}

	qreal minLoad = activeFlowsTimeline.first().second;
	foreach (RealRealPair event, activeFlowsTimeline) {
		minLoad = qMin(minLoad, event.second);
	}

	qreal maxLoad = activeFlowsTimeline.first().second;
	foreach (RealRealPair event, activeFlowsTimeline) {
		maxLoad = qMax(maxLoad, event.second);
	}

	qreal avgLoad = 0.0;
	foreach (RealRealPair event, activeFlowsTimeline) {
		avgLoad += event.second;
	}
	avgLoad /= activeFlowsTimeline.count();

	qreal medianLoad = 0.0;
	qreal q1Load = 0.0;
	qreal q3Load = 0.0;
	{
		QList<qreal> values;
		foreach (RealRealPair event, activeFlowsTimeline) {
			values << event.second;
		}
		qSort(values);
		medianLoad = values[values.count() / 2];
		q1Load = values[values.count() / 4];
		q3Load = values[(3 * values.count()) / 4];
	}

	result << QString("Flows started: %L1 (%L2 flows/s)").
			  arg(tsFlowStart.count()).
			  arg(qreal(tsFlowStart.count()) / tsMax, 0, 'f', 1);
	result << "";

	QString sizeUnit = ui->cmbParetoUnit->currentText();
	qreal sizeUnitMultiplier = dataSizeWithUnit2bits(QString("1%1").
													 arg(ui->cmbParetoUnit->currentText()),
													 &ok);
	Q_ASSERT_FORCE(ok);
	result << QString("Transfer size: %L1 - %L2 %3").
			  arg(minSize / sizeUnitMultiplier, 0, 'f', 1).
			  arg(maxSize / sizeUnitMultiplier, 0, 'f', 1).
			  arg(sizeUnit);
	result << QString("Transfer size avg.: %L1 %2").
			  arg(avgSize / sizeUnitMultiplier, 0, 'f', 1).
			  arg(sizeUnit);

	result << QString("Transfer size median: %L1 %2").
			  arg(medianSize / sizeUnitMultiplier, 0, 'f', 1).
			  arg(sizeUnit);

	result << QString("Transfer size 50% of flows: %L1 - %L2 %3").
			  arg(q1Size / sizeUnitMultiplier, 0, 'f', 1).
			  arg(q3Size / sizeUnitMultiplier, 0, 'f', 1).
			  arg(sizeUnit);
	result << "";

	result << QString("Load: %L1 - %L2 flows").
			  arg(minLoad, 0, 'f', 1).
			  arg(maxLoad, 0, 'f', 1);
	result << QString("Load avg.: %L1 flows").
			  arg(avgLoad, 0, 'f', 1);

	result << QString("Load median: %L1 flows").
			  arg(medianLoad, 0, 'f', 1);

	result << QString("Load 50% of time: %L1 - %L2 flows").
			  arg(q1Load, 0, 'f', 1).
			  arg(q3Load, 0, 'f', 1);
	result << "";

	result << QString("Load: %L1 - %L2 flows/Mbps").
			  arg(minLoad / (bandwidth * 1.0e-6), 0, 'f', 1).
			  arg(maxLoad / (bandwidth * 1.0e-6), 0, 'f', 1);
	result << QString("Load avg.: %L1 flows/Mbps").
			  arg(avgLoad / (bandwidth * 1.0e-6), 0, 'f', 1);

	result << QString("Load median: %L1 flows/Mbps").
			  arg(medianLoad / (bandwidth * 1.0e-6), 0, 'f', 1);

	result << QString("Load 50% of time: %L1 - %L2 flows/Mbps").
			  arg(q1Load / (bandwidth * 1.0e-6), 0, 'f', 1).
			  arg(q3Load / (bandwidth * 1.0e-6), 0, 'f', 1);
	result << "";

	ui->txtTestTCPPoissonParetoCon->setText(result.join("\n"));

	// number of flows plot
	QOPlotCurveData *nFlowsPlotData = new QOPlotCurveData;
	nFlowsPlotData->x.reserve(activeFlowsTimeline.count());
	nFlowsPlotData->y.reserve(activeFlowsTimeline.count());
	for (int i = 0; i < activeFlowsTimeline.count(); i++) {
		nFlowsPlotData->x << activeFlowsTimeline.at(i).first;
		nFlowsPlotData->y << activeFlowsTimeline.at(i).second;
	}

	if (!nFlowsPlot) {
		nFlowsPlot = new QOPlotWidget(NULL, 300, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		nFlowsPlot->resize(800, 400);
		nFlowsPlot->move(100, 200);
		nFlowsPlot->setWindowFlags(nFlowsPlot->windowFlags() | Qt::WindowStaysOnTopHint);
		nFlowsPlot->setWindowTitle("TCP Poisson-Pareto simulation - number of flows");
	}
	nFlowsPlot->plot.title = QString("Simulated number of concurrent flows (bw = %1 Mbps, lambda = %2 flows/s, index = %3, scale = %4 %5)").
							 arg(ui->spinTestTCPPoissonParetoConBw->value()).
							 arg(poissonRate).
							 arg(paretoExp).
							 arg(ui->spinParetoScale->value()).
							 arg(ui->cmbParetoUnit->currentText());
	nFlowsPlot->plot.xlabel = "Time (s)";
	nFlowsPlot->plot.ylabel = "Flows";
	nFlowsPlot->plot.data.clear();
	nFlowsPlot->plot.addData(nFlowsPlotData);
	nFlowsPlot->plot.drag_y_enabled = false;
	nFlowsPlot->plot.zoom_y_enabled = false;
	nFlowsPlot->autoAdjustAxes();
	nFlowsPlot->drawPlot();
	nFlowsPlot->show();

	// loss plot
	QOPlotCurveData *lossPlotData = new QOPlotCurveData;
	lossPlotData->x.reserve(activeFlowsTimeline.count());
	lossPlotData->y.reserve(activeFlowsTimeline.count());
	for (int i = 0; i < activeFlowsTimeline.count(); i++) {
		lossPlotData->x << activeFlowsTimeline.at(i).first;
		lossPlotData->y << (1.6 * activeFlowsTimeline.at(i).second / (bandwidth * 1.0e-6));
	}

	if (!lossPlot) {
		lossPlot = new QOPlotWidget(NULL, 300, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		lossPlot->resize(800, 400);
		lossPlot->move(100, 600);
		lossPlot->setWindowFlags(nFlowsPlot->windowFlags() | Qt::WindowStaysOnTopHint);
		lossPlot->setWindowTitle("TCP Poisson-Pareto simulation - loss rate");
	}
	lossPlot->plot.title = QString("Simulated loss rate (bw = %1 Mbps, lambda = %2 flows/s, index = %3, scale = %4 %5)").
						   arg(ui->spinTestTCPPoissonParetoConBw->value()).
						   arg(poissonRate).
						   arg(paretoExp).
						   arg(ui->spinParetoScale->value()).
						   arg(ui->cmbParetoUnit->currentText());
	lossPlot->plot.xlabel = "Time (s)";
	lossPlot->plot.ylabel = "Loss rate (%)";
	lossPlot->plot.data.clear();
	lossPlot->plot.addData(lossPlotData);
	lossPlot->plot.drag_y_enabled = false;
	lossPlot->plot.zoom_y_enabled = false;
	lossPlot->autoAdjustAxes();
	lossPlot->drawPlot();
	lossPlot->show();
}

void NetGraphEditor::on_spinTestTCPPoissonParetoConBw_valueChanged(double )
{
}

void NetGraphEditor::on_btnTestTCPPoissonParetoConApply_clicked()
{
	updateTCPPoissonParetoSimText();
}

void NetGraphEditor::on_spinTestTCPPoissonParetoConInterval_valueChanged(double )
{
}

void NetGraphEditor::on_spinTestTCPPoissonParetoConTime_valueChanged(double )
{
}

void NetGraphEditor::on_btnEditBatchLinks_clicked()
{
	foreach (NetGraphEdge e, graph()->edges) {
		bool internalLink = graph()->nodes[e.source].nodeType != NETGRAPH_NODE_HOST &&
																 graph()->nodes[e.dest].nodeType != NETGRAPH_NODE_HOST;
		if (internalLink && !ui->spinSetPropInternalLinks->isChecked())
			continue;
		if (!internalLink && !ui->spinSetPropGatewayLinks->isChecked())
			continue;
		qreal bwMbps = randInt(ui->spinRandomSpeedMin->value(), ui->spinRandomSpeedMax->value());
		qreal step = 1.0;
		if (ui->spinRandomSpeedMax->value() - ui->spinRandomSpeedMin->value() >= 50) {
			step = 10.0;
		} else if (ui->spinRandomSpeedMax->value() - ui->spinRandomSpeedMin->value() >= 20) {
			step = 5.0;
		}
		if (step > 1) {
			bwMbps = round(bwMbps / step) * step;
			bwMbps = qMax(bwMbps, ui->spinRandomSpeedMin->value());
			bwMbps = qMin(bwMbps, ui->spinRandomSpeedMax->value());
		}
		graph()->edges[e.index].bandwidth = bwMbps * 1000.0 / 8.0;
		graph()->edges[e.index].delay_ms = randInt(ui->spinRandomDelayMin->value(), ui->spinRandomDelayMax->value());
		graph()->edges[e.index].queueLength = NetGraph::optimalQueueLength(graph()->edges[e.index].bandwidth, graph()->edges[e.index].delay_ms);
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTCPPoissonBalanceRates_clicked()
{
	const bool onlyAccessLinks = false;

	graph()->computeRoutes();

	QHash<qint32, int> numFlowsPerLink;
	QHash<qint32, QSet<qint32> > link2connections;
	QHash<qint32, qreal> connection2rate;
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		if (c.basicType != "TCP-Poisson-Pareto")
			continue;
		bool ok;
		NetGraphPath p = graph()->pathByNodeIndexTry(c.source, c.dest, ok);
		if (!ok)
			continue;
		connection2rate[c.index] = c.poissonRate;
		foreach (NetGraphEdge e, p.edgeList) {
			numFlowsPerLink[e.index] += 1;
			link2connections[e.index].insert(c.index);
		}
	}
	qreal totalPoissonRate = ui->spinTCPPoissonRateBalance->value();
	qDebug() << "totalPoissonRate" << totalPoissonRate;
	foreach (qint32 e, link2connections.uniqueKeys()) {
		bool isAccessLink = graph()->nodes[graph()->edges[e].source].nodeType == NETGRAPH_NODE_HOST ||
							graph()->nodes[graph()->edges[e].dest].nodeType == NETGRAPH_NODE_HOST;
		if (onlyAccessLinks && !isAccessLink)
			continue;
		qreal poissonRate = totalPoissonRate/numFlowsPerLink[e];
		foreach (qint32 ic, link2connections[e]) {
			connection2rate[ic] = qMin(connection2rate[ic], poissonRate);
		}
	}
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		if (c.basicType != "TCP-Poisson-Pareto")
			continue;
		graph()->connections[c.index].poissonRate = connection2rate[c.index];
		graph()->connections[c.index].setTypeFromParams();
	}

	QHash<qint32, qreal> link2totalRate;
	foreach (qint32 e, link2connections.uniqueKeys()) {
		foreach (qint32 c, link2connections[e]) {
			link2totalRate[e] += connection2rate[c];
		}
	}
	QMultiMap<qreal, qint32> totalRate2link;
	foreach (qint32 e, link2connections.uniqueKeys()) {
		qreal r = link2totalRate[e];
		totalRate2link.insert(r, e);
	}
	foreach (qreal r, totalRate2link.uniqueKeys()) {
		foreach (qint32 e, totalRate2link.values(r)) {
			qDebug() << QString("%1 total poisson rate for link %2, type %3, %4")
						.arg(r, 5, 'f', 2)
						.arg(e + 1, 3)
						.arg((graph()->nodes[graph()->edges[e].source].nodeType == NETGRAPH_NODE_HOST ||
							  graph()->nodes[graph()->edges[e].dest].nodeType == NETGRAPH_NODE_HOST) ? "access" : "internal", 8)
						.arg(graph()->edges[e].isNeutral() ? "neutral" : "non-neutral", 11);
		}
	}

	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnGenericMultiplier_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		graph()->connections[c.index].multiplier = ui->spinGenericMultiplier->value();
		graph()->connections[c.index].setTypeFromParams();
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTcpCongestionCOntrol_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
        if (c.basicType != "TCP")
			continue;
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
		graph()->connections[c.index].tcpCongestionControl = ui->cmbTcpCongestionControl->currentText();
		graph()->connections[c.index].setTypeFromParams();
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTcpParetoCongestionControl_clicked()
{
    foreach (NetGraphConnection c, graph()->connections) {
        if (c.basicType != "TCP-Poisson-Pareto")
            continue;
        if (!ui->checkAdjustClass0->isChecked() &&
            c.trafficClass == 0)
            continue;
        if (!ui->checkAdjustClass1->isChecked() &&
            c.trafficClass == 1)
            continue;
        graph()->connections[c.index].tcpCongestionControl = ui->cmbTcpParetoCongestionControl->currentText();
        graph()->connections[c.index].setTypeFromParams();
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnTCPxMultiplier_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
        if (c.basicType == "TCP") {
			graph()->connections[c.index].multiplier = ui->spinTCPxMultiplier->value();
			graph()->connections[c.index].setTypeFromParams();
		}
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTCPxOnOff_clicked()
{
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
        if (c.basicType == "TCP") {
			graph()->connections[c.index].onOff = ui->checkTCPxOnOff->isChecked();
			graph()->connections[c.index].onDurationMin = ui->spinOnDurationMinAdjust->value();
			graph()->connections[c.index].onDurationMax = ui->spinOnDurationMaxAdjust->value();
			graph()->connections[c.index].offDurationMin = ui->spinOffDurationMinAdjust->value();
			graph()->connections[c.index].offDurationMax = ui->spinOffDurationMaxAdjust->value();
		}
	}
	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnTCPxMultiplierBalance_clicked()
{
	const bool onlyAccessLinks = false;

	graph()->computeRoutes();

	QHash<qint32, int> numFlowsPerLink;
	QHash<qint32, QSet<qint32> > link2connections;
	QHash<qint32, int> connection2rate;
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
        if (c.basicType != "TCP")
			continue;
		bool ok;
		NetGraphPath p = graph()->pathByNodeIndexTry(c.source, c.dest, ok);
		if (!ok)
			continue;
		connection2rate[c.index] = c.multiplier;
		foreach (NetGraphEdge e, p.edgeList) {
			numFlowsPerLink[e.index] += 1;
			link2connections[e.index].insert(c.index);
		}
	}
	int totalRate = ui->spinTCPxMultiplierBalance->value();
	qDebug() << "totalRate" << totalRate;
	foreach (qint32 e, link2connections.uniqueKeys()) {
		bool isAccessLink = graph()->nodes[graph()->edges[e].source].nodeType == NETGRAPH_NODE_HOST ||
							graph()->nodes[graph()->edges[e].dest].nodeType == NETGRAPH_NODE_HOST;
		if (onlyAccessLinks && !isAccessLink)
			continue;
		int rate = qMax(1, totalRate/numFlowsPerLink[e]);
		foreach (qint32 ic, link2connections[e]) {
			connection2rate[ic] = qMin(connection2rate[ic], rate);
		}
	}
	foreach (NetGraphConnection c, graph()->connections) {
		if (!ui->checkAdjustClass0->isChecked() &&
			c.trafficClass == 0)
			continue;
		if (!ui->checkAdjustClass1->isChecked() &&
			c.trafficClass == 1)
			continue;
        if (c.basicType != "TCP")
			continue;
		graph()->connections[c.index].multiplier = connection2rate[c.index];
		graph()->connections[c.index].setTypeFromParams();
	}

	QHash<qint32, int> link2totalRate;
	foreach (qint32 e, link2connections.uniqueKeys()) {
		foreach (qint32 c, link2connections[e]) {
			link2totalRate[e] += connection2rate[c];
		}
	}
	QMultiMap<int, qint32> totalRate2link;
	foreach (qint32 e, link2connections.uniqueKeys()) {
		int r = link2totalRate[e];
		totalRate2link.insert(r, e);
	}
	foreach (qreal r, totalRate2link.uniqueKeys()) {
		foreach (qint32 e, totalRate2link.values(r)) {
			qDebug() << QString("%1 total poisson rate for link %2, type %3, %4")
						.arg(r, 3)
						.arg(e + 1, 3)
						.arg((graph()->nodes[graph()->edges[e].source].nodeType == NETGRAPH_NODE_HOST ||
							  graph()->nodes[graph()->edges[e].dest].nodeType == NETGRAPH_NODE_HOST) ? "access" : "internal", 8)
						.arg(graph()->edges[e].isNeutral() ? "neutral" : "non-neutral", 11);
		}
	}

	setModified();
	reloadScene();
}

void NetGraphEditor::on_btnRemoveHosts_clicked()
{
    for (int n = graph()->nodes.count() - 1; n >= 0; n--) {
        if (graph()->nodes[n].nodeType == NETGRAPH_NODE_HOST) {
            graph()->deleteNode(n);
        }
    }
    setModified();
    reloadScene();
}

void NetGraphEditor::on_btnAddExtraHosts_clicked()
{
    int minHosts = ui->spinExtraHostsMin->value();
    int maxHosts = ui->spinExtraHostsMax->value();
    maxHosts = qMax(minHosts, maxHosts);
    foreach (NetGraphNode n, graph()->nodes) {
        if (n.nodeType == NETGRAPH_NODE_GATEWAY) {
            int numExtra = randInt(minHosts, maxHosts);
            Q_ASSERT_FORCE(numExtra >= minHosts && numExtra <= maxHosts);
            double angle = randInt(1, 10) * 6.28 / 10;
            for (int j = 0; j < numExtra; j++) {
                int host = graph()->addNode(NETGRAPH_NODE_HOST,
                                            QPointF(n.x + 100 * cos(angle + j * 6.28 / numExtra),
                                                    n.y + 100 * sin(angle + j * 6.28 / numExtra)),
                                            n.ASNumber);
                graph()->addEdgeSym(n.index, host, 300, 1, 0, 20);
            }
        }
    }
    setModified();
    reloadScene();
}



