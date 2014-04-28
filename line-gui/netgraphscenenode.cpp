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

#include "netgraphscenenode.h"

#include "convexhull.h"

NetGraphSceneNode::NetGraphSceneNode(int nodeIndex, int nodeType,
									 QPixmap pixmap,
									 QGraphicsItem *parent, QGraphicsScene *scene) :
	QGraphicsObject(parent), pixmap(pixmap)
{
	if (scene && !parent)
		scene->addItem(this);

	font = QFont("Arial", 12);
	font.setBold(true);
	spacing = 4;
	padding = 4;

	this->nodeIndex = nodeIndex;
	this->nodeType = nodeType;
	this->setAcceptHoverEvents(true);
	selected = false;
	used = true;
	unusedHidden = false;
	useVectorGraphics = false;
    web = false;
    p2p = false;
    vvoip = false;
    iptv = false;
	server = false;
	heavy = false;

	svgItem = NULL;
	pixmapItem = NULL;
	textItem = new QGraphicsSimpleTextItem(this);
	textItem->setBrush(Qt::black);
	textItem->setFont(font);

	setLabel(label);
    setVectorGraphics(useVectorGraphics);
}

void NetGraphSceneNode::setPos(const QPointF &pos)
{
	QGraphicsItem::setPos(pos);
	emit positionChanged(pos);
}

void NetGraphSceneNode::setSel(bool newSelected)
{
	selected = newSelected;
	update();
}

void NetGraphSceneNode::setFastMode(bool fastMode)
{
	this->fastMode = fastMode;
	textItem->setVisible(!fastMode);
	if (svgItem)
		svgItem->setVisible(!fastMode);
	if (pixmapItem)
		pixmapItem->setVisible(!fastMode);
	update();
}

void NetGraphSceneNode::setUsed(bool used)
{
	this->used = used;
	updateVisibility();
}

void NetGraphSceneNode::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	updateVisibility();
}

void NetGraphSceneNode::setLabel(QString label)
{
	if (label.isEmpty()) {
		this->label = QString::number(nodeIndex + 1);
	} else {
		this->label = label;
	}
	textItem->setText(this->label);
	textItem->setPos(-textItem->boundingRect().width()/2.0, -textItem->boundingRect().height()/2.0);
	setFastMode(fastMode);
}

const QPolygonF NetGraphSceneNode::getTightBgPoly() const
{
	if (tightBgPoly.count() >= 3)
		return tightBgPoly;
	QImage transparencyMask = pixmap.toImage();

	QList<QPointF> opaquePoints;

	for (int x = 0; x < transparencyMask.width(); x++) {
		for (int y = 0; y < transparencyMask.height(); y++) {
			if ((transparencyMask.pixel(x, y) & 0xFF000000) == 0xFF000000) {
				opaquePoints << QPointF(x - transparencyMask.width()/2.0, y - transparencyMask.height()/2.0);
			}
		}
	}

	if (opaquePoints.count() >= 3) {
		tightBgPoly = ConvexHull::giftWrap(opaquePoints).toVector();
	} else {
		tightBgPoly.clear();
	}

	return tightBgPoly;
}

void NetGraphSceneNode::setVectorGraphics(bool enabled)
{
	useVectorGraphics = enabled && !svgPath.isEmpty();
	if (useVectorGraphics) {
		if (!svgItem) {
			svgItem = new QGraphicsSvgItem(svgPath, this);
			svgItem->setPos(-pixmap.width()/2.0, -pixmap.height()/2.0);
			svgItem->setScale(pixmap.height()/svgItem->boundingRect().height());
			svgItem->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
		}
		delete pixmapItem;
		pixmapItem = NULL;
	} else {
		if (!pixmapItem) {
			pixmapItem = new QGraphicsPixmapItem(pixmap, this);
			pixmapItem->setPos(-pixmap.width()/2.0, -pixmap.height()/2.0);
			pixmapItem->setScale(pixmap.height()/pixmapItem->boundingRect().height());
			pixmapItem->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
		}
		delete svgItem;
		svgItem = NULL;
	}
	setFastMode(fastMode);
}

