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

#ifndef NETGRAPHSCENEAS_H
#define NETGRAPHSCENEAS_H

#include <QtCore>
#include <QtGui>

class NetGraphSceneAS : public QGraphicsObject
{
	Q_OBJECT

public:
	explicit NetGraphSceneAS(int index, int ASNumber, QList<QPointF> hull, QGraphicsItem *parent = 0, QGraphicsScene *scene = 0);

	int index;
	int ASNumber;
	QList<QPointF> hull;
	QColor color;
	bool used;
	bool unusedHidden;

	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void setUsed(bool used);
	void setUnusedHidden(bool unusedHidden);
	void setHull(QList<QPointF> hull);
};

#endif // NETGRAPHSCENEAS_H
