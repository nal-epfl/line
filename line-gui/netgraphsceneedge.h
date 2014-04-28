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

#ifndef NETGRAPHSCENEEDGE_H
#define NETGRAPHSCENEEDGE_H

#include <QtGui>

#include "netgraph.h"
#include "netgraphscenenode.h"

class NetGraphSceneEdge : public QGraphicsObject
{
	Q_OBJECT

public:
	explicit NetGraphSceneEdge(int startIndex, int endIndex, int edgeIndex,
						  QGraphicsItem *parent = 0, QGraphicsScene *scene = 0);
	int startIndex;
	int endIndex;
	int edgeIndex;
	QPointF startPoint;
	QPointF endPoint;
	NetGraphSceneNode *start; // may be NULL
	NetGraphSceneNode *end; // may be NULL
	bool selected;
	bool hovered;
	QString text;
	QColor color;
	QFont font;
	bool fastMode;
	bool used;
	bool unusedHidden;
	bool edgesHidden;
	bool flowEdge;
	qreal width;
	qreal flowSpacing;

	QGraphicsLineItem *lineItem;
	QGraphicsPolygonItem *tipItem;
	QGraphicsSimpleTextItem *textItem;


	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void setSelected(bool newSelected);
	void setHovered(bool newHovered);
	void setText(QString text);
	void setFastMode(bool fastMode);
	void setUsed(bool used);
	void setUnusedHidden(bool unusedHidden);
	void setEdgesHidden(bool edgesHidden);
	void setFlowEdge(int extraOffset, QColor color);
	void updateColor();
	void updateVisibility();

signals:
	void mousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge);
	void mouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge);
	void mouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge);
	void hoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);
	void hoverMove(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);
	void hoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);

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
	void updatePosition();

	void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent);
	void hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent);
};

#endif // NETGRAPHSCENEEDGE_H
