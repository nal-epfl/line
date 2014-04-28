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

#ifndef GEPHILAYOUT_H
#define GEPHILAYOUT_H

#include <QtCore>

/*
nodes = node positions
edges = pairs of nodes, each node identified by an int index starting from 0
config = gephi config, like: Random,OpenOrd,YifanHu:10,ForceAtlas:3,Center
*/
bool runLayout(QList<QPointF> &nodes, QList<qreal> nodeSizes, QList<QPair<int, int> > edges, QString config);

void rescale(QList<QPointF> &nodes, QList<QPair<int, int> > edges, const qreal lowerDistLimit = 50.0);

bool importGexfPositions(QList<QPointF> &nodes, QString fileName);

bool exportGexf(QList<QPointF> &nodes, QList<qreal> nodeSizes, QList<QPair<int, int> > edges, QString fileName);

#endif // GEPHILAYOUT_H
