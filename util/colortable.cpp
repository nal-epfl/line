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

#include "colortable.h"

ColorTable::ColorTable(int hueCount, int satCount, int valCount,
		   qreal hueMin, qreal hueMax,
		   qreal satMin, qreal satMax,
		   qreal valMin, qreal valMax)
{
    bool wrapHue = false;
    if (hueMin > hueMax) {
        hueMax += 1.0;
        wrapHue = true;
    }

	QList<QColor> linear;
	for (int h = 0; h < hueCount; h++) {
		qreal hue = hueMin + qreal(h)/qreal(hueCount) * (hueMax - hueMin);
        if (wrapHue) {
            if (hue > 1.0)
                hue -= 1.0;
        }
		for (int s = 0; s < satCount; s++) {
			qreal sat = satCount == 1 ? 1.0 : satMax - qreal(s)/qreal(satCount-1) * (satMax - satMin);
			for (int v = 0; v < valCount; v++) {
				qreal val = valCount == 1 ? 1.0 : valMax - qreal(v)/qreal(valCount-1) * (valMax - valMin);
				linear << QColor::fromHsvF(hue, sat, val, 1.0);
			}
		}
	}

	QList<bool> picked;
	for (int i = 0; i < linear.count(); i++) {
		picked << false;
	}

	colors.clear();

	int i = 0;
	for (qreal step = linear.count() / 4.0; colors.count() < linear.count(); step /= 2.0) {
		while (picked[i]) {
			i = (i + 1) % linear.count();
		}
		colors << linear[i];
		picked[i] = true;

		i = (i + linear.count()/2) % linear.count();
		if (!picked[i]) {
			colors << linear[i];
			picked[i] = true;
		}
		i = (i + linear.count()/2) % linear.count();

		int istep = qMax(1, qRound(step));
		i = (i + istep) % linear.count();
	}
}

QColor ColorTable::getColor(int index)
{
	if (colors.isEmpty()) {
		return Qt::blue;
	}
	index = abs(index);
	return colors[index % colors.count()];
}

static ColorTable *default24 = NULL;
ColorTable &ColorTable::getDefault24()
{
	if (default24)
		return *default24;
	default24 = new ColorTable(6, 2, 2);
    return *default24;
}

static ColorTable *cool24 = NULL;
ColorTable &ColorTable::getCool24()
{
    if (cool24)
        return *cool24;
    cool24 = new ColorTable(6, 2, 2, 120.0 / 360.0, 240.0 / 360.0, 1.0, 1.0, 0.75, 1.0);
    return *cool24;
}

static ColorTable *warm24 = NULL;
ColorTable &ColorTable::getWarm24()
{
    if (warm24)
        return *warm24;
    warm24 = new ColorTable(6, 2, 2, 300.0 / 360.0, 60.0 / 360.0, 1.0, 1.0, 0.75, 1.0);
    return *warm24;
}
