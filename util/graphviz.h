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

#ifndef GRAPHVIZ_H
#define GRAPHVIZ_H

#include <QtCore>

// Steel Blue
#define COLOR_DIRECT "#4863A0"
// Sky Blue
#define COLOR_DIRECT_SRC "#82CAFF"
// Green4
#define COLOR_REVERSE "#347C17"
// Green
#define COLOR_REVERSE_SRC "#00FF00"
// Dark Goldenrod1
#define COLOR_RR "#FBB117"
// Firebrick1
#define COLOR_TARGET "#F62817"
// light yellow
#define COLOR_DIRECT_REVERSE "#FFFF99"

QString graphVizName(QString ip);
QString graphVizName(QPair<QString, QString> e);
void generateGraphViz(QHash<QString, QHash<QString, QVariant> > &nodes, QHash<QPair<QString, QString>, QHash<QString, QVariant> > &edges, QString name, QString engine = "sfdp");

typedef QPair<QString, QString> Edge;

#endif // GRAPHVIZ_H
