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

#ifndef QGRAPHICSTOOLTIP_H
#define QGRAPHICSTOOLTIP_H

#include <QGraphicsTextItem>

class QGraphicsTooltip : public QGraphicsTextItem
{
    Q_OBJECT
public:
	explicit QGraphicsTooltip(QGraphicsItem *parent = 0);
	explicit QGraphicsTooltip(const QString &text, QGraphicsItem *parent = 0);

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

signals:

public slots:

};

#endif // QGRAPHICSTOOLTIP_H