QRectF NetGraphSceneNode::boundingRect() const
{
	return getTextRect().united(getBgRect());
}

QString NetGraphSceneNode::getLabel() const
{
	return label;
}

QRectF NetGraphSceneNode::getBgRect() const
{
	if (!fastMode) {
		return QRectF(-pixmap.width()/2, -pixmap.height()/2, pixmap.width(), pixmap.height()).united(QRectF(-NETGRAPH_NODE_RADIUS, -NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS));
	} else {
		return QRectF(-NETGRAPH_NODE_RADIUS_FAST, -NETGRAPH_NODE_RADIUS_FAST,
					  2*NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST);
	}
}

qreal fsgn(qreal x)
{
	if (x > 1.0e-9)
		return 1.0;
	if (x < -1.0e-9)
		return 1.0;
	return 0;
}

// only for non-fast mode
QPointF NetGraphSceneNode::getArrowTipPos(QLineF arrow) const
{
	arrow.translate(-pos());
	if (nodeType == NETGRAPH_NODE_SQUARE) {
		QRectF rect = boundingRect();
		qreal a, b;
		a = rect.width()/2.0;
		b = rect.height()/2.0;

		QList<QPointF> intersections;

		QPointF p;
		if (QLineF(-a, b, a, b).intersect(arrow, &p) == QLineF::BoundedIntersection) {
			intersections << p;
		}
		if (QLineF(-a, -b, a, -b).intersect(arrow, &p) == QLineF::BoundedIntersection) {
			intersections << p;
		}
		if (QLineF(-a, -b, -a, b).intersect(arrow, &p) == QLineF::BoundedIntersection) {
			intersections << p;
		}
		if (QLineF(a, -b, a, b).intersect(arrow, &p) == QLineF::BoundedIntersection) {
			intersections << p;
		}

		QPointF pbest = arrow.p2();
		qreal lbest = QLineF(arrow.p1(), pbest).length();
		foreach (QPointF i, intersections) {
			qreal l = QLineF(arrow.p1(), i).length();
			if (l < lbest) {
				lbest = l;
				pbest = i;
			}
		}
		return pbest + pos();
	} else if (nodeType == NETGRAPH_NODE_ELLIPSE) {
		QRectF rect = boundingRect();
		qreal a, b;
		a = rect.width()/2.0;
		b = rect.height()/2.0;

		QPointF int1, int2;
		int1 = int2 = arrow.p2();

		if (qFuzzyCompare(arrow.x1(), arrow.x2())) {
			// vertical line
			qreal x, y;

			if (a >= fabs(arrow.x1())) {
				x = arrow.x1();
				y = b/a * sqrt(a*a - x*x);
				int1 = QPointF(x, y);

				x = arrow.x1();
				y = -b/a * sqrt(a*a - x*x);
				int2 = QPointF(x, y);
			} else {
				// no intersection
				return arrow.p2() + pos();
			}
		} else {
			qreal m, n;

			m = (arrow.y2() - arrow.y1())/(arrow.x2() - arrow.x1());
			n = arrow.y1() - m*arrow.x1();

			qreal A, B, C, delta;
			A = b*b + a*a*m*m;
			B = 2*a*a*m*n;
			C = a*a*n*n - a*a*b*b;
			delta = B*B - 4*A*C;

			if (delta >= 0) {
				qreal x, y;
				x = (-B - sqrt(delta))/(2*A);
				y = m*x + n;
				int1 = QPointF(x, y);

				x = (-B + sqrt(delta))/(2*A);
				y = m*x + n;
				int2 = QPointF(x, y);
			} else {
				// no intersection
				return arrow.p2() + pos();
			}
		}

		if (QLineF(arrow.p1(), int1).length() < QLineF(arrow.p1(), int2).length()) {
			return int1 + pos();
		} else {
			return int2 + pos();
		}
	} else {
		QList<QPointF> intersections;

		QPolygonF poly = getTightBgPoly();
		if (poly.count() < 3) {
			poly = getBgRect();
		}
		poly.append(poly.first());
		for (int i = 0; i < poly.count()-1; i++) {
			QLineF line = QLineF(poly[i], poly[i+1]);
			QPointF p;
			if (line.intersect(arrow, &p) == QLineF::BoundedIntersection) {
				intersections << p;
			}
		}

		QPointF pbest = arrow.p2();
		qreal lbest = QLineF(arrow.p1(), pbest).length();
		foreach (QPointF i, intersections) {
			qreal l = QLineF(arrow.p1(), i).length();
			if (l < lbest) {
				lbest = l;
				pbest = i;
			}
		}

		return pbest + pos();
	}
}

