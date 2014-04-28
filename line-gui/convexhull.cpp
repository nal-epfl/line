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

#include "convexhull.h"

// Returns -, 0, + if a,b,c form a right, straight, or left turn
double turnValue(QPointF a, QPointF b, QPointF c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
}

double distance(QPointF a, QPointF b)
{
	return sqrt((a.x() - b.x()) * (a.x() - b.x()) + (a.y() - b.y()) * (a.y() - b.y()));
}

QList<QPointF> ConvexHull::giftWrap(QList<QPointF> points)
{
	if (points.count() <= 3)
		return points;

	// Step 1: pick start point (leftmost)
	int startIndex = 0;
	for (int i = 0; i < points.count(); i++) {
		if (points[i].x() < points[startIndex].x()) {
			startIndex = i;
		}
	}
	points.swap(0, startIndex);

	QList<int> hullIndices;
	int currentPoint = 0;

//	qDebug() << points;

	for (;;) {
		hullIndices << currentPoint;
//		qDebug() << points[currentPoint];
		int endPoint = 0;
//		qDebug() << currentPoint << endPoint;
		for (int i = 1; i < points.count(); i++) {
			// if points[i] is on left of line from currentPoint to endpoint
			double turn = turnValue(points[currentPoint], points[i], points[endPoint]);
			if (turn > 0) {
				endPoint = i;
			} else if (turn == 0 && distance(points[currentPoint], points[endPoint]) < distance(points[currentPoint], points[i])) {
				endPoint = i;
			}
//			qDebug() << currentPoint << endPoint;
		}
		currentPoint = endPoint;
		if (currentPoint == 0)
			break;
	}

	QList<QPointF> hull;
	foreach (int i, hullIndices)
		hull << points[i];

	return hull;
}

void ConvexHull::testHull()
{
	qDebug() << giftWrap(QList<QPointF>() << QPointF(0.5, 0.5) << QPointF(0, 0) << QPointF(1, 0) << QPointF(1, 1) << QPointF(0, 1));
}
