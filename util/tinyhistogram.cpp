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

#include "tinyhistogram.h"

// essentialy the integer base 2 logarithm
static inline quint64 bit_scan_reverse_asm64(quint64 v)
{
	if (v == 0)
		return 0;
	quint64 r;
	asm volatile("bsrq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

TinyHistogram::TinyHistogram(int numBins)
{
	bins.resize(numBins);
	max = 0ULL;
	min = 0xffFFffFFffFFffFFULL;
	sum = 0ULL;
}

void TinyHistogram::recordEvent(quint64 value)
{
	quint64 logarithm = bit_scan_reverse_asm64(value);
	if (logarithm >= quint64(bins.count())) {
		logarithm = bins.count() - 1;
	}
	bins[logarithm]++;
	min = qMin(min, value);
	max = qMax(max, value);
	sum += value;
}

QString TinyHistogram::toString(QString (*valuePrinter)(quint64))
{
	quint64 totalCount = 0;
	foreach (quint32 counter, bins) {
		totalCount += counter;
	}

	if (totalCount == 0) {
		return QString("No data.\n");
	}

	QString result;
	result += QString("Min %1, Max %2, Average %3:\n")
			  .arg(valuePrinter ? valuePrinter(min) : QString("%1").arg(min))
			  .arg(valuePrinter ? valuePrinter(max) : QString("%1").arg(max))
			  .arg(valuePrinter ? valuePrinter(sum / totalCount) : QString("%1").arg(sum / totalCount));

	result += "Histogram:\n";
	quint64 cumulativeCount = 0;
	for (int i = 0; i < bins.count(); i++) {
		if (bins[i] == 0)
			continue;
		cumulativeCount += bins[i];
		QString value1 = valuePrinter ? valuePrinter(1ULL << (i + 0)) : QString("%1").arg(1ULL << (i + 0));
		QString value2 = valuePrinter ? valuePrinter(1ULL << (i + 1)) : QString("%1").arg(1ULL << (i + 1));
		if (i == bins.count() - 1) {
			value2 = "infinity";
		}
		result += QString("%1 to %2: %3 (%4%, cumulative %5%)\n")
				  .arg(value1)
				  .arg(value2)
				  .arg(intWithCommas2String(bins[i]))
				  .arg(bins[i] * 100.0 / totalCount, 0, 'f', 2)
				  .arg(cumulativeCount * 100.0 / totalCount, 0, 'f', 2);
	}
	return result;
}

QString intWithCommas2String(quint64 value)
{
	return QLocale(QLocale::English).toString(value);
}

QString time2String(quint64 nanoseconds)
{
	quint64 microseconds = 0;
	quint64 ms = 0;
	quint64 seconds = 0;
	quint64 minutes = 0;
	quint64 hours = 0;
	quint64 days = 0;

	if (nanoseconds >= 1000ULL) {
		microseconds = nanoseconds / 1000ULL;
		nanoseconds = nanoseconds % 1000ULL;
	}

	if (microseconds >= 1000ULL) {
		ms = microseconds / 1000ULL;
		microseconds = microseconds % 1000ULL;
	}

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

	if (days > 0) {
		return QString("%1 d %2 h %3 m %4 s %5 ms %6 us %7 ns")
				.arg(days)
				.arg(hours)
				.arg(minutes)
				.arg(seconds)
				.arg(ms)
				.arg(microseconds)
				.arg(nanoseconds);
	} else if (hours > 0) {
		return QString("%2 h %3 m %4 s %5 ms %6 us %7 ns")
				.arg(hours)
				.arg(minutes)
				.arg(seconds)
				.arg(ms)
				.arg(microseconds)
				.arg(nanoseconds);
	} else if (minutes > 0) {
		return QString("%3 m %4 s %5 ms %6 us %7 ns")
				.arg(minutes)
				.arg(seconds)
				.arg(ms)
				.arg(microseconds)
				.arg(nanoseconds);
	} else if (seconds > 0) {
		return QString("%4 s %5 ms %6 us %7 ns")
				.arg(seconds)
				.arg(ms)
				.arg(microseconds)
				.arg(nanoseconds);
	} else if (ms > 0) {
		return QString("%5 ms %6 us %7 ns")
				.arg(ms)
				.arg(microseconds)
				.arg(nanoseconds);
	} else if (microseconds > 0) {
		return QString("%6 us %7 ns")
				.arg(microseconds)
				.arg(nanoseconds);
	} else {
		return QString("%7 ns")
				.arg(nanoseconds);
	}
}
