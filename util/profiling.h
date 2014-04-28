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

#ifndef PROFILING_H
#define PROFILING_H

#include <time.h>

#include <QtCore>

/*
#include <execinfo.h>
#include <stdlib.h>
void
	print_trace (void)
	{
	  void *array[10];
	  size_t size;
	  char **strings;
	  size_t i;

	  size = backtrace (array, 10);
	  strings = backtrace_symbols (array, size);

	  printf ("Obtained %zd stack frames.\n", size);

	  for (i = 0; i < size; i++)
		printf ("%s\n", strings[i]);

	  free (strings);
	}
*/

class ProfilingTick
{
public:
	ProfilingTick();
	ProfilingTick(quint64 time, quint64 index);
	quint64 time;
	quint64 index;
};

class ProfilingPoint
{
public:
	ProfilingPoint();
	void tick(quint64 index);
	QList<ProfilingTick> ticks;
};

class Profiling
{
public:
	Profiling();

	void touchPoint(QString function, int line);
	void printSummary();

	QHash<QString, QHash<int, ProfilingPoint> > points; // function name -> list of (line number -> point)
	unsigned long long nextIndex;
};

extern Profiling profiling;

//#define Q_MY_PROF_INIT Profiling profiling
#define Q_MY_PROF_TICK profiling.touchPoint(__FUNCTION__, __LINE__)
#define Q_MY_PROF_SUMMARY profiling.printSummary()
#define Q_MY_PROF_RESET profiling.points.clear()

#endif // PROFILING_H
