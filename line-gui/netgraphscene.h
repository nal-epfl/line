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

#ifndef NETGRAPHSCENE_H
#define NETGRAPHSCENE_H

#include <QGraphicsScene>

#include "netgraph.h"
#include "netgraphscenenode.h"
#include "netgraphsceneedge.h"
#include "netgraphsceneas.h"
#include "netgraphsceneconnection.h"

#define SHOW_MAX_NODES 100

class NetGraphScene : public QGraphicsScene
{
	Q_OBJECT
public:
	enum EditMode { InsertHost,
                    EditHost,
                    InsertGateway,
                    InsertRouter,
                    InsertBorderRouter,
                    InsertEdgeStart,
                    InsertEdgeEnd,
                    InsertConnectionStart,
                    InsertConnectionEnd,
                    MoveNode,
                    MoveNodeArea,
                    EditEdge,
                    DelNode,
                    DelNodeArea,
                    DelEdge,
                    DelConnection,
                    Drag,
                    ShowRoutes };

	explicit NetGraphScene(QObject *parent = 0);
	QSharedPointer<NetGraph> netGraph;

	// don't delete this pointer or you will burn in hell
	NetGraph *graph();

	QGraphicsItem *root;

	EditMode editMode;
	bool enabled;

	// for dragging
	QPointF oldMousePos;
	QPointF oldNodePos;
	QPointF oldViewportCenter;
	QRectF oldRect;
	bool mouseDown;
	bool dragging;

	// for adding edges
	NetGraphSceneEdge *newEdge;
	NetGraphEdge defaultEdge;

	int defaultASNumber;
	QString connectionType;

	// the start node for a new edge/connection
	NetGraphSceneNode *startNode;

	NetGraphSceneConnection *newConnection;

    // for editing nodes
    NetGraphSceneNode *selectedHost;
    NetGraphNode defaultHost;

	// for editing edges
	NetGraphSceneEdge *selectedEdge;

	QGraphicsRectItem *selectAreaRect;

	// for large graphs, switch to read only
	bool fastMode;
	bool fastModeAuto;
	bool unusedHidden;
	bool hideConnections;
	bool hideFlows;
	bool hideEdges;
	bool hideDomains;

	QPixmap pixmapHost;
	QString svgHostPath;
	QPixmap pixmapRouter;
	QString svgRouterPath;
	QPixmap pixmapGateway;
	QString svgGatewayPath;
	QPixmap pixmapBorderRouter;
	QString svgBorderRouterPath;

    QPixmap pixmapBlueHost;
    QString svgBlueHostPath;
    QPixmap pixmapGreenHost;
    QString svgGreenHostPath;
    QPixmap pixmapYellowHost;
    QString svgYellowHostPath;

	QGraphicsTextItem *tooltip;

signals:
    void hostSelected(NetGraphNode node);
	void edgeSelected(NetGraphEdge edge);
	void wheelEventFired(QGraphicsSceneWheelEvent *event);
	void viewportChanged();
    void graphChanged();

public slots:
	void setMode(EditMode mode);
	void setEnabled(bool val);

	// edge properties (selected/default)
	void delayChanged(int val);
	void bandwidthChanged(double val);
	void lossRateChanged(double val);
	void queueLenghtChanged(int val);
	void samplingPeriodChanged(quint64 val);
	void samplingChanged(bool val);
    void queueCountChanged(int val);
	void queueWeightChanged(int queue, qreal val);
	void policerCountChanged(int val);
	void policerWeightChanged(int policer, qreal val);

	// node properties
	void ASNumberChanged(int val);
	void hostTrafficFlagsChanged(bool web, bool vvoip, bool p2p, bool iptv, bool server, bool heavy);

	// connection properties
	void connectionTypeChanged(QString val);

	// graph events
	void reload();
	void usedChanged();
	void setUnusedHidden(bool unusedHidden);
	void setHideConnections(bool value);
	void setHideFlows(bool value);
	void setHideEdges(bool value);
	void setHideDomains(bool value);
	void setFastMode(bool valAuto, bool valMode = true);
	void routingChanged();
	void showFlows();
	void updateColors();

	void initTooltip();

	// new edge
	NetGraphSceneEdge *getNewEdge();
	void destroyNewEdge();

	// new connection
	NetGraphSceneConnection *getNewConnection();
	void destroyNewConnection();

	QGraphicsRectItem *getNewSelectAreaRect();
	void destroyNewSelectAreaRect();

	// node mouse events
	void nodeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void nodeMouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void nodeMouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void nodeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);
	void nodeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);

	// edge mouse events
	void edgeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge);
	void edgeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);
	void edgeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);

	// connection mouse events
	void connectionMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c);
	void connectionHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);
	void connectionHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);

protected:
	void *tooltipTarget;

	void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void wheelEvent(QGraphicsSceneWheelEvent *wheelEvent);

	NetGraphSceneNode *addNode(int type, QPointF pos,
							   bool web = false, bool vvoip = false, bool p2p = false,
							   bool iptv = false, bool server = false, bool heavy = false);
	NetGraphSceneNode *addNode(int index);

	NetGraphSceneEdge *addEdge(int startIndex, int endIndex, double bandwidth, int delay, double loss, int queueLength,
							   bool recordSampledTimeline, quint64 timelineSamplingPeriod,
						  NetGraphSceneNode *start, NetGraphSceneNode *end);
	NetGraphSceneEdge *addEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end);

	NetGraphSceneEdge *addFlowEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end, int extraOffset, QColor color);

	NetGraphSceneAS *addAS(int index);

	NetGraphSceneConnection *addConnection(int startIndex, int endIndex,
							NetGraphSceneNode *start, NetGraphSceneNode *end);
	NetGraphSceneConnection *addConnection(int index, NetGraphSceneNode *start, NetGraphSceneNode *end);

	friend class QGraphicsViewGood;
};

#endif // NETGRAPHSCENE_H
