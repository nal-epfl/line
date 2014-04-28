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

#include "qaccordion.h"

#include "qdisclosure.h"

QAccordion::QAccordion(QWidget *parent) :
	QWidget(parent)
{
	QVBoxLayout *verticalLayout = new QVBoxLayout(this);
	verticalLayout->setAlignment(Qt::AlignTop);
}

void QAccordion::addLabel(QString text)
{
	QLabel *txt = new QLabel(this);
	txt->setText(text);
	QFont font = txt->font();
	font.setBold(true);
	txt->setFont(font);
	txt->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	layout()->addWidget(txt);
}

void QAccordion::addWidget(QString title, QWidget *widget)
{
	QDisclosure *disc = new QDisclosure(this);
	disc->setTitle(title);
	disc->setWidget(widget);
	widget->setParent(disc);
	connect(disc, SIGNAL(toggled(bool)), SLOT(childToggled(bool)));
	disclosures << disc;
	layout()->addWidget(disc);
}

void QAccordion::expandAll()
{
	foreach (QDisclosure *d, disclosures) {
		d->expand();
	}
}

void QAccordion::childToggled(bool )
{
	/*
	bool childExpanded = false;
	int totalSize = 2 * layout()->margin();
	bool childPolicyExpands = false;
	foreach (QDisclosure *d, disclosures) {
		if (d->isExpanded()) {
			childExpanded = true;
			totalSize += d->sizeHint().height();
			childPolicyExpands |= d->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag;
		}
		totalSize += layout()->spacing();
	}
	if (!disclosures.isEmpty())
		totalSize -= layout()->spacing();
	if (!childExpanded ||
			(totalSize < height() && !childPolicyExpands)) {
		spacer = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
		layout()->addItem(spacer);
	} else if (spacer != NULL) {
		layout()->takeAt(layout()->count() - 1);
		delete spacer;
		spacer = NULL;
	}*/
}
