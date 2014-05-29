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

#ifndef NETGRAPHEDITOR_H
#define NETGRAPHEDITOR_H

#include <QWidget>

#include "netgraphscene.h"
#include "netgraphphasing.h"

namespace Ui {
class NetGraphEditor;
}

class QGraphicsViewGood;
class QOPlotWidget;

class NetGraphEditor : public QWidget
{
	Q_OBJECT
	
public:
	explicit NetGraphEditor(QWidget *parent = 0);
	~NetGraphEditor();

	// don't delete this pointer or you will burn in hell
	NetGraph *graph();

signals:
	void renamed(QString newName);

public slots:
	bool loadGraph(QString fileName);
	// does not take ownership of the pointer, it simply copies the graph
	void loadGraph(NetGraph *graph);
	void resetScene();
	void reloadScene();
	void setModified();
	void addPhaseGroup(NetGraphPhaseGroup group);
	void clearPhaseGroups();
	void clear();
	bool save();
    bool saveAs();
	bool saveNoGUI(); // can be called from non-GUI threads
    void askSave();
	void setReadOnly(bool value);
	bool saveSVG(QString fileName);

	void on_btnTCPPoissonBalanceRates_clicked();
	void on_btnTCPxMultiplierBalance_clicked();

private slots:
	void onWheelEventFired(QGraphicsSceneWheelEvent *event);
    void hostSelected(NetGraphNode host);
	void edgeSelected(NetGraphEdge edge);
	bool computeRoutes(NetGraph *netGraph, bool full = false);
	void autoAdjustQueue();
	void updateZoom();
	void onSamplingPeriodChanged(quint64 val);
    void onGraphChanged();
	void setRightPanelPage(QWidget *widget);
	void on_btnShowRightPanel_clicked();
	void on_btnNew_clicked();
	void on_btnOpen_clicked();
	bool on_btnSave_clicked();
	bool on_btnSaveAs_clicked();
	void on_btnLayout_clicked();
	void on_btnSCCDomains_clicked();
	void on_btnAddHost_toggled(bool checked);
	void on_btnAddGateway_toggled(bool checked);
	void on_btnAddRouter_toggled(bool checked);
	void on_btnAddBorderRouter_toggled(bool checked);
	void on_btnMoveNode_toggled(bool checked);
	void on_btnDelNode_toggled(bool checked);
	void on_btnDelNodeArea_toggled(bool checked);
	void on_btnAddEdge_toggled(bool checked);
	void on_btnEditEdge_toggled(bool checked);
	void on_btnDelEdge_toggled(bool checked);
	void on_btnAddConnection_toggled(bool checked);
	void on_btnDelConnection_toggled(bool checked);
	void on_btnShowRoutes_toggled(bool checked);
	void on_btnComputeRoutes_clicked();
	void on_btnComputeAllRoutes_clicked();
	void on_btnDrag_toggled(bool checked);
	void on_checkUnusedHidden_toggled(bool checked);
	void on_checkHideConnections_toggled(bool checked);
	void on_checkHideFlows_toggled(bool checked);
	void on_checkHideEdges_toggled(bool checked);
	void on_checkDomains_toggled(bool checked);
	void on_cmbFastMode_currentIndexChanged(const QString &arg1);
	void on_spinBandwidth_valueChanged(double arg1);
	void on_spinDelay_valueChanged(int arg1);
	void on_spinLoss_valueChanged(double arg1);
	void onSpinQueueWeight_valueChanged(double arg1);
	void onSpinPolicerWeight_valueChanged(double arg1);
	void on_spinQueueLength_valueChanged(int arg1);
	void on_txtSamplingPeriod_textChanged(const QString &arg1);
	void on_checkSampledTimeline_toggled(bool checked);
	void on_checkAutoQueue_toggled(bool checked);
	void on_spinASNumber_valueChanged(int arg1);
	void on_txtConnectionType_textChanged(const QString &arg1);
	void on_spinZoom_valueChanged(double arg1);
	void on_cmbPhaseGroup_currentIndexChanged(int index);
	void on_listPhases_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
	void on_btnPhasing_clicked();
	void on_btnFit_clicked();
	void on_btnAddConnections10_clicked();
	void on_btnAddConnectionsF_clicked();
	void on_btnAddConnectionsClear_clicked();
	bool colorEdges(QLineEdit *txtIds, QColor c);
	void on_btnHighlight_clicked();
	void on_btnHighlightGenAll_clicked();
	void on_btnHighlightGenLossy_clicked();
	void on_btnHighlightGenGood_clicked();
	void on_btnHighlightEdges_toggled(bool checked);
	void on_btnNodeLabels_clicked();
	void on_btnMoveNodeArea_toggled(bool checked);
    void on_spinConnectionTrafficClass_valueChanged(int arg1);
    void on_spinEdgeQueues_valueChanged(int val);
    void on_checkConnectionOnOff_toggled(bool checked);
    void on_btnAddConnections100_clicked();
    void updateStats();
    void on_btnEditHost_toggled(bool checked);
    void on_checkHostWeb_toggled(bool checked);
    void on_checkHostP2P_toggled(bool checked);
    void on_checkHostVoiceVideo_toggled(bool checked);
    void on_checkHostTv_toggled(bool checked);
	void on_checkHostServer_toggled(bool checked);
	void on_btnAddConnectionsBatch_clicked();
	void on_checkHostHeavy_toggled(bool checked);
	void setSceneTrafficFlags();
	void on_btnSaveImage_clicked();

