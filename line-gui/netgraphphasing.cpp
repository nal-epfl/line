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

#include "netgraphphasing.h"

NetGraphPhase::NetGraphPhase(QSharedPointer<NetGraph> netGraph, QString title, QString text)
	: netGraph(netGraph), title(title), text(text)
{
}

NetGraphPhase NetGraphPhase::createSimplePhase(NetGraph *graph, QString title, QString text,
											   QList<QRgb> edgeColors, QList<QRgb> connectionColors,
											   QList<QString> edgeLabels)
{
	NetGraph *netGraph = new NetGraph();
	*netGraph = *graph;

	if (!edgeColors.isEmpty()) {
		Q_ASSERT_FORCE(edgeColors.count() == netGraph->edges.count());
		for (int i = 0; i < netGraph->edges.count(); i++) {
			netGraph->edges[i].color = edgeColors[i];
		}
	}

	if (!edgeLabels.isEmpty()) {
		Q_ASSERT_FORCE(edgeLabels.count() == netGraph->edges.count());
		for (int i = 0; i < netGraph->edges.count(); i++) {
			netGraph->edges[i].extraTooltip = edgeLabels[i];
		}
	}

	if (!connectionColors.isEmpty()) {
		Q_ASSERT_FORCE(connectionColors.count() == netGraph->connections.count());
		for (int i = 0; i < netGraph->connections.count(); i++) {
			netGraph->connections[i].color = connectionColors[i];
		}
	}

	return NetGraphPhase(QSharedPointer<NetGraph>(netGraph), title, text);
}
