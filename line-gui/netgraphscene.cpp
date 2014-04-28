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

#include "netgraphscene.h"

#include "qgraphicstooltip.h"

class QNetGraphSceneDummyItem : public QGraphicsItem
{
public:
	explicit QNetGraphSceneDummyItem(QGraphicsItem *parent = 0) : QGraphicsItem(parent) {
		setFlag(QGraphicsItem::ItemHasNoContents);
		setAcceptHoverEvents(true);
		setZValue(-100000);
		setPos(0, 0);
	}

	QRectF boundingRect() const
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

	void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *)
	{
	}
};

NetGraphScene::NetGraphScene(QObject *parent) :
	QGraphicsScene(parent)
{
	netGraph = QSharedPointer<NetGraph>(new NetGraph());
	newEdge = 0;
	newConnection = 0;
    selectedHost = 0;
    defaultHost.index = -1;
    selectedEdge = 0;
    defaultEdge.index = -1;
	editMode = MoveNode;
	enabled = true;
	fastMode = false;
	fastModeAuto = true;
	unusedHidden = false;
	defaultASNumber = 0;
	connectionType = "TCP";
	hideConnections = false;
	hideFlows = false;
	hideEdges = false;
	hideDomains = false;
	mouseDown = false;
	pixmapHost.load(":/icons/extra-resources/video-display-32.png");
	pixmapRouter.load(":/icons/extra-resources/juanjo_Router_32.png");
	pixmapBorderRouter.load(":/icons/extra-resources/juanjo_Router_border_32.png");
	pixmapGateway.load(":/icons/extra-resources/juanjo_Router_gateway_32.png");
	tooltipTarget = 0;
	selectAreaRect = 0;
	root = new QNetGraphSceneDummyItem();
	addItem(root);
	initTooltip();
	reload();
	emit edgeSelected(defaultEdge);
}

NetGraph *NetGraphScene::graph()
{
	return netGraph.data();
}

void NetGraphScene::setMode(EditMode mode)
{
	if (!enabled) {
		editMode = mode;
		return;
	}
	if (editMode == EditHost && mode != EditHost) {
		if (selectedHost) {
			selectedHost->setSel(false);
			selectedHost = 0;
		}
        emit hostSelected(defaultHost);
    }
	if (editMode == EditEdge && mode != EditEdge) {
		if (selectedEdge) {
			selectedEdge->setSelected(false);
			selectedEdge = 0;
		}
		emit edgeSelected(defaultEdge);
	}
	if (editMode == InsertEdgeEnd && mode != InsertEdgeEnd && startNode) {
		startNode->setSel(false);
		startNode->ungrabMouse();
		newEdge->setVisible(false);
		startNode = 0;
	}
	if (editMode == InsertConnectionEnd && mode != InsertConnectionEnd && startNode) {
		startNode->setSel(false);
		startNode->ungrabMouse();
		newConnection->setVisible(false);
		startNode = 0;
	}

	if (mode == Drag && editMode != Drag ) {
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
			if (edge && !edge->flowEdge) {
				edge->setAcceptedMouseButtons(Qt::NoButton);
			}
			NetGraphSceneNode *node = dynamic_cast<NetGraphSceneNode*>(item);
			if (node) {
				node->setAcceptedMouseButtons(Qt::NoButton);
			}
		}
	} else if (editMode == Drag && mode != Drag) {
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
			if (edge && !edge->flowEdge) {
				edge->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
			}
			NetGraphSceneNode *node = dynamic_cast<NetGraphSceneNode*>(item);
			if (node) {
				node->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
			}
		}
	}

	if (editMode == ShowRoutes && mode != ShowRoutes) {
		if (tooltipTarget) {
			tooltip->setVisible(false);
			tooltipTarget = 0;
		}
	}

	editMode = mode;
}

void NetGraphScene::setEnabled(bool val)
{
	enabled = val;
	if (enabled)
		reload();
}

void NetGraphScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseDown = true;
	if (!enabled)
		return;
	if (editMode == Drag) {
		oldMousePos = mouseEvent->scenePos();
		oldViewportCenter = graph()->viewportCenter;
		mouseEvent->accept();
		return;
	}

	if (editMode == InsertHost) {
        addNode(NETGRAPH_NODE_HOST, mouseEvent->scenePos() - root->pos(),
				defaultHost.web, defaultHost.vvoip, defaultHost.p2p,
				defaultHost.iptv, defaultHost.server, defaultHost.heavy);
		routingChanged();
        emit graphChanged();
	} else if (editMode == InsertGateway) {
		addNode(NETGRAPH_NODE_GATEWAY, mouseEvent->scenePos() - root->pos());
		routingChanged();
        emit graphChanged();
	} else if (editMode == InsertRouter) {
		addNode(NETGRAPH_NODE_ROUTER, mouseEvent->scenePos() - root->pos());
		routingChanged();
        emit graphChanged();
	} else if (editMode == InsertBorderRouter) {
		addNode(NETGRAPH_NODE_BORDER, mouseEvent->scenePos() - root->pos());
		routingChanged();
        emit graphChanged();
	} else if (editMode == DelNodeArea) {
		oldMousePos = mouseEvent->scenePos() - root->pos();
		getNewSelectAreaRect()->setVisible(true);
		getNewSelectAreaRect()->setRect(QRectF(oldMousePos, QSizeF(0, 0)));
	} else if (editMode == MoveNodeArea) {
		oldMousePos = mouseEvent->scenePos() - root->pos();
		getNewSelectAreaRect()->setVisible(true);
		if (!getNewSelectAreaRect()->rect().contains(oldMousePos)) {
			getNewSelectAreaRect()->setRect(QRectF(oldMousePos, QSizeF(0, 0)));
			dragging = false;
		} else {
			oldRect = getNewSelectAreaRect()->rect();
			dragging = true;
		}
	}
	QGraphicsScene::mousePressEvent(mouseEvent);
}

void NetGraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	if (!enabled)
		return;
	if (editMode == Drag && mouseDown) {
		QPointF newPos = mouseEvent->scenePos();
		QPointF delta = oldMousePos - newPos;
		root->setPos(-delta);
		mouseEvent->accept();
		return;
	} else if (editMode == DelNodeArea) {
		QPointF newMousePos = mouseEvent->scenePos() - root->pos();
		getNewSelectAreaRect()->setVisible(true);
		getNewSelectAreaRect()->setRect(QRectF(oldMousePos, newMousePos).normalized());
	} else if (editMode == MoveNodeArea) {
		if (!dragging) {
			QPointF newMousePos = mouseEvent->scenePos() - root->pos();
			getNewSelectAreaRect()->setVisible(true);
			getNewSelectAreaRect()->setRect(QRectF(oldMousePos, newMousePos).normalized());
		} else if (mouseEvent->buttons() & Qt::LeftButton) {
			QPointF newMousePos = mouseEvent->scenePos() - root->pos();
			QPointF delta = newMousePos - oldMousePos;
			getNewSelectAreaRect()->setRect(oldRect.adjusted(delta.x(), delta.y(), delta.x(), delta.y()));
		}
	}
	QGraphicsScene::mouseMoveEvent(mouseEvent);
}

void NetGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseDown = false;
	if (!enabled)
		return;
	if (editMode == Drag) {
		graph()->viewportCenter -= root->pos();
		root->setPos(0, 0);
		emit viewportChanged();
        emit graphChanged();
	} else if (editMode == DelNodeArea) {
		QRectF selRect = getNewSelectAreaRect()->rect();
		QList<NetGraphSceneNode *> selNodes;
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
			if (n) {
				if (selRect.contains(n->pos())) {
					selNodes << n;
				}
			}
		}

		if (!selNodes.isEmpty()) {
			foreach (NetGraphSceneNode *n, selNodes) {
				n->setSel(true);
			}

			if (QMessageBox::question(0, "Delete nodes", "Are you sure you want to delete all the selected nodes? The operation cannot be undone.",
								  QMessageBox::Yes, QMessageBox::Cancel) == QMessageBox::Yes) {
				QList<int> indices;
				foreach (NetGraphSceneNode *n, selNodes) {
					indices << n->nodeIndex;
				}
				qSort(indices);
				while (!indices.isEmpty()) {
					int nodeIndex = indices.takeLast();
					netGraph->deleteNode(nodeIndex);
				}
				reload();
			} else {
				foreach (NetGraphSceneNode *n, selNodes) {
					n->setSel(false);
				}
			}
            emit graphChanged();
		}

		getNewSelectAreaRect()->setVisible(false);
	} else if (editMode == MoveNodeArea) {
		if (dragging) {
			QRectF selRect = oldRect;
			foreach (QGraphicsItem *item, items()) {
				NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
				if (n) {
					if (selRect.contains(n->pos())) {
						QPointF newMousePos = mouseEvent->scenePos();
						QPointF newPos = n->pos() + newMousePos - oldMousePos;
						n->setPos(newPos);
						netGraph->nodes[n->nodeIndex].x = n->pos().x();
						netGraph->nodes[n->nodeIndex].y = n->pos().y();
					}
				}
			}
			// recompute AS hulls
			netGraph->computeASHulls();
			foreach (QGraphicsItem *item, items()) {
				NetGraphSceneAS *domain = dynamic_cast<NetGraphSceneAS*>(item);
				if (domain) {
					domain->setHull(netGraph->domains[domain->ASNumber].hull);
				}
			}
            emit graphChanged();
		} else {
			oldRect = getNewSelectAreaRect()->rect();
		}
	}
	QGraphicsScene::mouseReleaseEvent(mouseEvent);
}

void NetGraphScene::wheelEvent(QGraphicsSceneWheelEvent *wheelEvent)
{
	emit wheelEventFired(wheelEvent);
}

