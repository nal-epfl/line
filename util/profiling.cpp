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

#include "../util/profiling.h"

#include "graphviz.h"
#include "debug.h"

Profiling profiling;

quint64 profiling_get_current_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ((quint64)ts.tv_sec) * 1000ULL * 1000ULL * 1000ULL + ((quint64)ts.tv_nsec);
}

ProfilingTick::ProfilingTick()
{
}

ProfilingTick::ProfilingTick(quint64 time, quint64 index) :
		time(time), index(index)
{
}

ProfilingPoint::ProfilingPoint()
{
}

void ProfilingPoint::tick(quint64 index)
{
	ticks.append(ProfilingTick(profiling_get_current_time(), index));
}

Profiling::Profiling()
{
}

void Profiling::touchPoint(QString function, int line)
{
	points[function][line].tick(nextIndex++);
	//graySilent(QString::number(clock() / double(CLOCKS_PER_SEC)) + " " + function + ":" + QString::number(line));
}

bool profileTickLessThan(const QPair<ProfilingTick, QString> &first, const QPair<ProfilingTick, QString> &second)
{
	return first.first.index <= second.first.index;
}

void Profiling::printSummary()
{
	QHash<QString, QHash<QString, QVariant> > nodes;
	QHash<QPair<QString, QString>, QHash<QString, QVariant> > edges;

	QList<QPair<ProfilingTick, QString> > path;

	foreach (QString f, points.uniqueKeys()) {
		foreach (int line, points[f].uniqueKeys()) {
			QString n = f + ":" + QString::number(line);
			nodes[n]["weight"] = nodes[n]["weight"].toInt() + 1;
			nodes[n]["label"] = n;
			nodes[n]["color"] = QString::number((qHash(f) % 200) / (double)200) + ", 1.0, 1.0";
			foreach (ProfilingTick t, points[f][line].ticks) {
				path.append(QPair<ProfilingTick, QString>(t, n));
			}
		}
	}

	qSort(path.begin(), path.end(), profileTickLessThan);

	for (int i = 0; i < path.count() - 1; i++) {
		quint64 t1 = path[i].first.time;
		quint64 t2 = path[i+1].first.time;
		QPair<QString, QString> e = QPair<QString, QString>(path[i].second, path[i+1].second);
		edges[e]["weight"] = edges[e]["weight"].toDouble() + (t2 - t1) * 1.0e-9;
	}

	foreach (QString n, nodes.uniqueKeys()) {
		nodes[n]["penwidth"] = 1 + log2(nodes[n]["weight"].toDouble());
	}

	foreach (Edge e, edges.uniqueKeys()) {
		edges[e]["tooltip"] = QString::number(edges[e]["weight"].toDouble());
		if (edges[e]["weight"].toDouble() == 0)
			edges[e]["penwidth"] = 1;
		else
			edges[e]["penwidth"] = 1 + log2(edges[e]["weight"].toDouble());
	}

	generateGraphViz(nodes, edges, "profile");
}
