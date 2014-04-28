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

#include "netgraphsceneedge.h"

#define PI 3.14159265359
#define RAD_TO_DEG (180.0 / 3.14159265359)
#define DEG_TO_RAD (3.14159265359 / 180.0)

NetGraphSceneEdge::NetGraphSceneEdge(int startIndex, int endIndex, int edgeIndex,
							  QGraphicsItem *parent, QGraphicsScene *scene) :
	QGraphicsObject(parent)
{
	if (scene && !parent)
	    scene->addItem(this);

	this->startIndex = startIndex;
	this->endIndex = endIndex;
	this->edgeIndex = edgeIndex;
	this->setAcceptHoverEvents(true);
	start = NULL;
	end = NULL;
	text = "";
	selected = false;
	hovered = false;
	font = QFont("Arial", 8);
	// how far from the line that connects the centers of the nodes should we draw the arrow
	offset = 5;
	tipw = 6;
	tiph = 8;
	tipyFull = NETGRAPH_NODE_RADIUS;
	textSpacing = 2;
	used = true;
	unusedHidden = false;
	edgesHidden = false;
	flowEdge = false;
	color = QColor(Qt::black);
	width = 2.0;
	flowSpacing = 1.0;

	textItem = new QGraphicsSimpleTextItem(this);
	textItem->setFont(font);
	textItem->setZValue(-1.1);
	lineItem = new QGraphicsLineItem(this);
	lineItem->setZValue(-1.1);
	tipItem = new QGraphicsPolygonItem(this);
	tipItem->setZValue(-1.1);
	updateColor();
	updateVisibility();
}

QRectF NetGraphSceneEdge::boundingRect() const
{
	QRectF bbox;
	foreach (QGraphicsItem *c, childItems()) {
		QRectF rect = c->boundingRect();
		if (isnan(rect.width()) || isnan(rect.height()))
			continue;
		bbox = bbox.united(rect);
	}

	return bbox;
}

void NetGraphSceneEdge::paint(QPainter *,
						const QStyleOptionGraphicsItem *,
						QWidget *)
{
}

void NetGraphSceneEdge::setStartPoint(QPointF newStartPoint)
{
	startPoint = newStartPoint;
	updateTipOffset();
}

void NetGraphSceneEdge::setEndPoint(QPointF newEndPoint)
{
	endPoint = newEndPoint;
	updateTipOffset();
}

void NetGraphSceneEdge::setStartNode(NetGraphSceneNode *newStart)
{
	start = newStart;
	updateTipOffset();
	updateVisibility();
}

void NetGraphSceneEdge::setEndNode(NetGraphSceneNode *newEnd)
{
	end = newEnd;
	updateTipOffset();
	updateVisibility();
}

void NetGraphSceneEdge::setTipOffset(qreal newTipOffset)
{
	tipyFull = newTipOffset;
	updatePosition();
}

void NetGraphSceneEdge::updatePosition()
{
	double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
					(endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));
	double angle = atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG;

	// line
	lineItem->setLine(0, 0, dist, 0);

	// tip
	{
		float tipy;
		if (!fastMode) {
			tipy = tipyFull;
		} else {
			tipy = sqrt(NETGRAPH_NODE_RADIUS_FAST * NETGRAPH_NODE_RADIUS_FAST - offset * offset);
		}
		QPolygonF tip;
		tip << QPointF(dist - tipy, 0)
				<< QPointF(dist - tipy - tiph, tipw/2)
				<< QPointF(dist - tipy - tiph, -tipw/2);
		tipItem->setPolygon(tip);
	}

	// text
	if (!fastMode) {
		if (angle > -90 && angle < 90) {
			textItem->resetTransform();
			QTransform t;
			t.translate(dist/2.0 - textItem->boundingRect().width()/2.0, 0);
			textItem->setTransform(t);
		} else {
			textItem->resetTransform();
			QTransform t;
			t.translate(dist/2.0 - textItem->boundingRect().width()/2.0, 0);
			t.rotate(180);
			t.translate(-textItem->boundingRect().width(), -textItem->boundingRect().height());
			textItem->setTransform(t);
		}
	} else {
		textItem->setVisible(false);
	}

	QTransform t;
	t.translate(startPoint.x(), startPoint.y());
	t.rotate(angle);
	t.translate(0, offset);
	setTransform(t);
}

void NetGraphSceneEdge::updateColor()
{
	QPen pen;
	pen.setWidth(width);
	pen.setCosmetic(true);
	pen.setColor(color);
	if (selected) {
//		if (hovered) {
//			pen.setColor(Qt::cyan);
//		} else {
			pen.setColor(Qt::cyan);
//		}
	} else {
//		if (hovered) {
//			pen.setColor(Qt::darkGray);
//		} else {
			pen.setColor(color);
//		}
	}
	lineItem->setPen(pen);
	tipItem->setPen(pen);
	tipItem->setBrush(pen.color());
	pen.setWidth(0);
	pen.setCosmetic(false);
	textItem->setPen(pen);
	textItem->setBrush(pen.color());
}

void NetGraphSceneEdge::updateVisibility()
{
	if (edgesHidden) {
		setVisible(false);
	} else {
		if (!used && unusedHidden) {
			setVisible(false);
		} else {
			setVisible(startIndex != endIndex);
		}
	}
}

void NetGraphSceneEdge::updateTipOffset()
{
	if (start && end) {
		double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
						(endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));
		QLineF line(0, 0, dist, 0);

		line = transform().map(line);

		setTipOffset(QLineF(end->getArrowTipPos(line), line.p2()).length());
	} else {
		setTipOffset(1.0);
	}
}

void NetGraphSceneEdge::setFastMode(bool fastMode)
{
	this->fastMode = fastMode;
	if (fastMode) {
		offset = 1;
		tipw = 2;
		tiph = 3;
		textSpacing = 2;
	} else {
		offset = 5;
		tipw = 6;
		tiph = 8;
		textSpacing = 2;
	}
	updatePosition();
}

void NetGraphSceneEdge::setUsed(bool used)
{
	this->used = used;
	updateVisibility();
}

void NetGraphSceneEdge::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	updateVisibility();
}

void NetGraphSceneEdge::setEdgesHidden(bool edgesHidden)
{
	this->edgesHidden = edgesHidden;
	updateVisibility();
}

void NetGraphSceneEdge::setSelected(bool newSelected)
{
	selected = newSelected;
	updateColor();
}

void NetGraphSceneEdge::setHovered(bool newHovered)
{
	hovered = newHovered;
	updateColor();
}

void NetGraphSceneEdge::setText(QString text)
{
	this->text = text;
	textItem->setText(text);
	updatePosition();
}

void NetGraphSceneEdge::setFlowEdge(int extraOffset, QColor color)
{
	flowEdge = true;
	offset += (extraOffset % 10) * (flowSpacing + width);
	this->color = color;
	updateColor();
	updatePosition();
	setAcceptedMouseButtons(0);
	setText("");
}

void NetGraphSceneEdge::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mousePressed(mouseEvent, this);
}

void NetGraphSceneEdge::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mouseMoved(mouseEvent, this);
}

void NetGraphSceneEdge::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mouseReleased(mouseEvent, this);
}

void NetGraphSceneEdge::hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverEnter(hoverEvent, this);
}

void NetGraphSceneEdge::hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverMove(hoverEvent, this);
}

void NetGraphSceneEdge::hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverLeave(hoverEvent, this);
}
