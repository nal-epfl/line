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

#ifndef QSWIFTPROGRESSDIALOG_H
#define QSWIFTPROGRESSDIALOG_H

#include <QtGui>

namespace Ui {
class QSwiftProgressDialog;
}

class QSwiftProgressDialog : public QDialog
{
	Q_OBJECT
	
public:
	explicit QSwiftProgressDialog(QString title, QString text,
								  int min, int max, QWidget *parent = 0, bool modal = true);
	~QSwiftProgressDialog();

	void setValue(int value);
	bool wasCanceled();

	void setText(QString text);
	QString text();
	void setRange(int min, int max);
	QPair<int, int> range();

private slots:
	void on_buttonBox_clicked(QAbstractButton *button);

private:
	Ui::QSwiftProgressDialog *ui;
	bool canceled;
	quint64 startTime;

	void updateProgressBarText();
};

#endif // QSWIFTPROGRESSDIALOG_H
