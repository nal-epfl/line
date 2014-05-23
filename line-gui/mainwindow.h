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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include <QFutureWatcher>
#include <QFileSystemWatcher>

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <float.h>
#include <math.h>

#include "netgrapheditor.h"
#include "netgraphscene.h"
#include "briteimporter.h"
#include "graphmlimporter.h"
#include "util.h"
#include "remoteprocessssh.h"
#include "qoplot.h"
#include "qdisclosure.h"
#include "qaccordion.h"
#include "../tomo/tomodata.h"
#include "writelog.h"
#include "run_experiment_params.h"
#include "../remote_config.h"

#ifdef HAVE_CUSTOM_CONTROLS
#include "customcontrols.h"
class CustomControls;
#endif

#define UDPPING_OVERHEAD 42

#define SEC_TO_NSEC  1000000000
#define MSEC_TO_NSEC 1000000
#define USEC_TO_NSEC 1000

#define TAB_INDEX_TOPOLOGY   0
#define TAB_INDEX_BRITE      1
#define TAB_INDEX_DEPLOY     2
#define TAB_INDEX_VALIDATION 3
#define TAB_INDEX_RUN        4
#define TAB_INDEX_RESULTS    5
#define TAB_INDEX_EXPERIMENT 6

#define TAB_COLOR_ERROR Qt::red
#define TAB_COLOR_CHANGED Qt::darkGreen
#define TAB_COLOR_DEFAULT Qt::black

namespace Ui {
	class MainWindow;
}

class PathDelayMeasurement {
public:
	double fwdDelay;
	double backDelay;
	int frameSize;
};

class PathCongestionMeasurement {
public:
	double trafficBandwidth;
	double loss;
	int frameSize;
};

class PathQueueMeasurement {
public:
	double burstSize;
	double loss;
	int frameSize;
};

class Simulation {
public:
	explicit Simulation();
	explicit Simulation(QString dir);
	QString dir;
	QString graphName;
};

class QSwiftProgressDialog;

void dumpTomoData(TomoData &data, QString outputFile);

class SetLinkPropertiesParams {
public:
	SetLinkPropertiesParams(qreal minSpeedMbps,
							qreal maxSpeedMbps,
							int minDelayMs,
							int maxDelayMs,
							bool internalLinksOnly,
							bool gatewayLinksOnly)
		: minSpeedMbps(minSpeedMbps),
		  maxSpeedMbps(maxSpeedMbps),
		  minDelayMs(minDelayMs),
		  maxDelayMs(maxDelayMs),
		  internalLinksOnly(internalLinksOnly),
		  gatewayLinksOnly(gatewayLinksOnly) {}

	qreal minSpeedMbps;
	qreal maxSpeedMbps;
	int minDelayMs;
	int maxDelayMs;
	bool internalLinksOnly;
	bool gatewayLinksOnly;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	typedef void (MainWindow::*MainWindowCallback)(QString);

	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void safeLogInformation(QPlainTextEdit *log, QString message);
	void safeLogError(QPlainTextEdit *log, QString message);
	void safeLogSuccess(QPlainTextEdit *log, QString message);
	void safeLogText(QPlainTextEdit *log, QString message);
	void safeLogOutput(QPlainTextEdit *log, QString message);
	void safeLogClear(QPlainTextEdit *log);
protected:
	void closeEvent(QCloseEvent *event);

private:
	Ui::MainWindow *ui;
#ifdef HAVE_CUSTOM_CONTROLS
	CustomControls *customControls;
	friend class CustomControls;
#endif

	int maxLogLines;

	// *** Topology editor
	NetGraphEditor *editor;

	NetGraphEditor *treeEditor;

	QFileSystemWatcher topoDirWatcher;

	QFutureWatcher<void> voidWatcher;
	BriteImporter briteImporter;
	GraphMLImporter graphMLImporter;

	QList<Simulation> simulations;
	int currentSimulation;
	int currentTopology;
	bool ignoreDirChanges;
	quint64 lastDirChangeNs;
	QString rootPath;

	double simple_bw_KBps;
	int simple_delay_ms;
	bool simple_jitter;
	int simple_queueSize;
	double simple_bufferbloat_factor; // 1 = no bloat

	int currentRepeat;
	bool mustStop;
	bool blocking;

	QList<RunParams> experimentQueue;
	bool runningInQueue;

	QProcess httpServer;

	QHash<QPlainTextEdit*, QTextCursor> logOutputPositions;
	QOPlotGroup resultsPlotGroup;

private slots:
	void initPlots();
	void on_actionStop_triggered();
	void blockingOperationStarting();
    void blockingOperationFinished(bool blink = true);
    void blinkTaskbar();
	QString getGraphName(NetGraph *netGraph);
	QString getCoreHostname();
    QString getCorePort();
    QString getClientHostname();
    QString getClientPort();
	void startRedirectingQDebug(QPlainTextEdit *log);
	void stopRedirectingQDebug();
    void doLogInformation(QPlainTextEdit *log, QString message);
	void doLogError(QPlainTextEdit *log, QString message);
	void doLogSuccess(QPlainTextEdit *log, QString message);
	void doLogText(QPlainTextEdit *log, QString message);
	void doLogOutput(QPlainTextEdit *log, QString message);
	void doLogClear(QPlainTextEdit *log);
	void doLogBriteInfo(QString s);
	void doLogBriteError(QString s);
	void doLogTabChanged(QPlainTextEdit *log, QColor color = TAB_COLOR_CHANGED);
	void updateTimeBox(QLineEdit *txt, bool &accepted, quint64 &value);
	void on_tabWidget_currentChanged(int index);
	void doTabChanged(QWidget *page, QColor color);
	void topoDirChanged(QString path);

