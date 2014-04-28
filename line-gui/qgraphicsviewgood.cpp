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

#include "qgraphicsviewgood.h"

#include "netgraphscene.h"

QGraphicsViewGood::QGraphicsViewGood(QWidget *parent) : QWidget(parent)
{
	scene = 0;
}

void QGraphicsViewGood::setScene(NetGraphScene *newScene)
{
	scene = newScene;
	connect(scene, SIGNAL(changed(QList<QRectF>)), this, SLOT(updateScene(QList<QRectF>)));
	update();
}

void QGraphicsViewGood::setTransform(QTransform t)
{
	transform = t;
	update();
}

void QGraphicsViewGood::updateTransform()
{
	qreal w_screen, h_screen;
	w_screen = width();
	h_screen = height();

	QTransform t;
	t.scale(1/scene->graph()->viewportZoom, 1/scene->graph()->viewportZoom);
	t.translate(-w_screen/2 + scene->graph()->viewportCenter.x() * scene->graph()->viewportZoom, -h_screen/2 + scene->graph()->viewportCenter.y() * scene->graph()->viewportZoom);
	setTransform(t);
}

void QGraphicsViewGood::paintEvent(QPaintEvent *event)
{
	if (!scene) {
		QWidget::paintEvent(event);
		return;
	}

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

	QRectF viewRect(0, 0, width(), height());

	qreal w_screen, h_screen;
	w_screen = width();
	h_screen = height();

	qreal w_world, h_world;
	w_world = w_screen / scene->graph()->viewportZoom;
	h_world = h_screen / scene->graph()->viewportZoom;

	qreal left, top;
	left = scene->graph()->viewportCenter.x() - w_world/2;
	top = scene->graph()->viewportCenter.y() - h_world/2;

	QRectF sceneRect (left, top, w_world, h_world);

	painter.fillRect(viewRect.toRect(), Qt::white);

	scene->render(&painter, viewRect, sceneRect, Qt::IgnoreAspectRatio);
}

void QGraphicsViewGood::resizeEvent(QResizeEvent *)
{
	updateTransform();
}

QGraphicsSceneMouseEvent *QGraphicsViewGood::mouseEvent2Scene(QMouseEvent *event)
{
	QGraphicsSceneMouseEvent *mouseEvent = new QGraphicsSceneMouseEvent(
				event->type() == QEvent::MouseButtonPress ? QEvent::GraphicsSceneMousePress :
				event->type() == QEvent::MouseMove ? QEvent::GraphicsSceneMouseMove :
				event->type() == QEvent::MouseButtonRelease ? QEvent::GraphicsSceneMouseRelease :
				event->type() == QEvent::MouseButtonDblClick ? QEvent::GraphicsSceneMouseDoubleClick :
															   QEvent::GraphicsSceneMouseMove);
	mouseEvent->setWidget(this);
	mouseEvent->setButtonDownScenePos(event->button(), transform.map(event->posF()));
	mouseEvent->setButtonDownScreenPos(event->button(), event->globalPos());
	mouseEvent->setScenePos(transform.map(event->posF()));
	mouseEvent->setScreenPos(event->globalPos());
	mouseEvent->setLastScenePos(transform.map(event->posF()));
	mouseEvent->setLastScreenPos(event->globalPos());
	mouseEvent->setButtons(event->buttons());
	mouseEvent->setButton(event->button());
	mouseEvent->setModifiers(event->modifiers());
	mouseEvent->setAccepted(false);
	return mouseEvent;
}

void QGraphicsViewGood::updateScene(QList<QRectF>)
{
	update();
}

void QGraphicsViewGood::mousePressEvent(QMouseEvent *e)
{
	QGraphicsSceneMouseEvent *me = mouseEvent2Scene(e);
	QApplication::sendEvent(scene, me);
	delete me;
}

void QGraphicsViewGood::mouseReleaseEvent(QMouseEvent *e)
{
	QGraphicsSceneMouseEvent *me = mouseEvent2Scene(e);
	QApplication::sendEvent(scene, me);
	delete me;
}

void QGraphicsViewGood::mouseDoubleClickEvent(QMouseEvent *e)
{
	QGraphicsSceneMouseEvent *me = mouseEvent2Scene(e);
	QApplication::sendEvent(scene, me);
	delete me;
}

void QGraphicsViewGood::mouseMoveEvent(QMouseEvent *e)
{
	QGraphicsSceneMouseEvent *me = mouseEvent2Scene(e);
	QApplication::sendEvent(scene, me);
	delete me;
}

void QGraphicsViewGood::wheelEvent(QWheelEvent *event)
{
	QGraphicsSceneWheelEvent wheelEvent(QEvent::GraphicsSceneWheel);
	wheelEvent.setWidget(viewport());
	wheelEvent.setScenePos(transform.map(event->pos()));
	wheelEvent.setScreenPos(event->globalPos());
	wheelEvent.setButtons(event->buttons());
	wheelEvent.setModifiers(event->modifiers());
	wheelEvent.setDelta(event->delta());
	wheelEvent.setOrientation(event->orientation());
	wheelEvent.setAccepted(false);
	scene->wheelEvent(&wheelEvent);
}