NetGraphSceneNode *NetGraphScene::addNode(int type, QPointF pos,
                                          bool web, bool vvoip,
										  bool p2p, bool iptv,
										  bool server, bool heavy)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	int nodeIndex = netGraph->addNode(type, pos, defaultASNumber);
    netGraph->nodes[nodeIndex].web = web;
    netGraph->nodes[nodeIndex].vvoip = vvoip;
    netGraph->nodes[nodeIndex].p2p = p2p;
    netGraph->nodes[nodeIndex].iptv = iptv;
	netGraph->nodes[nodeIndex].server = server;
	netGraph->nodes[nodeIndex].heavy = heavy;
	// update the AS hull
	netGraph->computeASHulls(defaultASNumber);

	// update the AS graphics item
	bool foundAS = false;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneAS *domain = dynamic_cast<NetGraphSceneAS*>(item);
		if (domain) {
			if (domain->ASNumber == defaultASNumber) {
				foundAS = true;
				foreach (NetGraphAS d, netGraph->domains) {
					if (d.ASNumber == defaultASNumber) {
						domain->setHull(d.hull);
						break;
					}
				}
				break;
			}
		}
	}
	if (!foundAS) {
		foreach (NetGraphAS d, netGraph->domains) {
			if (d.ASNumber == defaultASNumber) {
				addAS(d.index);
				break;
			}
		}
	}

	return addNode(nodeIndex);
}



NetGraphSceneNode *NetGraphScene::addNode(int index)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	QPixmap pixmap;
	int nodeType = netGraph->nodes[index].nodeType;
	if (nodeType == NETGRAPH_NODE_HOST) {
		pixmap = pixmapHost;
	} else if (nodeType == NETGRAPH_NODE_GATEWAY) {
		pixmap = pixmapGateway;
	} else if (nodeType == NETGRAPH_NODE_ROUTER) {
		pixmap = pixmapRouter;
	} else if (nodeType == NETGRAPH_NODE_BORDER) {
		pixmap = pixmapBorderRouter;
	}

	NetGraphSceneNode *item = new NetGraphSceneNode(index, nodeType, pixmap, root, this);
    item->setTrafficType(netGraph->nodes[index].web, netGraph->nodes[index].vvoip,
						 netGraph->nodes[index].p2p, netGraph->nodes[index].iptv,
						 netGraph->nodes[index].server, netGraph->nodes[index].heavy);
	item->setFastMode(fastMode);
	item->setPos(QPointF(netGraph->nodes[index].x, netGraph->nodes[index].y));
	item->setUsed(netGraph->nodes[index].used);
	item->setUnusedHidden(unusedHidden);
	item->setLabel(netGraph->nodes[index].customLabel);
    item->updateSvgPath();
	item->bgColor = QColor::fromRgb(netGraph->nodes[index].bgColor);
	item->setVectorGraphics(true);
	connect(item, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)), SLOT(nodeMousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)));
	connect(item, SIGNAL(mouseMoved(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)), SLOT(nodeMouseMoved(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)));
	connect(item, SIGNAL(mouseReleased(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)), SLOT(nodeMouseReleased(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)));
	connect(item, SIGNAL(hoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)), SLOT(nodeHoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)));
	connect(item, SIGNAL(hoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)), SLOT(nodeHoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)));
	return item;
}

NetGraphSceneAS *NetGraphScene::addAS(int index)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	NetGraphSceneAS *domain = new NetGraphSceneAS(index, netGraph->domains[index].ASNumber, netGraph->domains[index].hull, root, this);
	domain->setUsed(netGraph->domains[index].used);
	domain->setUnusedHidden(unusedHidden);
	domain->setZValue(-2);
	domain->setVisible(!hideDomains);
	return domain;
}

NetGraphSceneEdge *NetGraphScene::addEdge(int startIndex, int endIndex, double bandwidth, int delay, double loss, int queueLength,
								  bool recordSampledTimeline, quint64 timelineSamplingPeriod, NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	int edgeIndex = netGraph->addEdge(startIndex, endIndex, bandwidth, delay, loss, queueLength);
	netGraph->edges[edgeIndex].recordSampledTimeline = recordSampledTimeline;
	netGraph->edges[edgeIndex].timelineSamplingPeriod = timelineSamplingPeriod;
	return addEdge(edgeIndex, start, end);
}

NetGraphSceneEdge *NetGraphScene::addEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	NetGraphSceneEdge *edge = new NetGraphSceneEdge(netGraph->edges[index].source, netGraph->edges[index].dest, index, root, this);
	edge->setFastMode(fastMode);
	edge->setStartPoint(start->pos());
	edge->setEndPoint(end->pos());
	edge->setStartNode(start);
	edge->setEndNode(end);
	edge->setZValue(-1);
	edge->setText(netGraph->edges[edge->edgeIndex].tooltip());
	edge->setUsed(netGraph->edges[index].used);
	edge->setUnusedHidden(unusedHidden);
	edge->setEdgesHidden(hideEdges);
	edge->width = netGraph->edges[index].width;
	edge->color = QColor::fromRgba(netGraph->edges[index].getColor());
	edge->updateColor();
	connect(start, SIGNAL(positionChanged(QPointF)), edge, SLOT(setStartPoint(QPointF)));
	connect(end, SIGNAL(positionChanged(QPointF)), edge, SLOT(setEndPoint(QPointF)));
	connect(edge, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*, NetGraphSceneEdge*)), SLOT(edgeMousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneEdge*)));
	connect(edge, SIGNAL(hoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)), SLOT(edgeHoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)));
	connect(edge, SIGNAL(hoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)), SLOT(edgeHoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)));
	return edge;
}

