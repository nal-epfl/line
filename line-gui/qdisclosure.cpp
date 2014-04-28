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

#include "qdisclosure.h"

QDisclosure::QDisclosure(QWidget *parent) :
	QWidget(parent)
{
	mWidget = NULL;

	verticalLayout = new QVBoxLayout(this);
	verticalLayout->setSpacing(2);
	verticalLayout->setContentsMargins(0, 0, 0, 0);

	checkExpand = new QCheckBox(this);
	checkExpand->setText("Test");
	verticalLayout->addWidget(checkExpand);

	checkExpand->setStyleSheet("QCheckBox { border-top-width: 1px; border-top-color: gray; border-top-style: outset; padding-top: 1px; padding-bottom: 1px; } "
							   "QCheckBox::indicator { "
							   "  width: 15px; "
							   "  height: 15px; "
							   "} "
							   "QCheckBox::indicator:unchecked { "
							   "  image: url(:/icons/extra-resources/stylesheet-branch-closed.png); "
							   "} "
							   "QCheckBox::indicator:checked { "
							   "  image: url(:/icons/extra-resources/stylesheet-branch-open.png); "
							   "} ");
	checkExpand->setCheckable(false);

	connect(checkExpand, SIGNAL(toggled(bool)), SLOT(checkToggled(bool)));
}

void QDisclosure::setTitle(QString title)
{
	checkExpand->setText(title);
}

QString QDisclosure::title()
{
	return checkExpand->text();
}

void QDisclosure::setWidget(QWidget *widget)
{
	if (mWidget) {
		verticalLayout->removeWidget(mWidget);
	}
	mWidget = widget;
	if (mWidget) {
		verticalLayout->addWidget(mWidget);
		mWidget->setParent(this);
	}

	checkExpand->setCheckable(mWidget != NULL);
	checkToggled(checkExpand->isChecked());
}

QWidget *QDisclosure::widget()
{
	return mWidget;
}

void QDisclosure::checkToggled(bool checked)
{
	if (mWidget) {
		if (!checked) {
			oldMaxHeight = mWidget->maximumHeight();
			oldMinHeight = mWidget->minimumHeight();
			mWidget->setMinimumHeight(0);
			mWidget->setMaximumHeight(0);
			setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
		} else {
			mWidget->setMaximumHeight(oldMaxHeight);
			mWidget->setMinimumHeight(oldMinHeight);
			setSizePolicy(QSizePolicy::MinimumExpanding, mWidget->sizePolicy().verticalPolicy());
		}
	}
	adjustSize();
	updateGeometry();
	emit toggled(checked);
}

void QDisclosure::expand()
{
	checkExpand->setChecked(true);
}

void QDisclosure::collapse()
{
	checkExpand->setChecked(false);
}

bool QDisclosure::isExpanded()
{
	return checkExpand->isChecked();
}