	void on_spinEdgePolicers_valueChanged(int arg1);

	void on_spinOnDurationMin_valueChanged(double arg1);

	void on_spinOnDurationMax_valueChanged(double arg1);

	void on_spinOffDurationMin_valueChanged(double arg1);

	void on_spinOffDurationMax_valueChanged(double arg1);

	void on_btnAdjustConnections_toggled(bool checked);

	void on_btnTCPPoissonProb_clicked();

	void on_btnTCPParetoScale_clicked();

	void on_btnTCPParetoExp_clicked();

	void on_cmbConnectionTypeBase_currentIndexChanged(const QString &arg1);
	void updateTxtConnectionType();

	void on_spinPoissonRate_valueChanged(double arg1);

	void on_spinParetoExp_valueChanged(double arg1);

	void on_spinParetoScale_valueChanged(double arg1);

	void on_cmbParetoUnit_currentIndexChanged(const QString &arg1);

	void on_spinUDPCBRRate_valueChanged(double arg1);

	void on_checkUDPCBRPoisson_toggled(bool checked);

	void on_spinUDPVCBRRate_valueChanged(double arg1);

	void on_spinTestTCPPoissonParetoConBw_valueChanged(double arg1);

	void updateTCPPoissonParetoSimText();

	void on_btnTestTCPPoissonParetoConApply_clicked();

	void on_spinTestTCPPoissonParetoConInterval_valueChanged(double arg1);

	void on_spinTestTCPPoissonParetoConTime_valueChanged(double arg1);

	void on_btnEditBatchLinks_clicked();

	void on_btnTCPxMultiplier_clicked();

	void on_btnTCPxOnOff_clicked();

	void on_btnHighlightReset_clicked();

	void on_checkPoissonSequential_toggled(bool checked);

	void on_btnGenericMultiplier_clicked();

	void on_spinConnectionMultiplier_valueChanged(int arg1);

	void on_btnTCPParetoMultiplier_clicked();

	void on_btnTcpCongestionCOntrol_clicked();

    void on_btnRemoveHosts_clicked();

    void on_btnAddExtraHosts_clicked();

    void on_cmbTcpCc_currentIndexChanged(const QString &arg1);

    void on_cmbTcpParetoCc_currentIndexChanged(const QString &arg1);

    void on_btnTcpParetoCongestionControl_clicked();

    void on_cmbTcpDashCc_currentIndexChanged(int index);

    void on_spinTcpDashRate_valueChanged(double arg1);

    void on_spinTcpDashBufferingRate_valueChanged(double arg1);

    void on_spinTcpDashBufferingTime_valueChanged(double arg1);

    void on_spinTcpDashStreamingPeriod_valueChanged(double arg1);

    void on_btnTcpDashCongestionControl_clicked();

    void on_btnTCPDashMultiplier_clicked();

    void on_btnTCPDashRate_clicked();

    void on_btnTCPDashBufferingRate_clicked();

    void on_btnTCPDashBufferingTime_clicked();

    void on_btnTCPDashStreamingPeriod_clicked();

protected:
	virtual void resizeEvent(QResizeEvent* event);

private:
	Ui::NetGraphEditor *ui;
	NetGraphScene *scene;
	QGraphicsViewGood *graphicsView;
	QSharedPointer<NetGraph> nonPhasedGraph;
	QList<NetGraphPhaseGroup> phaseGroups;
    bool changesNotSaved;
	QOPlotWidget *nFlowsPlot;
	QOPlotWidget *lossPlot;
};

#endif // NETGRAPHEDITOR_H