QRectF NetGraphSceneNode::getTextRect() const
{
	if (!fastMode) {
		QRectF textBoundingRect = textItem->boundingRect();

		return QRectF(-textBoundingRect.width()/2 - padding, textBoundingRect.height()/2 - textBoundingRect.height() - padding, textBoundingRect.width() + 2*padding, textBoundingRect.height() + 2*padding);
	} else {
		return QRectF(pos(), QSizeF(0, 0));
	}
}

QPointF NetGraphSceneNode::getTextPos() const
{
	if (!fastMode) {
		QFont textFont = font;
		textFont.setBold(true);
		QFontMetricsF fm(textFont);
		QRectF textBoundingRect = fm.tightBoundingRect(getLabel());

		//return QPointF(-textBoundingRect.width()/2.0, pixmap.height()/2 - fm.descent() - spacing);
		return QPointF(-textBoundingRect.width()/2.0, textBoundingRect.height()/2);
	} else {
		return pos();
	}
}

void NetGraphSceneNode::paint(QPainter *painter,
							  const QStyleOptionGraphicsItem *option,
							  QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (!used && unusedHidden)
		return;

	if (!fastMode) {
		if (nodeType == NETGRAPH_NODE_SQUARE) {
            painter->setPen(Qt::black);
			painter->setBrush(bgColor);
			painter->drawRect(boundingRect());
		} else if (nodeType == NETGRAPH_NODE_ELLIPSE) {
            painter->setPen(Qt::black);
			painter->setBrush(bgColor);
			painter->drawEllipse(boundingRect());
		}
	} else {
        painter->setPen(Qt::black);
        if (nodeType == NETGRAPH_NODE_HOST) {
            painter->setBrush(Qt::green);
        } else if (nodeType == NETGRAPH_NODE_GATEWAY) {
            painter->setBrush(Qt::yellow);
        } else if (nodeType == NETGRAPH_NODE_ROUTER) {
            painter->setBrush(Qt::blue);
        } else if (nodeType == NETGRAPH_NODE_BORDER) {
            painter->setBrush(Qt::gray);
        } else if (nodeType == NETGRAPH_NODE_SQUARE || nodeType == NETGRAPH_NODE_ELLIPSE) {
            painter->setBrush(Qt::white);
        }
		painter->drawEllipse(-NETGRAPH_NODE_RADIUS_FAST, -NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST);
	}

	if (selected) {
		if (!fastMode) {
			painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
			painter->setBrush(Qt::NoBrush);
			painter->drawRect(-NETGRAPH_NODE_RADIUS, -NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS);
		} else {
			painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
			painter->setBrush(Qt::NoBrush);
			painter->drawRect(-NETGRAPH_NODE_RADIUS_FAST, -NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST);
		}
	}
}

void NetGraphSceneNode::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseEvent->accept();
	emit mousePressed(mouseEvent, this);
}

void NetGraphSceneNode::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseEvent->accept();
	emit mouseMoved(mouseEvent, this);
}

