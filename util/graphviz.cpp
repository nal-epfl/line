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

#include "graphviz.h"
#include "debug.h"
#include "util.h"

QString graphVizName(QString ip)
{
	QString result = "n" + ip;
	result.replace(".", "_");
	result.replace("-", "_");
	result.replace(":", "_");
	return result;
}

QString graphVizName(QPair<QString, QString> e)
{
	return graphVizName(e.first) + " -> " + graphVizName(e.second);
}

void generateGraphViz(QHash<QString, QHash<QString, QVariant> > &nodes, QHash<QPair<QString, QString>, QHash<QString, QVariant> > &edges, QString name, QString engine)
{
	fdbg << "Generating graphviz input file...";
	QString g = "digraph G {\n";
	g.append("nodesep = 1.0; overlap=\"prism\"; outputorder=\"edgesfirst\";\n");
	{
		foreach (QString ip, nodes.uniqueKeys()) {
			QString label = ip;
			QString attr;
			if (nodes[ip].contains("color"))
				attr += ", style=filled, fillcolor = \"" + nodes[ip]["color"].toString() + "\"";
			if (nodes[ip].contains("pencolor"))
				attr += ", color = \"" + nodes[ip]["pencolor"].toString() + "\"";
			if (nodes[ip].contains("label"))
				label = nodes[ip]["label"].toString();
			if (nodes[ip].contains("tooltip")) {
				QString tooltip = nodes[ip]["tooltip"].toString();
				if (tooltip.length() > 512) {
					tooltip = tooltip.mid(0, 512) + "...";
				}
				makePrintable(tooltip);
				tooltip.replace('"', ' ');
				attr += ", tooltip = \"" + tooltip + "\"";
			}
			if (nodes[ip].contains("penwidth"))
				attr += ", penwidth = " + nodes[ip]["penwidth"].toString() + "";
			g.append(graphVizName(ip) + " [label = \"" + label + "\", fontname = \"Arial\", fontsize = \"10\"" + attr + "];\n");
		}
	}
	{
		foreach (Edge e, edges.uniqueKeys()) {
			QString ip1 = e.first;
			QString ip2 = e.second;
			QString attr;
			if (edges[e].contains("color"))
				attr += ", style=filled, fillcolor = \"" + edges[e]["color"].toString() + "\"";
			if (edges[e].contains("pencolor"))
				attr += ", color = \"" + edges[e]["pencolor"].toString() + "\"";
			if (edges[e].contains("tooltip")) {
				QString tooltip = edges[e]["tooltip"].toString();
				if (tooltip.length() > 512) {
					tooltip = tooltip.mid(0, 512) + "...";
				}
				makePrintable(tooltip);
				tooltip.replace('"', ' ');
				attr += ", tooltip = \"" + tooltip + "\"";
			}
			if (edges[e].contains("penwidth"))
				attr += ", penwidth = " + edges[e]["penwidth"].toString() + "";
			g.append(graphVizName(e) + " [label = \"\", arrowhead = vee, dir = forward " + attr + "];\n");
		}
	}
	g.append("}");

	fdbg << "Running graphviz: " + QString(name + ".dot");
	QTime t;
	t.start();
	saveFile(QString(name + ".dot"), g);
	QProcess graphviz;
	graphviz.start(engine, QStringList() << "-Tsvg" << "-O" << name + ".dot");
	if (!graphviz.waitForStarted()) {
		fdbgf("Could not start graphviz");
		return;
	}
	if (!graphviz.waitForFinished(-1)) {
		fdbgf("Problem when running graphviz: waitForFinished failed");
		return;
	}
	QTime end(0, 0);
	end = end.addMSecs(t.elapsed());
	fdbg << "Finished after" << end.toString() << "(" << t.elapsed() / 1000.0 / 60.0 << "minutes)";
}