NetGraphSceneEdge *NetGraphScene::addFlowEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end, int extraOffset, QColor color)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	NetGraphSceneEdge *edge = new NetGraphSceneEdge(netGraph->edges[index].source, netGraph->edges[index].dest, index, root, this);
	edge->setFastMode(fastMode);
	edge->setStartPoint(start->pos());
	edge->setEndPoint(end->pos());
	edge->setStartNode(start);
	edge->setEndNode(end);
	edge->setZValue(-2);
	edge->setText(netGraph->edges[edge->edgeIndex].tooltip());
	edge->setUsed(netGraph->edges[index].used);
	edge->setUnusedHidden(unusedHidden);
	edge->setFlowEdge(extraOffset, color);
	connect(start, SIGNAL(positionChanged(QPointF)), edge, SLOT(setStartPoint(QPointF)));
	connect(end, SIGNAL(positionChanged(QPointF)), edge, SLOT(setEndPoint(QPointF)));
	return edge;
}

NetGraphSceneConnection *NetGraphScene::addConnection(int startIndex, int endIndex,
								  NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	int connectionIndex = netGraph->addConnection(NetGraphConnection(startIndex, endIndex, connectionType, ""));
	netGraph->updateUsed();
	return addConnection(connectionIndex, start, end);
}

NetGraphSceneConnection *NetGraphScene::addConnection(int index, NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	NetGraphSceneConnection *c = new NetGraphSceneConnection(netGraph->connections[index].source,
															 netGraph->connections[index].dest,
															 index,
															 netGraph->connections[index].trafficClass,
															 netGraph->connections[index].onOff ||
															 netGraph->connections[index].basicType == "UDP-CBR",
                                                             root, this);
	c->setFastMode(fastMode);
	c->setStartPoint(start->pos());
	c->setEndPoint(end->pos());
	c->setStartNode(start);
	c->setEndNode(end);
	c->setZValue(-1);
	c->setText(netGraph->connections[c->connectionIndex].tooltip());
	c->setVisible(!hideConnections);
	if (netGraph->connections[index].color != 0)
		c->color = QColor::fromRgba(netGraph->connections[index].color);
	connect(start, SIGNAL(positionChanged(QPointF)), c, SLOT(setStartPoint(QPointF)));
	connect(end, SIGNAL(positionChanged(QPointF)), c, SLOT(setEndPoint(QPointF)));
	connect(c, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*, NetGraphSceneConnection*)), SLOT(connectionMousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneConnection*)));
	connect(c, SIGNAL(hoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)), SLOT(connectionHoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)));
	connect(c, SIGNAL(hoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)), SLOT(connectionHoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)));
	return c;
}

void NetGraphScene::nodeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node)
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	if (fastMode)
		return;
    if (editMode == MoveNode && mouseEvent->buttons() & Qt::LeftButton) {
		oldMousePos = mouseEvent->scenePos();
		oldNodePos = node->pos();
    } else if (editMode == EditHost && node->nodeType == NETGRAPH_NODE_HOST) {
        if (selectedHost == node) {
            node->setSel(false);
            selectedHost = 0;
            emit hostSelected(defaultHost);
        } else {
            if (selectedHost)
                selectedHost->setSel(false);
            node->setSel(true);
            selectedHost = node;
            emit hostSelected(netGraph->nodes[selectedHost->nodeIndex]);
        }
    } else if (editMode == DelNode && mouseEvent->buttons() & Qt::LeftButton) {
		int nodeIndex = node->nodeIndex;
		netGraph->deleteNode(nodeIndex);
		delete node;
		reload();
        emit graphChanged();
	} else if (editMode == InsertEdgeStart && mouseEvent->buttons() & Qt::LeftButton) {
		getNewEdge()->startIndex = getNewEdge()->endIndex = node->nodeIndex;
		getNewEdge()->setStartPoint(node->pos());
		getNewEdge()->setStartNode(node);
		getNewEdge()->setEndPoint(node->pos());
		getNewEdge()->setEndNode(NULL);
		startNode = node;
		editMode = InsertEdgeEnd;
		node->ungrabMouse();
		node->setSel(true);
	} else if (editMode == InsertEdgeEnd && mouseEvent->buttons() & Qt::LeftButton) {
		getNewEdge()->endIndex = node->nodeIndex;
		editMode = InsertEdgeStart;
		startNode->setSel(false);
		node->ungrabMouse();
		if (netGraph->canAddEdge(newEdge->startIndex, newEdge->endIndex)) {
			addEdge(newEdge->startIndex, getNewEdge()->endIndex,
				   defaultEdge.bandwidth, defaultEdge.delay_ms,
				   defaultEdge.lossBernoulli, defaultEdge.queueLength,
					defaultEdge.recordSampledTimeline, defaultEdge.timelineSamplingPeriod,
				   startNode, node);
			routingChanged();
            emit graphChanged();
		}
		getNewEdge()->startIndex = getNewEdge()->endIndex = 0;
		getNewEdge()->start = getNewEdge()->end = NULL;
		getNewEdge()->updateVisibility();
		startNode = 0;
	} else if (editMode == InsertConnectionStart && mouseEvent->buttons() & Qt::LeftButton && node->nodeType == NETGRAPH_NODE_HOST) {
		getNewConnection()->setVisible(true);
		getNewConnection()->startIndex = getNewConnection()->endIndex = node->nodeIndex;
		getNewConnection()->setStartPoint(node->pos());
		getNewConnection()->setStartNode(node);
		startNode = node;
		editMode = InsertConnectionEnd;
		node->ungrabMouse();
		node->setSel(true);
	} else if (editMode == InsertConnectionEnd && mouseEvent->buttons() & Qt::LeftButton && node->nodeType == NETGRAPH_NODE_HOST && getNewConnection()->startIndex != node->nodeIndex) {
		getNewConnection()->endIndex = node->nodeIndex;
		editMode = InsertConnectionStart;
		startNode->setSel(false);
		node->ungrabMouse();
		addConnection(getNewConnection()->startIndex, getNewConnection()->endIndex, startNode, node);
        emit graphChanged();
		getNewConnection()->startIndex = getNewConnection()->endIndex = 0;
		getNewConnection()->start = getNewConnection()->end = NULL;
		getNewConnection()->setVisibility(false);
		startNode = 0;
	}
}

