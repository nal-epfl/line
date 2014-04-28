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

#include "qwraplabel.h"

QWrapLabel::QWrapLabel(QWidget *parent, Qt::WindowFlags f)
	: QLabel(parent, f)
{
}

QWrapLabel::QWrapLabel(const QString &text, QWidget *parent, Qt::WindowFlags f)
	: QLabel(text, parent, f)
{
}

QWrapLabel::~QWrapLabel()
{
}

QSize QWrapLabel::sizeHint() const
{
	return size();
}

QSize QWrapLabel::minimumSizeHint() const
{
	return size();
}

int QWrapLabel::heightForWidth(int w) const
{
	return size(w).height();
}

QSize QWrapLabel::size(int w) const
{
	QTextDocument document;
	document.setTextWidth(w >= 0 ? w : width());
	document.setHtml(text());
	QSize s = document.size().toSize();
	s += QSize(2 * margin(), 2 * margin());
	return s;
}


