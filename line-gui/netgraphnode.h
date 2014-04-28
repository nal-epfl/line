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

#ifndef NETGRAPHNODE_H
#define NETGRAPHNODE_H

#include <QtCore>

#include "netgraphcommon.h"
#include "route.h"

#define NETGRAPH_NODE_RADIUS 16
#define NETGRAPH_NODE_RADIUS_FAST 16

#define NETGRAPH_NODE_HOST    0
#define NETGRAPH_NODE_GATEWAY 1
#define NETGRAPH_NODE_ROUTER  2
#define NETGRAPH_NODE_BORDER  3

#define NETGRAPH_NODE_SQUARE  4
#define NETGRAPH_NODE_ELLIPSE 5

// 0 or 1, based on whether the system allows using the .0 subnet address
#define IP_OFFSET 1

#ifdef LINE_CLIENT
#include "../remote_config.h"
#endif

class NetGraphNode
{
public:
	NetGraphNode();

	qint32 index;    // node index in graph (0..n-1)
	qreal x;     // geometric position
	qreal y;
	qint32 nodeType; // host, gateway, router, border
	qint32 ASNumber;
	bool loadBalancing;

	bool used;    // must be updated explicitly
	RoutingTable routes; // must be updated explicitly

	QHash<QString, QString> tags;

	QString customLabel; // used instead of the index for drawing
	QRgb bgColor;

    // traffic flags
    bool web;
    bool vvoip;
    bool p2p;
    bool iptv;
	bool server;
	bool heavy;

	// Used instead of the automatically computed IP
	QString customIp;
	QString customIpForeign;

    // only set in line-traffic
	bool maskedOut;

	QString ip() {
		if (!customIp.isEmpty())
			return customIp;
		return ip(index);
	}

#ifdef LINE_CLIENT
	QString ipForeign() {
		if (!customIpForeign.isEmpty())
			return customIpForeign;
//		if (QString(REMOTE_DEDICATED_IF_HOSTS) == "lo") {
//			return ip(index);
//		} else {
			return ip(index | 0x00800000);
//		}
	}
#endif

	static QString ip(int i) {
		return QString("10.%1.%2.%3")
				.arg(((i+IP_OFFSET) & 0x00FF0000) >> 16)
				.arg(((i+IP_OFFSET) & 0x0000FF00) >> 8)
				.arg(((i+IP_OFFSET) & 0x000000FF) >> 0);
	}

	QString routeTooltip();

	QString toText();

	static QRgb colorFromType(int nodeType);
};

QDataStream& operator>>(QDataStream& s, NetGraphNode& n);

QDataStream& operator<<(QDataStream& s, const NetGraphNode& n);

#endif // NETGRAPHNODE_H