void NetGraphSceneNode::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseEvent->accept();
	emit mouseReleased(mouseEvent, this);
}

void NetGraphSceneNode::hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	hoverEvent->accept();
	emit hoverEnter(hoverEvent, this);
}

void NetGraphSceneNode::hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	hoverEvent->accept();
	emit hoverMove(hoverEvent, this);
}

void NetGraphSceneNode::hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	hoverEvent->accept();
	emit hoverLeave(hoverEvent, this);
}


void NetGraphSceneNode::updateVisibility()
{
	if (!used && unusedHidden) {
		setVisible(false);
	} else {
		setVisible(true);
    }
}

void NetGraphSceneNode::setTrafficType(bool web, bool vvoip, bool p2p, bool iptv, bool server, bool heavy)
{
    this->web = web;
    this->vvoip = vvoip;
    this->p2p = p2p;
    this->iptv = iptv;
	this->server = server;
	this->heavy = heavy;
	updateSvgPath();
}

void NetGraphSceneNode::updateSvgPath()
{
    if (nodeType == NETGRAPH_NODE_HOST) {
		if (web && !vvoip && !p2p && !iptv && !heavy) {
			if (!server) {
				svgPath = ":/icons/extra-resources/host-green.svg";
				pixmap.load(":/icons/extra-resources/host-green_32.png");
			} else {
				svgPath = ":/icons/extra-resources/host-darkgreen.svg";
				pixmap.load(":/icons/extra-resources/host-darkgreen_32.png");
			}
		} else if (!web && vvoip && !p2p && !iptv && !heavy) {
			if (!server) {
				svgPath = ":/icons/extra-resources/host-yellow.svg";
				pixmap.load(":/icons/extra-resources/host-yellow_32.png");
			} else {
				svgPath = ":/icons/extra-resources/host-darkyellow.svg";
				pixmap.load(":/icons/extra-resources/host-darkyellow_32.png");
			}
		} else if (!web && !vvoip && p2p && !iptv && !heavy) {
			if (!server) {
				svgPath = ":/icons/extra-resources/host-blue.svg";
				pixmap.load(":/icons/extra-resources/host-blue_32.png");
			} else {
				svgPath = ":/icons/extra-resources/host-darkblue.svg";
				pixmap.load(":/icons/extra-resources/host-darkblue_32.png");
			}
		} else if (!web && !vvoip && !p2p && iptv && !heavy) {
			if (!server) {
				svgPath = ":/icons/extra-resources/host-purple.svg";
				pixmap.load(":/icons/extra-resources/host-purple_32.png");
			} else {
				svgPath = ":/icons/extra-resources/host-darkpurple.svg";
				pixmap.load(":/icons/extra-resources/host-darkpurple_32.png");
			}
		} else {
			if (heavy) {
				svgPath = ":/icons/extra-resources/host-red.svg";
				pixmap.load(":/icons/extra-resources/host-red_32.png");
			} else {
				svgPath = ":/icons/extra-resources/video-display.svg";
				pixmap.load(":/icons/extra-resources/video-display-32.png");
			}
		}
    } else if (nodeType == NETGRAPH_NODE_GATEWAY) {
        svgPath = ":/icons/extra-resources/juanjo_Router_gateway.svg";
    } else if (nodeType == NETGRAPH_NODE_ROUTER) {
        svgPath = ":/icons/extra-resources/juanjo_Router.svg";
    } else if (nodeType == NETGRAPH_NODE_BORDER) {
        svgPath = ":/icons/extra-resources/juanjo_Router_border.svg";
    }

    if (svgItem) {
       svgItem->scene()->removeItem(svgItem);
    }
    delete svgItem;
    svgItem = 0;
    if (pixmapItem) {
        pixmapItem->scene()->removeItem(pixmapItem);
    }
    delete pixmapItem;
    pixmapItem = 0;
    setVectorGraphics(useVectorGraphics);
    update();
}
