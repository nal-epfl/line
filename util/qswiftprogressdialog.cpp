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

#include "qswiftprogressdialog.h"
#include "ui_qswiftprogressdialog.h"

#include "chronometer.h"

QSwiftProgressDialog::QSwiftProgressDialog(QString title, QString text,
										   int min, int max, QWidget *parent, bool modal) :
	QDialog(parent),
	ui(new Ui::QSwiftProgressDialog)
{
	ui->setupUi(this);

	setWindowTitle(title);
	ui->label->setText(text);
	ui->progressBar->setRange(min, max);
	ui->progressBar->setValue(0);

	startTime = Chronometer::currentTimeMs();
	updateProgressBarText();

	canceled = false;
	setModal(modal);

	show();
}

QSwiftProgressDialog::~QSwiftProgressDialog()
{
	delete ui;
}

void QSwiftProgressDialog::setRange(int min, int max)
{
	ui->progressBar->setRange(min, max);
	updateProgressBarText();
}

QPair<int, int> QSwiftProgressDialog::range()
{
	return QPair<int, int>(ui->progressBar->minimum(), ui->progressBar->maximum());
}

bool QSwiftProgressDialog::wasCanceled()
{
	return canceled;
}

void QSwiftProgressDialog::setText(QString text)
{
	ui->label->setText(text);
}

QString QSwiftProgressDialog::text()
{
	return ui->label->text();
}

void QSwiftProgressDialog::setValue(int value)
{
	ui->progressBar->setValue(value);
	updateProgressBarText();
	repaint();
	while (QApplication::hasPendingEvents())
		QApplication::processEvents();
}

void QSwiftProgressDialog::on_buttonBox_clicked(QAbstractButton *)
{
	canceled = true;
}

void QSwiftProgressDialog::updateProgressBarText()
{
	if (ui->progressBar->value() - ui->progressBar->minimum() > 0) {
		quint64 elapsed = Chronometer::currentTimeMs() - startTime;
		qreal msPerStep = ((qreal)elapsed)/(ui->progressBar->value() - ui->progressBar->minimum());
		quint64 eta = qRound64(msPerStep * (ui->progressBar->maximum() - ui->progressBar->value()));
		ui->progressBar->setFormat(QString("%p% (ETA: %1...)").arg(Chronometer::durationToString(eta, "s")));
	} else {
		ui->progressBar->setFormat(QString("%p% (ETA: calculating...)"));
	}
}