void NetGraphScene::nodeMouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node)
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	if (fastMode)
		return;
	if (editMode == MoveNode && mouseEvent->buttons() & Qt::LeftButton) {
		QPointF newMousePos = mouseEvent->scenePos();
		QPointF newPos = oldNodePos + newMousePos - oldMousePos;
		node->setPos(newPos);
		netGraph->nodes[node->nodeIndex].x = node->pos().x();
		netGraph->nodes[node->nodeIndex].y = node->pos().y();
        emit graphChanged();
	}
}

void NetGraphScene::nodeMouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node)
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	if (fastMode)
		return;
	Q_UNUSED(mouseEvent);
	Q_UNUSED(node);
	if (editMode == MoveNode) {
		QRectF newRect = itemsBoundingRect().adjusted(-100, -100, 100, 100);
		foreach (QGraphicsView *view, views()) {
			if (view) {
				QWidget *viewport = view->viewport();
				if (viewport) {
					if (viewport->width() > newRect.width()) {
						newRect.setWidth(viewport->width());
					}
					if (viewport->height() > newRect.height()) {
						newRect.setHeight(viewport->height());
					}
				}
			}
		}

		// recompute AS hulls
		netGraph->computeASHulls(netGraph->nodes[node->nodeIndex].ASNumber);
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneAS *domain = dynamic_cast<NetGraphSceneAS*>(item);
			if (domain) {
				if (domain->ASNumber == netGraph->nodes[node->nodeIndex].ASNumber) {
					foreach (NetGraphAS d, netGraph->domains) {
						if (d.ASNumber == netGraph->nodes[node->nodeIndex].ASNumber) {
							domain->setHull(d.hull);
							break;
						}
					}
					break;
				}
			}
		}

        emit graphChanged();
		//setSceneRect(newRect);
	}
}

void NetGraphScene::nodeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node)
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	if (fastMode)
		return;
	Q_UNUSED(hoverEvent);
	if (editMode == InsertEdgeEnd) {
		getNewEdge()->endIndex = node->nodeIndex;
		if (netGraph->canAddEdge(getNewEdge()->startIndex, getNewEdge()->endIndex)) {
			getNewEdge()->setEndPoint(node->pos());
			getNewEdge()->setEndNode(node);
		}
	} else if (editMode == InsertConnectionEnd && node->nodeType == NETGRAPH_NODE_HOST && getNewConnection()->startIndex != node->nodeIndex) {
		getNewConnection()->endIndex = node->nodeIndex;
		getNewConnection()->setEndPoint(node->pos());
		getNewConnection()->setEndNode(node);
	} else if (editMode == ShowRoutes) {
		if (tooltipTarget != node) {
			tooltip->setHtml(netGraph->nodes[node->nodeIndex].routeTooltip());
			tooltip->setVisible(true);
			tooltipTarget = node;
		}
		tooltip->setPos(hoverEvent->scenePos());
	}
}

void NetGraphScene::nodeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node)
{
	if (!enabled)
		return;
	if (fastMode)
		return;
	Q_UNUSED(hoverEvent);
	Q_UNUSED(node);
	if (editMode == InsertEdgeEnd) {
		getNewEdge()->endIndex = getNewEdge()->startIndex;
		getNewEdge()->setEndPoint(getNewEdge()->startPoint);
		getNewEdge()->setEndNode(NULL);
	} else if (editMode == InsertConnectionEnd && node->nodeType == NETGRAPH_NODE_HOST && getNewConnection()->startIndex != node->nodeIndex) {
		getNewConnection()->endIndex = getNewConnection()->startIndex;
		getNewConnection()->setEndPoint(getNewConnection()->startPoint);
		getNewConnection()->setEndNode(NULL);
		getNewConnection()->updateShape();
	} else if (editMode == ShowRoutes) {
		if (tooltipTarget == node) {
			tooltip->setVisible(false);
			tooltipTarget = 0;
		}
	}
}

