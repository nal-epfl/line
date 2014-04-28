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

#include "chronometer.h"

#include <sys/time.h>
#include <time.h>

qint64 Chronometer::currentTimeMs()
{
#if QT_VERSION >= 0x040700
    return QDateTime::currentMSecsSinceEpoch();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	return ((qint64)ts.tv_sec) * 1000LL + ((qint64)ts.tv_nsec) / 1000LL / 1000LL;
#endif
}

Chronometer::Chronometer(QString name, bool silent) :
	name(name), silent(silent)
{
	tStart = Chronometer::currentTimeMs();
    tLast = tStart;
	if (!silent)
		qDebug() << QString("Timing %1").arg(name);
}

Chronometer::~Chronometer()
{
	showElapsedText("finish");
}

quint64 Chronometer::elapsedMs()
{
	quint64 now = Chronometer::currentTimeMs();
    tLast = now;
    return now - tStart;
}

void Chronometer::showElapsedText(QString suffix)
{
	if (!silent)
		qDebug() << elapsedText(suffix);
}

QString Chronometer::elapsedText(QString suffix)
{
    QString msg = name + " took " + durationToString(elapsedMs()) + QString(" to %1").arg(suffix);
    return msg;
}

quint64 Chronometer::elapsedDelta()
{
	quint64 now = Chronometer::currentTimeMs();
    quint64 delta = now - tLast;
    tLast = now;
    return delta;
}

void Chronometer::showElapsedDeltaText(QString suffix)
{
    if (!silent)
        qDebug() << elapsedDeltaText(suffix);
}

QString Chronometer::elapsedDeltaText(QString suffix)
{
    QString msg = name + " took " + durationToString(elapsedDelta()) + QString(" to %1").arg(suffix);
    return msg;
}

QString Chronometer::durationToString(quint64 ms, QString resolution)
{
    quint64 seconds = 0;
    quint64 minutes = 0;
    quint64 hours = 0;
    quint64 days = 0;

    if (ms >= 1000ULL) {
        seconds = ms / 1000ULL;
        ms = ms % 1000ULL;
    }

    if (seconds >= 60ULL) {
        minutes = seconds / 60ULL;
        seconds = seconds % 60ULL;
    }

    if (minutes >= 60ULL) {
        hours = minutes / 60ULL;
        minutes = minutes % 60ULL;
    }

    if (hours >= 24ULL) {
        days = hours / 24ULL;
        hours = hours % 24ULL;
    }

    QString msg;
    if (days > 0) {
        if (resolution == "ms") {
            msg += QString("%1 days, %2 hours, %3 minutes, %4 seconds and %5 ms")
                    .arg(days).arg(hours).arg(minutes).arg(seconds).arg(ms);
        } else if (resolution == "s") {
            msg += QString("%1 days, %2 hours, %3 minutes and %4 seconds")
                    .arg(days).arg(hours).arg(minutes).arg(seconds);
        } else if (resolution == "m") {
            msg += QString("%1 days, %2 hours and %3 minutes")
                    .arg(days).arg(hours).arg(minutes);
        } else if (resolution == "h") {
            msg += QString("%1 days and %2 hours")
                    .arg(days).arg(hours);
        } else {
            msg += QString("%1 days")
                    .arg(days);
        }
    } else if (hours > 0) {
        if (resolution == "ms") {
            msg += QString("%2 hours, %3 minutes, %4 seconds and %5 ms")
                    .arg(hours).arg(minutes).arg(seconds).arg(ms);
        } else if (resolution == "s") {
            msg += QString("%2 hours, %3 minutes and %4 seconds")
                    .arg(hours).arg(minutes).arg(seconds);
        } else if (resolution == "m") {
            msg += QString("%2 hours and %3 minutes")
                    .arg(hours).arg(minutes);
        } else {
            msg += QString("%2 hours")
                    .arg(hours);
        }
    } else if (minutes > 0) {
        if (resolution == "ms") {
            msg += QString("%3 minutes, %4 seconds and %5 ms")
                    .arg(minutes).arg(seconds).arg(ms);
        } else if (resolution == "s") {
            msg += QString("%3 minutes and %4 seconds")
                    .arg(minutes).arg(seconds);
        } else {
            msg += QString("%3 minutes and %4 seconds")
                    .arg(minutes).arg(seconds);
        }
    } else if (seconds > 0) {
        if (resolution == "ms") {
            msg += QString("%4 seconds and %5 ms")
                    .arg(seconds).arg(ms);
        } else {
            msg += QString("%4 seconds")
                    .arg(seconds);
        }
    } else {
        msg += QString("%5 ms").arg(ms);
    }
    return msg;
}

qint64 currentTimeMs() {
	return (getCurrentTimeNanosec() / 1000000ULL) % 1000LL;
}

qreal getCurrentTimeSec() {
	struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

	return ts.tv_sec + ts.tv_nsec * 1.0e-9;
}

quint64 getCurrentTimeNanosec() {
	struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

	return ts.tv_sec * 1000ULL * 1000ULL * 1000ULL + ts.tv_nsec;
}
