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

#ifndef QGRAPHICSVIEWGOOD_H
#define QGRAPHICSVIEWGOOD_H

#include <QtGui>

class NetGraphScene;

class QGraphicsViewGood : public QWidget
{
	Q_OBJECT

public:
	explicit QGraphicsViewGood(QWidget *parent = 0);

	void setScene(NetGraphScene *newScene);
	void setTransform(QTransform t);
	QTransform getTransform() { return transform; }
	QWidget *viewport() {return this;}

	void updateTransform();

protected:
	NetGraphScene *scene;
	QTransform transform;
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *);

	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void mouseDoubleClickEvent(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void wheelEvent(QWheelEvent *e);

	QGraphicsSceneMouseEvent *mouseEvent2Scene(QMouseEvent *event);

protected slots:
	void updateScene(QList<QRectF>);
};

#endif // QGRAPHICSVIEWGOOD_H
