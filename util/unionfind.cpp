/*
 *	Copyright (C) 2012 Ovidiu Mara
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

#include "unionfind.h"

class UnionFindNode
{
public:
	UnionFindNode(quint32 label) : label(label) {
		parent = this;
		rank = 0;
	}

	UnionFindNode *find() {
		if (parent == this)
			return this;
		parent = parent->find();
		return parent;
	}

	UnionFindNode *parent;
	quint32 label;
	qint32 rank;
};

class UnionFind
{
public:
	UnionFind() {}

	void addSet() {
		UnionFindNode *root = new UnionFindNode(nodes.count());
		nodes << root;
	}

	int find(int i) {
		return nodes[i]->find()->label;
	}

	void unite(int i, int j) {
		UnionFindNode *ri = nodes[find(i)];
		UnionFindNode *rj = nodes[find(j)];
		if (ri == rj)
			return;

		if (ri->rank < rj->rank) {
			ri->parent = rj;
		} else if (ri->rank > rj->rank) {
			rj->parent = ri;
		} else {
			rj->parent = ri;
			ri->rank++;
		}
	}

	QList<UnionFindNode*> nodes;
};

void unionfindtest()
{
	//

	UnionFind u;
	for (int i = 0; i < 10; i++)
		u.addSet();

	u.unite(0, 6);
	u.unite(2, 8);
	u.unite(2, 4);
	u.unite(4, 6);

	u.unite(1, 3);
	u.unite(1, 5);
	u.unite(1, 7);
	u.unite(1, 9);

	for (int i = 0; i < 10; i++)
		fprintf(stderr, "%d ", u.find(i));
	fprintf(stderr, "\n");

	exit(0);
}
