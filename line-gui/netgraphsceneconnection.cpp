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

#include "netgraphsceneconnection.h"
#include "../util/colortable.h"

#define PI 3.14159265359
#define RAD_TO_DEG (180.0 / 3.14159265359)
#define DEG_TO_RAD (3.14159265359 / 180.0)

NetGraphSceneConnection::NetGraphSceneConnection(int startIndex, int endIndex, int connectionIndex,
                              qint32 trafficClass, bool onOff, QGraphicsItem *parent, QGraphicsScene *scene) :
	QGraphicsObject(parent)
{
	if (scene && !parent)
	    scene->addItem(this);

	this->startIndex = startIndex;
	this->endIndex = endIndex;
	this->connectionIndex = connectionIndex;
    this->trafficClass = trafficClass;
    this->onOff = onOff;
	this->setAcceptHoverEvents(true);
	start = NULL;
	end = NULL;
	text = "";
	selected = false;
	hovered = false;
	font = QFont("Arial", 8);
	// how far from the line that connects the centers of the nodes should we draw the arrow
	offset = 7;
	tipw = 6;
	tiph = 8;
	tipyFull = NETGRAPH_NODE_RADIUS;
	textSpacing = 2;
    color = getColorByIndex(connectionIndex, trafficClass, onOff);
}

QColor NetGraphSceneConnection::getColorByIndex(int index, qint32 trafficClass, bool onOff)
{
//	const int colorCount = 30;
//	const double alpha = 1.0;
//	double hue = (abs((index * 2654435761) ^ index) % colorCount) / (double)colorCount;
//	return QColor::fromHslF(hue, 0.5, 0.5, alpha);
    QColor color;
    if (trafficClass == 0) {
        color = ColorTable::getCool24().getColor(index);
    } else if (trafficClass == 1) {
        color = ColorTable::getWarm24().getColor(index);
    } else {
        color = Qt::darkGray;
    }
    if (onOff) {
		color.setAlphaF(0.3);
    } else {
        color.setAlphaF(1.0);
    }
    return color;
}

QRectF NetGraphSceneConnection::boundingRect() const
{
	return shape().boundingRect();
}

QPainterPath NetGraphSceneConnection::shape() const
{
	double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
				    (endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));

	QFontMetricsF fm(font);
	double textHeight = fm.height();

	QPolygonF p = QPolygonF(QRectF(-(textHeight + textSpacing), -dist, textHeight + textSpacing + tipw/2, dist));

	QTransform transform;
	transform = transform.translate(startPoint.x(), startPoint.y());
	transform = transform.rotate(90 + atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG);
	transform = transform.translate(-offset, 0);

	p = p * transform;

	QPainterPath path;
	path.addPolygon(p);
	return path;
}

