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

#include "writelog.h"

#ifdef QT_GUI_LIB

#include "mainwindow.h"

static MainWindow *writeLog_mw = 0;
static QPlainTextEdit *writeLog_txtEdit = 0;

#else

class MainWindow {};
class QPlainTextEdit {};

#endif

void writeLog(QString msg, QString type)
{
	if (type == "e" || type == "error") {
		qWarning() << msg;
	} else {
		qDebug() << msg;
	}
#ifdef QT_GUI_LIB
	if (writeLog_mw && writeLog_txtEdit) {
		if (type == "e" || type == "error") {
			writeLog_mw->safeLogError(writeLog_txtEdit, msg);
		} else if (type == "i" || type == "info" || type == "information") {
			writeLog_mw->safeLogInformation(writeLog_txtEdit, msg);
		} else if (type == "s" || type == "success" || type == "ok") {
			writeLog_mw->safeLogSuccess(writeLog_txtEdit, msg);
		} else if (type == "o" || type == "out" || type == "output") {
			writeLog_mw->safeLogOutput(writeLog_txtEdit, msg);
		} else {
			writeLog_mw->safeLogText(writeLog_txtEdit, msg);
		}
		QApplication::processEvents();
	}
#endif
}

void writeLogInit(MainWindow *mainWindow, QPlainTextEdit *textEdit)
{
#ifdef QT_GUI_LIB
	writeLog_mw = mainWindow;
	writeLog_txtEdit = textEdit;
#else
	Q_UNUSED(mainWindow);
	Q_UNUSED(textEdit);
#endif
}
