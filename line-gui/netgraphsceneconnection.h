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

#ifndef NETGRAPHSCENECONNECTION_H
#define NETGRAPHSCENECONNECTION_H

#include <QGraphicsItem>
#include <QtGui>

#include "netgraph.h"
#include "netgraphscenenode.h"

class NetGraphSceneConnection : public QGraphicsObject
{
	Q_OBJECT

public:
    explicit NetGraphSceneConnection(int startIndex, int endIndex, int connectionIndex, qint32 trafficClass = 0,
                                     bool onOff = false, QGraphicsItem *parent = 0, QGraphicsScene *scene = 0);
	int startIndex;
	int endIndex;
	int connectionIndex;
	QPointF startPoint;
	QPointF endPoint;
	NetGraphSceneNode *start; // may be NULL
	NetGraphSceneNode *end; // may be NULL
	bool selected;
	bool hovered;
	QString text;
	QFont font;
	QColor color;
	bool fastMode;
    qint32 trafficClass;
    bool onOff;

	QRectF boundingRect() const;
	QPainterPath shape() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void updateShape() { prepareGeometryChange(); }
	void setSelected(bool newSelected);
	void setHovered(bool newHovered);
	void setText(QString text);
	void setFastMode(bool fastMode);
	void setVisibility(bool visible);

    static QColor getColorByIndex(int index, qint32 trafficClass, bool onOff);

signals:
	void mousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c);
	void mouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c);
	void mouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c);
	void hoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);
	void hoverMove(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);
	void hoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);

public slots:
	void setStartPoint(QPointF newStartPoint);
	void setEndPoint(QPointF newEndPoint);
	void setStartNode(NetGraphSceneNode *newStart);
	void setEndNode(NetGraphSceneNode *newEnd);

protected:
	// drawing parameters
	float offset;
	float tipw;
	float tiph;
	float textSpacing;
	float tipyFull;

	void updateTipOffset();
	void setTipOffset(qreal newTipOffset);

	void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent);
	void hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent);
};


#endif // NETGRAPHSCENECONNECTION_H
