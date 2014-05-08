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

#include <QtConcurrentRun>
#include <QtXml>
#include <QX11Info>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "flowlayout.h"

#include "../line-gui/intervalmeasurements.h"
#include "chronometer.h"
#include "../remote_config.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef QPair<qint32, qint32> QInt32Pair;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	blocking = false;
	ignoreDirChanges = false;
	lastDirChangeNs = 0;

	delete ui->widgetEditorDummy;
	ui->widgetEditorDummy = 0;
	editor = new NetGraphEditor(ui->widgetEditorParent);
	ui->widgetEditorParent->layout()->addWidget(editor);

	maxLogLines = 10000;

	{
		QRegExp rx("[1-9][0-9]*(s|ms|us|ns)");
		QValidator *validator = new QRegExpValidator(rx, this);
		ui->txtGenericTimeout->setValidator(validator);
		ui->txtIntervalSize->setValidator(validator);
	}

	FlowLayout::applyLayout(ui->groupBoxQuickOpen);

	currentTopology = -1;
	currentSimulation = -1;

	{
		QDir::setCurrent("..");
		rootPath = QDir::currentPath();
		QDir d(".");
        if (!d.exists("line-topologies")) {
            d.mkdir("line-topologies");
            QDir::setCurrent("line-topologies");
            QProcess::execute("bash",
                              QStringList() <<
                              "-c" <<
                              "ls -1 | grep -e '.*.graph$' || cp -r ../line-topologies-pristine/. .");
        } else {
            QDir::setCurrent("line-topologies");
        }
	}
	connect(&topoDirWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(topoDirChanged(QString)));
	topoDirWatcher.addPath(".");
	topoDirChanged(".");

	connect(this, SIGNAL(logError(QPlainTextEdit*,QString)), SLOT(doLogError(QPlainTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logInformation(QPlainTextEdit*,QString)), SLOT(doLogInformation(QPlainTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logText(QPlainTextEdit*,QString)), SLOT(doLogText(QPlainTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logOutput(QPlainTextEdit*,QString)), SLOT(doLogOutput(QPlainTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logSuccess(QPlainTextEdit*,QString)), SLOT(doLogSuccess(QPlainTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logClear(QPlainTextEdit*)), SLOT(doLogClear(QPlainTextEdit*)), Qt::QueuedConnection);
	connect(this, SIGNAL(tabChanged(QWidget *,QColor)), SLOT(doTabChanged(QWidget *,QColor)), Qt::QueuedConnection);
	connect(this, SIGNAL(reloadSimulationList(QString)), SLOT(doReloadSimulationList(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(reloadTopologyList(QString)), SLOT(doReloadTopologyList(QString)), Qt::QueuedConnection);
	//connect(this, SIGNAL(delayValidationFinished(QList<QOPlot>*)), SLOT(doDelayValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	//connect(this, SIGNAL(congestionValidationFinished(QList<QOPlot>*)), SLOT(doCongestionValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	//connect(this, SIGNAL(queuingLossValidationFinished(QList<QOPlot>*)), SLOT(doQueuingLossValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	//connect(this, SIGNAL(queuingDelayValidationFinished(QList<QOPlot>*)), SLOT(doQueuingDelayValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	connect(&voidWatcher, SIGNAL(finished()), SLOT(blockingOperationFinished()));
	connect(this, SIGNAL(reloadScene()), editor, SLOT(reloadScene()), Qt::QueuedConnection);
	connect(this, SIGNAL(saveGraph()), editor, SLOT(save()), Qt::QueuedConnection);
	connect(&briteImporter, SIGNAL(logInfo(QString)), SLOT(doLogBriteInfo(QString)), Qt::QueuedConnection);
	connect(&briteImporter, SIGNAL(logError(QString)), SLOT(doLogBriteError(QString)), Qt::QueuedConnection);
	connect(&graphMLImporter, SIGNAL(logInfo(QString)), SLOT(doLogBriteInfo(QString)), Qt::QueuedConnection);
	connect(&graphMLImporter, SIGNAL(logError(QString)), SLOT(doLogBriteError(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(routingChanged()), editor, SLOT(reloadScene()), Qt::QueuedConnection);
	connect(this, SIGNAL(usedChanged()), editor, SLOT(reloadScene()), Qt::QueuedConnection);
	connect(editor, SIGNAL(renamed(QString)), this, SLOT(onRename(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(btnGenericFinished(QString)), this, SLOT(onBtnGenericFinished(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(topoSizeComputed(QString,int,int,int,int)), this, SLOT(onTopoSizeComputed(QString,int,int,int,int)), Qt::QueuedConnection);

	//	httpServer.start("python",
	//					 QStringList() << "-m" << "SimpleHTTPServer" << "8000");

	// Check for line-runner
	if (!lineRunner(QStringList(), ui->txtBatch)) {
		QMessageBox::critical(this,
							  "line-runner not detected",
                              "Error: the executable of line-runner was not found. Make sure it is compiled in the correct location.",
							  QMessageBox::Ok);
	}

	on_checkGenericTimeout_toggled(ui->checkGenericTimeout->isChecked());
    on_cmbExpType_currentIndexChanged(ui->cmbExpType->currentText());

	initPlots();

	ui->tabWidget->setCurrentWidget(ui->tabTopology);
	emit reloadTopologyList(QString());
	updateWindowTitle();
	loadExperimentQueue();

	showMaximized();

#ifdef HAVE_CUSTOM_CONTROLS
	customControls = new CustomControls();
	customControls->mainWindow = this;
	customControls->show();
#endif
}

MainWindow::~MainWindow()
{
	//	httpServer.terminate();
	//	if (!httpServer.waitForFinished(5000)) {
	//		httpServer.kill();
	//		httpServer.waitForFinished(5000);
	//	}
	delete ui;
}

QString REMOTE_HOST_ROUTER_resolved;
QString REMOTE_HOST_HOSTS_resolved;
void MainWindow::updateWindowTitle()
{
	if (REMOTE_HOST_ROUTER_resolved.isEmpty()) {
		REMOTE_HOST_ROUTER_resolved = resolveDNSReverse(REMOTE_HOST_ROUTER);
		if (REMOTE_HOST_ROUTER_resolved.isEmpty()) {
			REMOTE_HOST_ROUTER_resolved = REMOTE_HOST_ROUTER;
		}
	}
	if (REMOTE_HOST_HOSTS_resolved.isEmpty()) {
		REMOTE_HOST_HOSTS_resolved = resolveDNSReverse(REMOTE_HOST_HOSTS);
		if (REMOTE_HOST_HOSTS_resolved.isEmpty()) {
			REMOTE_HOST_HOSTS_resolved = REMOTE_HOST_HOSTS;
		}
	}
	setWindowTitle(QString("line-gui - %1 and %2 - %3 - %4").
				   arg(REMOTE_HOST_ROUTER_resolved).
				   arg(REMOTE_HOST_HOSTS_resolved).
				   arg(ui->cmbTopologies->currentText()).
				   arg(ui->cmbSimulation->currentText()));
}

void MainWindow::initPlots()
{
#define DEBUG_PLOTS 0
#if DEBUG_PLOTS
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);

	accordion->addLabel("Emulator");
	QOPlotWidget *plot = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
	plot->plot.title = "Zzzz";

	QOPlotStemData *stem = new QOPlotStemData();
	for (int i = 0; i < 10; i++) {
		stem->x.append(i);
		stem->y.append(i);
	}
	stem->pen = QPen(Qt::blue);
	stem->legendLabel = "Events (0 = forward, 1 = drop)";
	plot->plot.addData(stem);
	plot->plot.drag_y_enabled = false;
	plot->plot.zoom_y_enabled = false;
	plot->fixAxes(0, 1000, 0, 2);
	plot->drawPlot();
	accordion->addWidget("Zzzz", plot);

	plot = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
	plot->plot.title = "Zzzz";
	QOPlotCurveData *curve = new QOPlotCurveData();
	for (int i = 0; i < 10; i++) {
		curve->x.append(i);
		curve->y.append(sqrt(i));
	}
	curve->pen = QPen(Qt::blue);
	curve->legendLabel = "Events (0 = forward, 1 = drop)";
	plot->plot.addData(curve);
	plot->fixAxes(0, 1000, 0, 2);
	plot->drawPlot();
	accordion->addWidget("Zzzz", plot);

	plot = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
	plot->plot.title = "Zzzz";
	accordion->addWidget("Zzzz", plot);
	QOPlotBoxPlotData *boxPlots = new QOPlotBoxPlotData();
	boxPlots->items.append(QOPlot::makeBoxWhiskers(QList<qreal>() << 0.1 << 0.11 << 0.15 << 0.16 << 0.19,
												   "First",
												   Qt::blue,
												   false,
												   false));
	boxPlots->items.append(QOPlot::makeBoxWhiskers(QList<qreal>() << 0.2 << 0.21 << 0.25 << 0.28 << 0.29 << 0.72,
												   "Second",
												   Qt::blue,
												   false,
												   false));
	plot->plot.addData(boxPlots);
	plot->plot.xCustomTicks[1] = boxPlots->items[0].legendLabel;
	plot->plot.xCustomTicks[2] = boxPlots->items[1].legendLabel;
	plot->plot.includeOriginX = false;
	plot->autoAdjustAxes();
	plot->drawPlot();

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
#endif
}

void MainWindow::on_actionStop_triggered()
{
	mustStop = true;
}

QString MainWindow::getGraphName(NetGraph *netGraph)
{
	if (netGraph->fileName.isEmpty())
		return QString("untitled");
	QString graphName = netGraph->fileName.split('/', QString::SkipEmptyParts).last();
	graphName = graphName.replace(".graph", "");
	return graphName;
}

QString MainWindow::getCoreHostname()
{
	return REMOTE_HOST_ROUTER;
}

QString MainWindow::getCorePort()
{
	return REMOTE_PORT_ROUTER;
}

QString MainWindow::getClientHostname()
{
	return REMOTE_HOST_HOSTS;
}

QString MainWindow::getClientPort()
{
	return REMOTE_PORT_HOSTS;
}

MainWindow *mainWindow = 0;
QPlainTextEdit *logRedirect = 0;
void qDebugMessageHandler(QtMsgType type, const char *msg)
{
	if (mainWindow && logRedirect) {
		if (type == QtDebugMsg) {
			mainWindow->safeLogText(logRedirect, QString(msg));
		} else {
			mainWindow->safeLogError(logRedirect, QString(msg));
		}
	} else {
		fprintf(stderr, "%s", msg);
	}
}

void MainWindow::startRedirectingQDebug(QPlainTextEdit *log)
{
	mainWindow = this;
	logRedirect = log;
	if (logRedirect) {
		qInstallMsgHandler(qDebugMessageHandler);
	} else {
		qInstallMsgHandler(0);
	}
}

void MainWindow::stopRedirectingQDebug()
{
	qInstallMsgHandler(0);
}

void MainWindow::doLogInformation(QPlainTextEdit *log, QString message)
{
#if 1
	QTextCharFormat format = log->currentCharFormat();
	format.setForeground(Qt::darkCyan);
	format.setFontFamily("Liberation Sans");
	format.setFontStyleHint(QFont::SansSerif);
	format.setFontPointSize(10);
	log->setCurrentCharFormat(format);
	log->appendPlainText(QString("[%1] %2").arg(QTime::currentTime().toString("hh:mm:ss"), message));
	logOutputPositions.remove(log);
	doLogTabChanged(log);
#else
	Q_UNUSED(log);Q_UNUSED(message);
#endif
}

void MainWindow::doLogError(QPlainTextEdit *log, QString message)
{
#if 1
	QTextCharFormat format = log->currentCharFormat();
	format.setForeground(Qt::red);
	format.setFontFamily("Liberation Sans");
	format.setFontStyleHint(QFont::SansSerif);
	format.setFontPointSize(10);
	log->setCurrentCharFormat(format);
	log->appendPlainText(message);
	logOutputPositions.remove(log);
	doLogTabChanged(log, TAB_COLOR_ERROR);
#else
	Q_UNUSED(log);Q_UNUSED(message);
#endif
}

void MainWindow::doLogSuccess(QPlainTextEdit *log, QString message)
{
#if 1
	QTextCharFormat format = log->currentCharFormat();
	format.setForeground(Qt::darkGreen);
	format.setFontFamily("Liberation Sans");
	format.setFontStyleHint(QFont::SansSerif);
	format.setFontPointSize(10);
	log->setCurrentCharFormat(format);
	log->appendPlainText(message);
	logOutputPositions.remove(log);
	doLogTabChanged(log);
#else
	Q_UNUSED(log);Q_UNUSED(message);
#endif
}

void MainWindow::doLogText(QPlainTextEdit *log, QString message)
{
#if 1
	QTextCharFormat format = log->currentCharFormat();
	format.setForeground(Qt::black);
	format.setFontFamily("Liberation Sans");
	format.setFontStyleHint(QFont::SansSerif);
	format.setFontPointSize(10);
	log->setCurrentCharFormat(format);
	log->appendPlainText(message);
	logOutputPositions.remove(log);
	doLogTabChanged(log);
#else
	Q_UNUSED(log);Q_UNUSED(message);
#endif
}

void overwriteText(QPlainTextEdit *log, QString newText, QTextCharFormat format) {
	for (int offset = 0; offset < newText.length() && !log->textCursor().atEnd(); offset++) {
		log->moveCursor(QTextCursor::Right, QTextCursor::KeepAnchor);
	}
	log->textCursor().removeSelectedText();
	log->setCurrentCharFormat(format);
	log->insertPlainText(newText);
	log->moveCursor(QTextCursor::NoMove);
}

void MainWindow::doLogOutput(QPlainTextEdit *log, QString message)
{
#if 1
	QTextCharFormat oldFormat = log->currentCharFormat();
	QTextCharFormat newFormat = oldFormat;
	newFormat.setForeground(Qt::darkBlue);
	newFormat.setFontFamily("Liberation Mono");
    newFormat.setFontStyleHint(QFont::Monospace);
	newFormat.setFontPointSize(10);
	log->setCurrentCharFormat(newFormat);
	QTextCursor storedCursorPos = log->textCursor();
	if (logOutputPositions.contains(log)) {
		log->setTextCursor(logOutputPositions[log]);
	} else {
		log->moveCursor(QTextCursor::End);
		log->insertPlainText("\n");
	}
	{
		QStringList lines = message.split('\n');
		if (lines.length() > log->maximumBlockCount()) {
			lines = lines.mid(qMax(0, lines.length() - log->maximumBlockCount()));
			message = lines.join("\n");
		}
		while (!message.isEmpty()) {
			bool restart = false;
			for (int i = 0; i < message.length(); i++) {
				if (message.at(i) == '\n') {
					QString line = message.mid(0, i + 1);
					message = message.mid(i + 1);
					overwriteText(log, line, newFormat);
					log->moveCursor(QTextCursor::End);
					restart = true;
					break;
				} else if (message.at(i) == '\r') {
					QString line = message.mid(0, i);
					message = message.mid(i + 1);
					overwriteText(log, line, newFormat);
					log->moveCursor(QTextCursor::StartOfLine);
					restart = true;
					break;
				} else if (message.mid(i, 4) == "\033[2K") {
					// VT100 code for: clear whole line (cursor position unchanged)
					message = message.mid(i + 4);
					{
						QTextCursor tmpCursorPos = log->textCursor();
						log->moveCursor(QTextCursor::EndOfLine);
						log->moveCursor(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
						int length = log->textCursor().selectionEnd() - log->textCursor().selectionStart();
						log->textCursor().removeSelectedText();
						for (int offset = 0; offset < length; offset++) {
							overwriteText(log, " ", newFormat);
						}
						log->setTextCursor(tmpCursorPos);
					}
					restart = true;
					break;
				} else if (message.at(i) == '\033') {
					qDebug() << QString("Unhandled escape code: %1").arg(message.mid(i+1, 5));
				}
			}
			if (!restart) {
				break;
			}
		}
		if (!message.isEmpty()) {
			// This happens when there is no trailing newline
			overwriteText(log, message, newFormat);
		}
	}
	logOutputPositions[log] = log->textCursor();
	log->setTextCursor(storedCursorPos);
	log->setCurrentCharFormat(oldFormat);
	doLogTabChanged(log);
#else
	Q_UNUSED(log);Q_UNUSED(message);
#endif
}

void MainWindow::doLogClear(QPlainTextEdit *log)
{
	log->clear();
	doLogTabChanged(log, TAB_COLOR_DEFAULT);
}

void MainWindow::safeLogInformation(QPlainTextEdit *log, QString message)
{
	emit logInformation(log, message);
}

void MainWindow::safeLogError(QPlainTextEdit *log, QString message)
{
	emit logError(log, message);
}

void MainWindow::safeLogSuccess(QPlainTextEdit *log, QString message)
{
	emit logSuccess(log, message);
}

void MainWindow::safeLogText(QPlainTextEdit *log, QString message)
{
	emit logText(log, message);
}

void MainWindow::safeLogOutput(QPlainTextEdit *log, QString message)
{
	emit logOutput(log, message);
}

void MainWindow::safeLogClear(QPlainTextEdit *log)
{
	emit logClear(log);
}

void MainWindow::closeEvent(QCloseEvent *)
{
	closeTopoBySize();
}

void MainWindow::doLogBriteInfo(QString s)
{
	safeLogInformation(ui->txtBrite, s);
}

void MainWindow::doLogBriteError(QString s)
{
	safeLogError(ui->txtBrite, s);
}

void MainWindow::doLogTabChanged(QPlainTextEdit *log, QColor color)
{
	if (log == ui->txtBrite) {
		emit tabChanged(ui->tabBRITE, color);
	} else if (log == ui->txtBenchmark) {
        emit tabChanged(ui->tabBenchmark, color);
    } else if (log == ui->txtDeploy) {
		emit tabChanged(ui->tabDeploy, color);
    } else if (log == ui->txtBatch) {
		emit tabChanged(ui->tabRun, color);
	} else if (log == ui->txtCapture) {
		emit tabChanged(ui->tabCapture, color);
	} else if (log == ui->txtExperimentQueue) {
		emit tabChanged(ui->tabQueue, color);
	}
}

void MainWindow::blockingOperationStarting()
{
	// lock gui
	qDebug() << __FUNCTION__;
	ui->btnDeploy->setEnabled(false);
	ui->groupBoxQuickOpen->setEnabled(false);
	ui->btnBriteImport->setEnabled(false);
	ui->btnBRITEBatchImport->setEnabled(false);
    editor->setReadOnly(true);
	mustStop = false;
	blocking = true;
}

void MainWindow::blockingOperationFinished(bool blink)
{
	// unlock gui
	qDebug() << __FUNCTION__;
	ui->btnDeploy->setEnabled(true);
    ui->groupBoxQuickOpen->setEnabled(true);
	ui->btnBriteImport->setEnabled(true);
	ui->btnBRITEBatchImport->setEnabled(true);
    editor->setReadOnly(false);
	mustStop = false;
	blocking = false;

    // flash taskbar button
    if (blink) {
        blinkTaskbar();
    }
}

void MainWindow::blinkTaskbar()
{
    Display *display = QX11Info::display();
    Window rootWin = QX11Info::appRootWindow();

    static Atom demandsAttention = XInternAtom(display, "_NET_WM_STATE_DEMANDS_ATTENTION", true);
    static Atom wmState = XInternAtom(display, "_NET_WM_STATE", true);

    XEvent e;
    e.xclient.type = ClientMessage;
    e.xclient.message_type = wmState;
    e.xclient.display = display;
    e.xclient.window = this->winId();
    e.xclient.format = 32;
    e.xclient.data.l[0] = 1;
    e.xclient.data.l[1] = demandsAttention;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;

    XSendEvent(display, rootWin, False, (SubstructureRedirectMask | SubstructureNotifyMask), &e);
}

void MainWindow::updateTimeBox(QLineEdit *txt, bool &accepted, quint64 &value)
{
	accepted = false;
	const QValidator *validator = txt->validator();

	if (validator) {
		QString text = txt->text();
		int pos = txt->cursorPosition();
		QValidator::State state = validator->validate(text, pos);
		if (state == QValidator::Acceptable) {
			txt->setStyleSheet("color:black");
			value = timeFromString(text);
			accepted = true;
		} else if (state == QValidator::Intermediate) {
			txt->setStyleSheet("color:blue");
		} else if (state == QValidator::Invalid) {
			txt->setStyleSheet("color:red");
		}
	}
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
	ui->tabWidget->tabBar()->setTabTextColor(index, TAB_COLOR_DEFAULT);
}

void MainWindow::doTabChanged(QWidget *page, QColor color)
{
	if (ui->tabWidget->currentWidget() != page) {
		if (color == TAB_COLOR_CHANGED &&
			ui->tabWidget->tabBar()->tabTextColor(ui->tabWidget->indexOf(page)) == TAB_COLOR_ERROR) {
			return;
		}
		ui->tabWidget->tabBar()->setTabTextColor(ui->tabWidget->indexOf(page), color);
	}
}

void MainWindow::topoDirChanged(QString)
{
	if (ignoreDirChanges)
		return;
	if (getCurrentTimeNanosec() < lastDirChangeNs + 1000000000ULL)
		return;
	lastDirChangeNs = getCurrentTimeNanosec();
	QString currentTopologyEntry;
	if (currentTopology >= 0) {
		currentTopologyEntry = ui->cmbTopologies->itemText(currentTopology);
	}

	QStringList allTopos;
	allTopos << "(untitled)";

	{
		QDir dir;
		dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
		dir.setSorting(QDir::Name | QDir::IgnoreCase);
		QFileInfoList list = dir.entryInfoList();
		for (int i = 0; i < list.size(); ++i) {
			QFileInfo fileInfo = list.at(i);

			if (!fileInfo.fileName().endsWith(".graph"))
				continue;

			allTopos << fileInfo.fileName().replace(QRegExp("\\.graph$"), "");
		}
	}

	int i, j;
	for (i = 0, j = 0; i < ui->cmbTopologies->count() && j < allTopos.count(); ) {
		QString cmbItem = ui->cmbTopologies->itemText(i);
		QString allItem = allTopos[j];
		if (cmbItem < allItem) {
			// delete
			ui->cmbTopologies->removeItem(i);
		} else if (cmbItem == allItem) {
			i++;
			j++;
		} else {
			// insert
			ui->cmbTopologies->insertItem(i, QIcon(), allItem);
			i++;
			j++;
		}
	}

	while (i < ui->cmbTopologies->count()) {
		ui->cmbTopologies->removeItem(i);
	}

	while (j < allTopos.count()) {
		ui->cmbTopologies->addItem(allTopos[j]);
		j++;
	}

	for (i = 0; i < ui->cmbTopologies->count(); i++) {
		if (ui->cmbTopologies->itemText(i) == currentTopologyEntry) {
			ui->cmbTopologies->setCurrentIndex(i);
			break;
		}
	}
	lastDirChangeNs = getCurrentTimeNanosec();
}

void MainWindow::onRename(QString newName)
{
	topoDirChanged("");
	QFileInfo pathInfo(newName);
	QString graphName = pathInfo.fileName();
	graphName = graphName.replace(QRegExp("\\.graph$", Qt::CaseInsensitive), "");
	for (int i = 0; i < ui->cmbTopologies->count(); i++) {
		if (ui->cmbTopologies->itemText(i) == graphName) {
			ui->cmbTopologies->setCurrentIndex(i);
			break;
		}
	}
}

const int topoBySizeColTopology = 0;
const int topoBySizeColNodes = 1;
const int topoBySizeColRouters = 2;
const int topoBySizeColEdges = 3;
const int topoBySizeColInternalEdges = 4;
const int topoBySizeColSize = 5;
QWidget *topoBySizeWindow = NULL;
QTableWidget *topoBySizeTable = NULL;
QString topoBySizeIncludeFilter;
QString topoBySizeExcludeFilter;

void MainWindow::onTopoBySizeDestroyed(QObject *)
{
	topoBySizeWindow = NULL;
	topoBySizeTable = NULL;
}

void MainWindow::closeTopoBySize()
{
	if (topoBySizeWindow) {
		delete topoBySizeWindow;
		topoBySizeWindow = NULL;
	}
}

void MainWindow::onTableTopoBySizeDoubleClicked(int row, int)
{
	QTableWidget *table = topoBySizeTable;
	if (!table)
		return;
	QString graphName = table->item(row, topoBySizeColTopology)->data(Qt::DisplayRole).toString();
	for (int i = 0; i < ui->cmbTopologies->count(); i++) {
		if (ui->cmbTopologies->itemText(i) == graphName) {
			ui->cmbTopologies->setCurrentIndex(i);
			break;
		}
	}
}

void MainWindow::onTopoBySizeTxtIncludeFilterChanged(const QString &val)
{
	topoBySizeIncludeFilter = val;
	onTopoBySizeFilter();
}

void MainWindow::onTopoBySizeTxtExcludeFilterChanged(const QString &val)
{
	topoBySizeExcludeFilter = val;
	onTopoBySizeFilter();
}

void MainWindow::onTopoBySizeFilter()
{
	QTableWidget *table = topoBySizeTable;
	if (!table)
		return;
	for (int i = 0; i < table->rowCount(); i++) {
		QString graphName = table->item(i, topoBySizeColTopology)->text();
		bool included = true;
		if (!topoBySizeIncludeFilter.isEmpty()) {
			included = included && graphName.contains(topoBySizeIncludeFilter, Qt::CaseInsensitive);
		}
		if (!topoBySizeExcludeFilter.isEmpty()) {
			included = included && !graphName.contains(topoBySizeExcludeFilter, Qt::CaseInsensitive);
		}
		table->setRowHidden(i, !included);
	}
}

void MainWindow::doComputeTopoSizes(QStringList topologies)
{
	foreach (QString graphName, topologies) {
		NetGraph g;
		g.setFileName(graphName + ".graph");
		if (!g.loadFromFile())
			continue;

		int numNodes = g.nodes.count();
		int numRouters = g.nodes.count() - g.getHostNodes().count();
		int numEdges = g.edges.count();
		int numInternalEdges = 0;
		foreach (NetGraphEdge e, g.edges) {
			if (g.nodes[e.source].nodeType != NETGRAPH_NODE_HOST &&
				g.nodes[e.dest].nodeType != NETGRAPH_NODE_HOST) {
				numInternalEdges++;
			}
		}
		emit topoSizeComputed(graphName, numNodes, numRouters, numEdges, numInternalEdges);
	}
}

bool topoSizeLessThan(const QPair<QString, qint64> &a, const QPair<QString, qint64> &b)
{
	return a.second < b.second;
}

void MainWindow::on_btnListTopoBySize_clicked()
{
	if (topoBySizeWindow) {
		topoBySizeWindow->raise();
		return;
	}
	QWidget *window = new QWidget();
	topoBySizeWindow = window;

	QWidget *filters = new QWidget(window);
	QLabel *labelInclude = new QLabel("Include:", filters);
	QLineEdit *txtIncludeFilter = new QLineEdit(filters);
	connect(txtIncludeFilter, SIGNAL(textChanged(QString)), this, SLOT(onTopoBySizeTxtIncludeFilterChanged(QString)), Qt::DirectConnection);
	QLabel *labelExclude = new QLabel("Exclude:", filters);
	QLineEdit *txtExcludeFilter = new QLineEdit(filters);
	connect(txtExcludeFilter, SIGNAL(textChanged(QString)), this, SLOT(onTopoBySizeTxtExcludeFilterChanged(QString)), Qt::DirectConnection);
	QHBoxLayout *hbox = new QHBoxLayout(filters);
	hbox->addWidget(labelInclude);
	hbox->addWidget(txtIncludeFilter);
	hbox->addWidget(labelExclude);
	hbox->addWidget(txtExcludeFilter);
	hbox->setContentsMargins(0, 0, 0, 0);
	filters->setLayout(hbox);
	topoBySizeIncludeFilter.clear();
	topoBySizeExcludeFilter.clear();

	QTableWidget *table = new QTableWidget(window);
	topoBySizeTable = table;
	table->setColumnCount(6);
    table->setHorizontalHeaderLabels(QStringList() << "Topology" << "Nodes" << "Routers" << "Links" << "Internal links" << "Size");
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	QList<QPair<QString, qint64> > topologies;
	for (int i = 0; i < ui->cmbTopologies->count(); i++) {
		QString graphName = ui->cmbTopologies->itemText(i);
		if (graphName == "(untitled)")
			continue;
		QFileInfo fileInfo(graphName + ".graph");
		if (fileInfo.size() == 0)
			continue;
		topologies << QPair<QString, qint64>(graphName, fileInfo.size());

		table->setRowCount(table->rowCount() + 1);
		QTableWidgetItem *cell;
		cell = new QTableWidgetItem(graphName);
		table->setItem(table->rowCount() - 1, topoBySizeColTopology, cell);

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, -1);
		cell->setData(Qt::DisplayRole, " ");
		table->setItem(table->rowCount() - 1, topoBySizeColNodes, cell);

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, -1);
		cell->setData(Qt::DisplayRole, " ");
		table->setItem(table->rowCount() - 1, topoBySizeColRouters, cell);

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, -1);
		cell->setData(Qt::DisplayRole, " ");
		table->setItem(table->rowCount() - 1, topoBySizeColEdges, cell);

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, -1);
		cell->setData(Qt::DisplayRole, " ");
		table->setItem(table->rowCount() - 1, topoBySizeColInternalEdges, cell);

		cell = new QTableWidgetItem();
		cell->setData(Qt::EditRole, fileInfo.size());
		table->setItem(table->rowCount() - 1, topoBySizeColSize, cell);
	}
	connect(table, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onTableTopoBySizeDoubleClicked(int,int)), Qt::DirectConnection);
	table->setSortingEnabled(true);
	table->sortByColumn(topoBySizeColTopology, Qt::AscendingOrder);
	table->setColumnWidth(topoBySizeColTopology, 300);
	table->setColumnHidden(topoBySizeColSize, true);

	window->setAttribute(Qt::WA_DeleteOnClose);
	window->resize(800, QApplication::desktop()->height() - 50);
	window->show();
	window->raise();
	window->move(QApplication::desktop()->width() - 800, 0);
	window->setWindowTitle("Topologies by size");

	QVBoxLayout *vbox = new QVBoxLayout(window);
	vbox->addWidget(filters);
	vbox->addWidget(table);
	window->setLayout(vbox);
	connect(window, SIGNAL(destroyed(QObject*)), this, SLOT(onTopoBySizeDestroyed(QObject*)), Qt::DirectConnection);
	qSort(topologies.begin(), topologies.end(), topoSizeLessThan);
	QStringList topologyNames;
	for (int i = 0; i < topologies.count(); i++) {
		topologyNames << topologies[i].first;
	}
	QtConcurrent::run(this, &MainWindow::doComputeTopoSizes, topologyNames);
}

void MainWindow::onTopoSizeComputed(QString name, int numNodes, int numRouters, int numEdges, int numInternalEdges)
{
	QTableWidget *table = topoBySizeTable;
	if (!table)
		return;
	for (int i = 0; i < table->rowCount(); i++) {
		QString graphName = table->item(i, topoBySizeColTopology)->text();
		if (graphName == name) {
			QTableWidgetItem *cell;
			cell = table->item(i, topoBySizeColNodes);
			cell->setData(Qt::EditRole, numNodes);
			cell->setData(Qt::DisplayRole, numNodes);

			cell = table->item(i, topoBySizeColRouters);
			cell->setData(Qt::EditRole, numRouters);
			cell->setData(Qt::DisplayRole, numRouters);

			cell = table->item(i, topoBySizeColEdges);
			cell->setData(Qt::EditRole, numEdges);
			cell->setData(Qt::DisplayRole, numEdges);

			cell = table->item(i, topoBySizeColInternalEdges);
			cell->setData(Qt::EditRole, numInternalEdges);
			cell->setData(Qt::DisplayRole, numInternalEdges);
			break;
		}
	}
}
