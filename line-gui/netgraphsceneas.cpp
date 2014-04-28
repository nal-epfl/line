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

#include "netgraphsceneas.h"
#include "../util/colortable.h"

NetGraphSceneAS::NetGraphSceneAS(int index, int ASNumber, QList<QPointF> hull, QGraphicsItem *parent, QGraphicsScene *scene) :
	QGraphicsObject(parent), index(index), ASNumber(ASNumber), hull(hull)
{
	if (scene && !parent)
	    scene->addItem(this);

//	const int colorCount = 30;
	const double alpha = 0.4;
//	double hue = (ASNumber % colorCount) / (double)colorCount;
//	color = QColor::fromHsvF(hue, 1, 1, alpha);
	color = ColorTable::getDefault24().getColor(ASNumber);
	color.setAlphaF(alpha);

	used = true;
	unusedHidden = false;
}

void NetGraphSceneAS::setUsed(bool used)
{
	this->used = used;
	update();
}

void NetGraphSceneAS::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	update();
}

QRectF NetGraphSceneAS::boundingRect() const
{
	QPolygonF poly;
	foreach (QPointF p, hull)
		poly.append(p);
	return poly.boundingRect();
}

void NetGraphSceneAS::paint(QPainter *painter,
						const QStyleOptionGraphicsItem *option,
						QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (!used && unusedHidden)
		return;

	if (hull.count() < 3)
		return;

	painter->setPen(Qt::gray);
	painter->setBrush(color);

	QPolygonF poly;
	QPointF center;
	foreach (QPointF p, hull) {
		poly.append(p);
		center += p;
	}
	center.setX(center.x() / (hull.count() + 0.001));
	center.setY(center.y() / (hull.count() + 0.001));
	painter->drawPolygon(poly);

	QRectF brect = boundingRect();

	int fontSize = qMax(8, (int)qMin(brect.height()/6, brect.width()/6));
	QFont font = QFont("Arial", fontSize, QFont::Bold);
	QFontMetricsF fm(font);
	QString text = QString("AS %1").arg(ASNumber);
	double textWidth = fm.width(text);
	double textHeight = fm.height();

	painter->setPen(color.darker(300));
	painter->setFont(font);
	painter->drawText(center.x() - textWidth/2, center.y() + textHeight/2, text);
}

void NetGraphSceneAS::setHull(QList<QPointF> hull)
{
	prepareGeometryChange();
	this->hull = hull;
	update();
}