void NetGraphScene::edgeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge)
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	if (fastMode)
		return;
	Q_UNUSED(mouseEvent);
	if (editMode == EditEdge) {
		if (selectedEdge == edge) {
			edge->setSelected(false);
			selectedEdge = 0;
            emit edgeSelected(defaultEdge);
		} else {
			if (selectedEdge)
				selectedEdge->setSelected(false);
			edge->setSelected(true);
			selectedEdge = edge;
			emit edgeSelected(netGraph->edges[selectedEdge->edgeIndex]);
		}
	} else if (editMode == DelEdge) {
		int edgeIndex = edge->edgeIndex;
		netGraph->deleteEdge(edgeIndex);
		delete edge;

		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *e = dynamic_cast<NetGraphSceneEdge*>(item);
			if (e && !e->flowEdge) {
				if (e->edgeIndex > edgeIndex) {
					e->edgeIndex--;
					e->setText(netGraph->edges[e->edgeIndex].tooltip());
				}
			}
		}

		routingChanged();
        emit graphChanged();
	}
}

void NetGraphScene::edgeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge)
{
	if (!enabled)
		return;
	if (fastMode)
		return;
	Q_UNUSED(hoverEvent);
	if (editMode == EditEdge || editMode == DelEdge) {
		edge->setHovered(true);
	}
}

void NetGraphScene::edgeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge)
{
	if (!enabled)
		return;
	if (fastMode)
		return;
	Q_UNUSED(hoverEvent);
	if (editMode == EditEdge || editMode == DelEdge) {
		edge->setHovered(false);
	}
}

void NetGraphScene::connectionMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c)
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	if (fastMode)
		return;
	Q_UNUSED(mouseEvent);
	if (editMode == DelConnection) {
		int connectionIndex = c->connectionIndex;
		netGraph->deleteConnection(connectionIndex);
		delete c;

		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneConnection *c2 = dynamic_cast<NetGraphSceneConnection*>(item);
			if (c2) {
				if (c2->connectionIndex > connectionIndex)
					c2->connectionIndex--;
			}
		}
        emit graphChanged();
	}
}

void NetGraphScene::connectionHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c)
{
	if (!enabled)
		return;
	if (fastMode)
		return;
	Q_UNUSED(hoverEvent);
	if (editMode == DelConnection) {
		c->setHovered(true);
	}
}

void NetGraphScene::connectionHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c)
{
	if (!enabled)
		return;
	if (fastMode)
		return;
	Q_UNUSED(hoverEvent);
	if (editMode == DelConnection) {
		c->setHovered(false);
	}
}

void NetGraphScene::delayChanged(int val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].delay_ms = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
        emit graphChanged();
	} else {
		defaultEdge.delay_ms = val;
	}
}

void NetGraphScene::bandwidthChanged(double val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].bandwidth = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
        emit graphChanged();
	} else {
		defaultEdge.bandwidth = val;
	}
}

void NetGraphScene::lossRateChanged(double val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].lossBernoulli = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
        emit graphChanged();
	} else {
		defaultEdge.lossBernoulli = val;
	}
}

void NetGraphScene::queueLenghtChanged(int val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].queueLength = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
        emit graphChanged();
	} else {
		defaultEdge.queueLength = val;
	}
}

void NetGraphScene::samplingPeriodChanged(quint64 val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].timelineSamplingPeriod = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
        emit graphChanged();
	} else {
		defaultEdge.timelineSamplingPeriod = val;
	}
}

void NetGraphScene::samplingChanged(bool val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].recordSampledTimeline = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
        emit graphChanged();
	} else {
		defaultEdge.recordSampledTimeline = val;
    }
}

void NetGraphScene::queueCountChanged(int val)
{
    if (enabled && editMode == EditEdge && selectedEdge) {
        //QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
        netGraph->edges[selectedEdge->edgeIndex].queueCount = val;
		netGraph->edges[selectedEdge->edgeIndex].queueWeights.resize(val);
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
		selectedEdge->color = val == 1 ? Qt::black : Qt::red;
		selectedEdge->updateColor();
		emit edgeSelected(netGraph->edges[selectedEdge->edgeIndex]);
        emit graphChanged();
    } else {
		defaultEdge.queueWeights.resize(val);
        defaultEdge.queueCount = val;
		emit edgeSelected(defaultEdge);
	}
}

void NetGraphScene::queueWeightChanged(int queue, qreal val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].queueWeights[queue] = val;
		emit edgeSelected(netGraph->edges[selectedEdge->edgeIndex]);
		emit graphChanged();
	} else {
		defaultEdge.queueWeights[queue] = val;
		emit edgeSelected(defaultEdge);
	}
}

void NetGraphScene::policerCountChanged(int val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].policerCount = val;
		netGraph->edges[selectedEdge->edgeIndex].policerWeights.resize(val);
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
		selectedEdge->color = netGraph->edges[selectedEdge->edgeIndex].getColor();
		selectedEdge->updateColor();
		emit edgeSelected(netGraph->edges[selectedEdge->edgeIndex]);
		emit graphChanged();
	} else {
		defaultEdge.policerWeights.resize(val);
		defaultEdge.policerCount = val;
		emit edgeSelected(defaultEdge);
	}
}

