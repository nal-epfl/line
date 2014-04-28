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

#ifndef CHRONOMETER_H
#define CHRONOMETER_H

#include <QtCore>

class Chronometer
{
public:
	Chronometer(QString name = QString("Duration:"), bool silent = false);
	~Chronometer();

	quint64 elapsedMs();
	void showElapsedText(QString suffix);
	QString elapsedText(QString suffix);

    quint64 elapsedDelta();
    void showElapsedDeltaText(QString suffix);
    QString elapsedDeltaText(QString suffix);

	// resolution can be d, h, m, s, ms
	static QString durationToString(quint64 ms, QString resolution = "ms");

	static qint64 currentTimeMs();

private:
	QString name;
	quint64 tStart;
    quint64 tLast;
	bool silent;
};

qint64 currentTimeMs();

qreal getCurrentTimeSec();

quint64 getCurrentTimeNanosec();

#endif // CHRONOMETER_H
