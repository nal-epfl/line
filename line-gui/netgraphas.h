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

#ifndef NETGRAPHAS_H
#define NETGRAPHAS_H

#include <QtCore>

#include "netgraphcommon.h"

class NetGraphAS
{
public:
	NetGraphAS(int index = 0, int ASNumber = 0, QList<QPointF> hull = QList<QPointF>()) : index(index), ASNumber(ASNumber), hull(hull)
	{
		used = true;
	}

	qint32 index;
	qint32 ASNumber;
	bool used;
	QList<QPointF> hull;
};

QDataStream& operator>>(QDataStream& s, NetGraphAS& as);

QDataStream& operator<<(QDataStream& s, const NetGraphAS& as);

#endif // NETGRAPHAS_H
