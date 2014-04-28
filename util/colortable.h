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

#ifndef COLORTABLE_H
#define COLORTABLE_H

#include <QtGui>

class ColorTable
{
public:
    // Hue, saturation and value must be in 0-1.0.
    // The hue wraps around (hueMin > hueMax is allowed).
	ColorTable(int hueCount, int satCount, int valueCount,
			   qreal hueMin = 0.0, qreal hueMax = 1.0,
			   qreal satMin = 0.5, qreal satMax = 1.0,
			   qreal valMin = 0.5, qreal valMax = 1.0);

	QList<QColor> colors;

	QColor getColor(int index);

	static ColorTable &getDefault24();

    static ColorTable &getCool24();

    static ColorTable &getWarm24();
};

#endif // COLORTABLE_H