void NetGraphSceneConnection::paint(QPainter *painter,
						const QStyleOptionGraphicsItem *option,
						QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (startIndex == endIndex)
		return;

	if (selected) {
		if (hovered) {
			painter->setPen(Qt::cyan);
			painter->setBrush(Qt::cyan);
		} else {
			painter->setPen(Qt::blue);
			painter->setBrush(Qt::blue);
		}
	} else {
		if (hovered) {
			painter->setPen(Qt::darkGray);
			painter->setBrush(Qt::darkGray);
		} else {
			painter->setPen(Qt::black);
			painter->setBrush(Qt::black);
		}
	}

	double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
				    (endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));

	QTransform transform = painter->transform();

	painter->translate(startPoint);
	painter->rotate(270 + atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG);
	painter->translate(offset, 0);

	QPen pen;
	pen.setColor(color);
	pen.setStyle(Qt::DashLine);
	painter->setPen(pen);
	QBrush brush;
	brush.setColor(color);
	brush.setStyle(Qt::SolidPattern);

	painter->setBrush(brush);

	painter->drawLine(0, 0, 0, dist);
	QPolygonF tip;
	float tipy;
	if (!fastMode) {
		tipy = tipyFull;
	} else {
		tipy = sqrt(NETGRAPH_NODE_RADIUS_FAST * NETGRAPH_NODE_RADIUS_FAST - offset * offset);
	}
	tip << QPointF(0, dist - tipy)
			<< QPointF(tipw/2, dist - tipy - tiph)
			<< QPointF(-tipw/2, dist - tipy - tiph);
	painter->drawPolygon(tip);

	if (!fastMode) {
		QFontMetricsF fm(font);
		double textWidth = fm.width(text);
		double textHeight = fm.height();

		if (1 || textWidth + textSpacing <= dist - 2 * tipy) {
			painter->setFont(font);

			double angle = atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG;

			if (fabs(angle) <= 90) {
				painter->rotate(90);

				painter->drawText(tipy + textSpacing, -(int)(textSpacing + textHeight + 0.5), (int)(dist - 2 * tipy + 0.5), (int)(textHeight + 0.5), Qt::AlignCenter | Qt::AlignVCenter, text);
			} else {
				painter->rotate(-90);
				painter->drawText(-tipy + textSpacing - (int)(dist - 2 * tipy + 0.5), textSpacing, (int)(dist - 2 * tipy + 0.5), (int)(textHeight + 0.5), Qt::AlignCenter | Qt::AlignVCenter, text);
				// painter->drawText(textX, textSpacing, (int)(dist - 2 * tipy + 0.5), (int)(textHeight + 0.5), Qt::AlignCenter | Qt::AlignVCenter, text);
			}
		}
	}

	painter->setTransform(transform);
	painter->setPen(Qt::red);
	painter->setBrush(Qt::NoBrush);
//	painter->drawPath(shape());
//	painter->drawRect(boundingRect());
}

void NetGraphSceneConnection::setStartPoint(QPointF newStartPoint)
{
	startPoint = newStartPoint;
	updateTipOffset();
	updateShape();
}

void NetGraphSceneConnection::setEndPoint(QPointF newEndPoint)
{
	endPoint = newEndPoint;
	updateTipOffset();
	updateShape();
}

void NetGraphSceneConnection::setStartNode(NetGraphSceneNode *newStart)
{
	start = newStart;
	updateTipOffset();
	updateShape();
}

void NetGraphSceneConnection::setEndNode(NetGraphSceneNode *newEnd)
{
	end = newEnd;
	updateTipOffset();
	updateShape();
}

void NetGraphSceneConnection::setFastMode(bool fastMode)
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
	update();
}

void NetGraphSceneConnection::setVisibility(bool visible)
{
	setVisible(visible && startIndex != endIndex);
}

void NetGraphSceneConnection::setSelected(bool newSelected)
{
	selected = newSelected;
	update();
}

void NetGraphSceneConnection::setHovered(bool newHovered)
{
	hovered = newHovered;
	update();
}

void NetGraphSceneConnection::setText(QString text)
{
	this->text = text;
	update();
}

void NetGraphSceneConnection::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mousePressed(mouseEvent, this);
}

void NetGraphSceneConnection::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mouseMoved(mouseEvent, this);
}

void NetGraphSceneConnection::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mouseReleased(mouseEvent, this);
}

void NetGraphSceneConnection::hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverEnter(hoverEvent, this);
}

void NetGraphSceneConnection::hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverMove(hoverEvent, this);
}

void NetGraphSceneConnection::hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverLeave(hoverEvent, this);
}

void NetGraphSceneConnection::updateTipOffset()
{
	if (start && end) {
		double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
						(endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));
		QLineF line(0, 0, 0, dist);

		QTransform t;
		t.translate(startPoint.x(), startPoint.y());
		qreal angle = 270 + atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG;
		t.rotate(angle);
		t.translate(offset, 0);

		line = t.map(line);

		setTipOffset(QLineF(end->getArrowTipPos(line), line.p2()).length());
	} else {
		setTipOffset(1.0);
	}
}

void NetGraphSceneConnection::setTipOffset(qreal newTipOffset)
{
	tipyFull = newTipOffset;
	update();
}