void NetGraphScene::policerWeightChanged(int policer, qreal val)
{
	if (enabled && editMode == EditEdge && selectedEdge) {
		//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
		netGraph->edges[selectedEdge->edgeIndex].policerWeights[policer] = val;
		emit edgeSelected(netGraph->edges[selectedEdge->edgeIndex]);
		emit graphChanged();
	} else {
		defaultEdge.policerWeights[policer] = val;
		emit edgeSelected(defaultEdge);
	}
}

void NetGraphScene::ASNumberChanged(int val)
{
    //TODO
	defaultASNumber = val;
	if (!enabled)
        return;
}

void NetGraphScene::hostTrafficFlagsChanged(bool web, bool vvoip, bool p2p, bool iptv, bool server, bool heavy)
{
    if (enabled && editMode == EditHost && selectedHost) {
        //QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
        netGraph->nodes[selectedHost->nodeIndex].web = web;
        netGraph->nodes[selectedHost->nodeIndex].vvoip = vvoip;
        netGraph->nodes[selectedHost->nodeIndex].p2p = p2p;
        netGraph->nodes[selectedHost->nodeIndex].iptv = iptv;
		netGraph->nodes[selectedHost->nodeIndex].server = server;
		netGraph->nodes[selectedHost->nodeIndex].heavy = heavy;
		selectedHost->setTrafficType(web, vvoip, p2p, iptv, server, heavy);
        emit graphChanged();
    } else {
        defaultHost.web = web;
        defaultHost.vvoip = vvoip;
        defaultHost.p2p = p2p;
        defaultHost.iptv = iptv;
        defaultHost.server = server;
		defaultHost.heavy = heavy;
    }
}

void NetGraphScene::connectionTypeChanged(QString val)
{
	connectionType = val;
	if (!enabled)
        return;
}

void NetGraphScene::setFastMode(bool valAuto, bool valMode)
{
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	fastModeAuto = valAuto;
	if (fastModeAuto) {
		fastMode = (netGraph->nodes.count() > SHOW_MAX_NODES);
	} else {
		fastMode = valMode;
	}

	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && edge->edgeIndex >= 0) {
			edge->setFastMode(fastMode);
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			n->setFastMode(fastMode);
		}

		NetGraphSceneConnection *c = dynamic_cast<NetGraphSceneConnection*>(item);
		if (c) {
			c->setFastMode(fastMode);
		}
	}
}

void NetGraphScene::reload()
{
	if (!enabled)
		return;

	destroyNewConnection();
	destroyNewEdge();
	destroyNewSelectAreaRect();

	while (!root->childItems().isEmpty()) {
		QGraphicsItem *item = root->childItems().first();
		delete item;
	}

	initTooltip();

    selectedHost = 0;
	selectedEdge = 0;
	startNode = 0;

	if (!netGraph)
		return;

	if (fastModeAuto)
		fastMode = (netGraph->nodes.count() > SHOW_MAX_NODES);

	QList<NetGraphSceneNode *> nodes;
	foreach (NetGraphNode node, netGraph->nodes) {
		nodes << addNode(node.index);
	}

	foreach (NetGraphEdge edge, netGraph->edges) {
		addEdge(edge.index, nodes[edge.source], nodes[edge.dest]);
	}

	foreach (NetGraphConnection c, netGraph->connections) {
		addConnection(c.index, nodes[c.source], nodes[c.dest]);
	}

	foreach (NetGraphAS domain, netGraph->domains) {
		if (!domain.hull.isEmpty())
			addAS(domain.index);
	}
	routingChanged();
	emit edgeSelected(defaultEdge);
	emit hostSelected(defaultHost);
}

void NetGraphScene::initTooltip()
{
	tooltip = new QGraphicsTooltip();
	addItem(tooltip);
	tooltip->setZValue(100000);
	tooltip->setVisible(false);
	tooltip->setAcceptHoverEvents(false);
	tooltip->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

void NetGraphScene::setHideConnections(bool value)
{
	hideConnections = value;
	if (!enabled)
		return;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneConnection *c = dynamic_cast<NetGraphSceneConnection*>(item);
		if (c) {
			c->setVisibility(!hideConnections);
		}
	}
}

void NetGraphScene::setHideFlows(bool value)
{
	hideFlows = value;
	if (!enabled)
		return;
	showFlows();
}

void NetGraphScene::setHideEdges(bool value)
{
	hideEdges = value;
	if (!enabled)
		return;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *e = dynamic_cast<NetGraphSceneEdge*>(item);
		if (e && !e->flowEdge) {
			e->setEdgesHidden(hideEdges);
		}
	}
}

void NetGraphScene::setHideDomains(bool value)
{
	hideDomains = value;
	if (!enabled)
		return;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneAS *d = dynamic_cast<NetGraphSceneAS*>(item);
		if (d) {
			d->setVisible(!hideDomains);
		}
	}
}

void NetGraphScene::routingChanged()
{
	if (!enabled)
		return;
	showFlows();
}

