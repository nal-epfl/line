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

#ifndef ROUTE_H
#define ROUTE_H

#include <QtCore>

class Route {
public:
	Route(int destination, int nextHop) :
		destination(destination), nextHop(nextHop)
	{}
	Route() {}
	// node ID or AS ID
	int destination;
	// nextHop = ID of next node on the path to that destination
	int nextHop;

	// two routes are equal of they have the same destination and the same next hop
	bool operator==(const Route &other) const {
		return (this->destination == other.destination) && (this->nextHop == other.nextHop);
	}
	inline bool operator!=(const Route &other) const { return !(*this == other); }
};

#define AS_AGGREGATION 0
class RoutingTable {
public:
	// Maps a destination (node id) (from any AS) to one or more routes (by next hop) to that destination
	QMultiHash<int, Route> localRoutes;
#if AS_AGGREGATION
	// Maps a different AS destination (AS id) to one or more routes (by next hop) to that destination
	QMultiHash<int, Route> interASroutes;
#else
	// not guaranteed to contain the route here, fallback to localRoutes if not found
	QMultiHash<int, Route> aggregateInterASroutes;
#endif
};

inline QDebug operator<<(QDebug debug, const Route &route)
{
	debug.nospace() << "to " <<  route.destination << " via " << route.nextHop;
	return debug.space();
}

QDataStream& operator>>(QDataStream& s, Route& r);
QDataStream& operator<<(QDataStream& s, const Route& r);

QDataStream& operator>>(QDataStream& s, RoutingTable& t);
QDataStream& operator<<(QDataStream& s, const RoutingTable& t);

#endif // ROUTE_H
