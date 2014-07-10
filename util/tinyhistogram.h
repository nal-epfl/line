/*
 *	Copyright (C) 2014 Ovidiu Mara
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

#ifndef DELAYHISTOGRAM_H
#define DELAYHISTOGRAM_H

#include <QtCore>

class TinyHistogram
{
public:
	TinyHistogram(int numBins = 16);

	void recordEvent(quint64 value);

	QString toString(QString (*valuePrinter)(quint64) = NULL);

protected:
	QVector<quint32> bins;
	quint64 min;
	quint64 max;
	quint64 sum;
};

QString time2String(quint64 nanoseconds);
QString intWithCommas2String(quint64 value);

#endif // DELAYHISTOGRAM_H