	// *** Open/save
	// newName must be an absolute path
	void onRename(QString newName);

	// *** Topology editor

	// *** Import
	void on_btnBriteImport_clicked();
	void on_btnBRITEBatchImport_clicked();
	void importBrite(QString fromFileName, QString toFileName);
	void importBriteMulti(QStringList fromFileNames, QStringList toFileNames);
	void importGraphML(QString fromFileName, QString toFileName);
	void importGraphMLMulti(QStringList fromFileNames, QStringList toFileNames);
	void layoutGraph();
	void layoutGraphUsed();
	void doLayoutGraph();
	void onLayoutFinished();
	void doRecomputeRoutes();
	void recomputeRoutes();
	void onRoutingFinished();
	void onImportFinished(QString fileName);
	void on_btnGraphMLBatchImport_clicked();
	void on_btnGraphMLImport_clicked();
	void on_btnGraphMLImportMerge_clicked();
    void on_btnImportTraces_clicked();

	// *** Batch
	void onBtnGenericFinished(QString testId);
	void on_btnGenericStop_clicked();
	void on_checkGenericTimeout_toggled(bool checked);
	void generateSimpleTopology(int pairs);
	void on_txtGenericTimeout_textChanged(const QString &arg1);
	bool experimentOk(QString testId, QString &reason, double delayErrorLimit = 0.2);
	void saveExperimentQueue();
	void loadExperimentQueue();
	bool runLongCommand(QString path, QStringList args, QPlainTextEdit *log);
	bool lineRunner(QStringList args, QPlainTextEdit *log);

	// *** Deployment
	bool getRunParams(RunParams &params, QPlainTextEdit *log);
	void setUIToRunParams(const RunParams &params);
	void on_btnDeploy_clicked();
	bool computeRoutes(NetGraph *netGraph);
	bool computeFullRoutes(NetGraph *netGraph);

	// *** Remote
	bool runCommand(QPlainTextEdit *log, QString command, QStringList args, QString description);
	bool runCommand(QPlainTextEdit *log, QString command, QStringList args, QString description, bool showOutput, QString &output);
	bool startProcess(QPlainTextEdit *log, QProcess &process, QString command, QStringList args, QString description, QString &output);
	bool stopProcess(QPlainTextEdit *log, QProcess &process, QString description, QString stopCommand, QString &output);

	// *** Results
	void doReloadTopologyList(QString graphName);
	void doReloadSimulationList(QString testId);
	void loadSimulation();
	void on_cmbSimulation_currentIndexChanged(int index);
	void on_cmbTopologies_currentIndexChanged(int index);
	bool processCapture(QString expPath, int &packetCount, int &queueEventCount);
	void openCapture(QString expPath);
	QString getCurrentSimulation();

	void on_btnCaptureProcess_clicked();

	void on_checkCapture_toggled(bool checked);

    void on_txtIntervalSize_textChanged(const QString &arg1);

	void on_btnListTopoBySize_clicked();
    void onTopoSizeComputed(QString name,
                            int numNodes, int numRouters,
                            int numEdges, int numInternalEdges);
	void onTableTopoBySizeDoubleClicked(int row, int);
    void onTopoBySizeTxtIncludeFilterChanged(const QString &val);
    void onTopoBySizeTxtExcludeFilterChanged(const QString &val);
    void onTopoBySizeDestroyed(QObject *obj);
	void closeTopoBySize();
    void onTopoBySizeFilter();
    void doComputeTopoSizes(QStringList topologies);

	void on_checkTcpReceiveWin_toggled(bool checked);

	void on_btnReadjustQueuesNow_clicked();

	void on_checkTcpReceiveWinFixed_toggled(bool checked);

	void on_checkTcpReceiveWinAuto_toggled(bool checked);

	void on_btnOpenCapturePlots_clicked();

	void updateWindowTitle();

	void on_btnEnqueue_clicked(bool automatic = false);
	void updateQueueTable();

	void on_btnQueueClear_clicked();

	void on_btnQueueStartAll_clicked();

	void on_btnQueueSave_clicked();

	void on_btnDeployStop_clicked();

	void on_btnQueueStop_clicked();

	void on_btnEnqueueAndStart_clicked();

	void on_btnResultsRename_clicked();

	void on_btnSelectNewSimulation_clicked();

	void on_btnBenchmarkGenerateGraph_clicked();

	void on_btnQueueRemove_clicked();

    void on_txtResultsInterestingLinks_returnPressed();

    void on_cmbExpType_currentIndexChanged(const QString &arg1);

signals:
	void logInformation(QPlainTextEdit *log, QString message);
	void logError(QPlainTextEdit *log, QString message);
	void logSuccess(QPlainTextEdit *log, QString message);
	void logText(QPlainTextEdit *log, QString message);
	void logOutput(QPlainTextEdit *log, QString message);
	void logClear(QPlainTextEdit *log);
	void tabChanged(QWidget *page, QColor color = TAB_COLOR_CHANGED);
	void reloadScene();
	void saveGraph();
	void saveImage(QString fileName);
	void importFinished(QString fileName);
	void layoutFinished();
	void routingFinished();
	void routingChanged();
	void usedChanged();
	void reloadTopologyList(QString graphName);
	void reloadSimulationList(QString testId);
	void delayValidationFinished(QList<QOPlot> *plots);
	void congestionValidationFinished(QList<QOPlot> *plots);
	void queuingLossValidationFinished(QList<QOPlot> *plots);
	void queuingDelayValidationFinished(QList<QOPlot> *plots);
	void btnGenericFinished(QString testId);
	void experimentQueueFinished();
    void topoSizeComputed(QString name,
                          int numNodes, int numRouters,
                          int numEdges, int numInternalEdges);
};

#endif // MAINWINDOW_H