void NetGraphScene::showFlows()
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);

	QHash<int, NetGraphSceneNode*> sceneNodes;

	// remove previous flow edges
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && edge->flowEdge) {
			delete edge;
			continue;
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			sceneNodes[n->nodeIndex] = n;
		}
	}

	if (hideFlows)
		return;

	if (netGraph->paths.isEmpty())
		return;

	QHash<int, int> extraOffsets; // edge index -> offset
	bool messageRoutingShown = false;
	foreach (NetGraphConnection c, netGraph->connections) {
		bool ok;
		NetGraphPath p12 = netGraph->pathByNodeIndexTry(c.source, c.dest, ok);
		if (ok) {
			foreach (NetGraphEdge e, p12.edgeSet) {
				NetGraphSceneNode *start = sceneNodes.value(e.source, NULL);
				NetGraphSceneNode *end = sceneNodes.value(e.dest, NULL);

				if (start && end) {
					if (!extraOffsets.contains(e.index))
						extraOffsets[e.index] = 1;
                    addFlowEdge(e.index, start, end, extraOffsets[e.index],
                                c.color ? QColor::fromRgba(c.color) :
                                          NetGraphSceneConnection::getColorByIndex(c.index, c.trafficClass, c.onOff));
					extraOffsets[e.index]++;
				}
			}
		} else {
			if (!messageRoutingShown) {
				messageRoutingShown = true;
				QMessageBox::warning(0, "Could not display all flows", "Not displaying all flows. Recomputing the routes should fix the problem.", QMessageBox::Ok);
			}
		}

		NetGraphPath p21 = netGraph->pathByNodeIndexTry(c.dest, c.source, ok);
		if (ok) {
			foreach (NetGraphEdge e, p21.edgeSet) {
				NetGraphSceneNode *start = sceneNodes.value(e.source, NULL);
				NetGraphSceneNode *end = sceneNodes.value(e.dest, NULL);

				if (start && end) {
					if (!extraOffsets.contains(e.index))
						extraOffsets[e.index] = 1;
                    addFlowEdge(e.index, start, end, extraOffsets[e.index],
                                c.color ? QColor::fromRgba(c.color) :
                                          NetGraphSceneConnection::getColorByIndex(c.index, c.trafficClass, c.onOff));
					extraOffsets[e.index]++;
				}
			}
		} else {
			if (!messageRoutingShown) {
				messageRoutingShown = true;
				QMessageBox::warning(0, "Could not display all flows", "Not displaying all flows. Recomputing the routes should fix the problem.", QMessageBox::Ok);
			}
		}
	}
}

void NetGraphScene::updateColors()
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && !edge->flowEdge && edge->edgeIndex >= 0) {
			edge->color = QColor::fromRgba(netGraph->edges[edge->edgeIndex].getColor());
			edge->updateColor();
		}

		NetGraphSceneConnection *c = dynamic_cast<NetGraphSceneConnection*>(item);
		if (c && c->connectionIndex >= 0) {
			c->color = netGraph->connections[c->connectionIndex].color ?
						QColor::fromRgba(netGraph->connections[c->connectionIndex].color) :
                        NetGraphSceneConnection::getColorByIndex(c->connectionIndex, c->trafficClass, c->onOff);
			c->update();
		}
	}
}

void NetGraphScene::usedChanged()
{
	if (!enabled)
		return;
	//QMutexLockerDbg locker(netGraph->mutex, __FUNCTION__); Q_UNUSED(locker);
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && !edge->flowEdge && edge->edgeIndex >= 0) {
			edge->setUsed(netGraph->edges[edge->edgeIndex].used);
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			n->setUsed(netGraph->nodes[n->nodeIndex].used);
		}

		NetGraphSceneAS *as = dynamic_cast<NetGraphSceneAS*>(item);
		if (as) {
			as->setUsed(netGraph->domains[as->index].used);
		}
	}
}

void NetGraphScene::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	if (!enabled)
		return;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && !edge->flowEdge && edge->edgeIndex >= 0) {
			edge->setUnusedHidden(unusedHidden);
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			n->setUnusedHidden(unusedHidden);
		}

		NetGraphSceneAS *as = dynamic_cast<NetGraphSceneAS*>(item);
		if (as) {
			as->setUnusedHidden(unusedHidden);
		}
	}
}

NetGraphSceneEdge *NetGraphScene::getNewEdge()
{
	if (!newEdge) {
		newEdge = new NetGraphSceneEdge(-1, -1, -1, root);
		newEdge->setZValue(-1);
		newEdge->setPos(0, 0);
	}
	return newEdge;
}

void NetGraphScene::destroyNewEdge()
{
	delete newEdge;
	newEdge = 0;
}

NetGraphSceneConnection *NetGraphScene::getNewConnection()
{
	if (!newConnection) {
        newConnection = new NetGraphSceneConnection(-1, -1, -1, -1, false, root);
		newConnection->setZValue(-1);
		newConnection->setPos(0, 0);
	}
	return newConnection;
}

void NetGraphScene::destroyNewConnection()
{
	delete newConnection;
	newConnection = 0;
}

QGraphicsRectItem *NetGraphScene::getNewSelectAreaRect()
{
	if (!selectAreaRect) {
		selectAreaRect = new QGraphicsRectItem(QRectF(), root);
		QPen pen(Qt::darkGray, 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
		pen.setCosmetic(true);
		selectAreaRect->setPen(pen);
	}
	return selectAreaRect;
}

void NetGraphScene::destroyNewSelectAreaRect()
{
	delete selectAreaRect;
	selectAreaRect = 0;
}


