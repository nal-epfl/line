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

#include "netgraphnode.h"

NetGraphNode::NetGraphNode()
{
	used = true;
	loadBalancing = false;
    web = false;
    vvoip = false;
    p2p = false;
    iptv = false;
	server = false;
	heavy = false;
	maskedOut = false;
	customIp.clear();
	customIpForeign.clear();
}

QDataStream& operator<<(QDataStream& s, const NetGraphNode& n)
{
	qint32 ver = 7;

	if (!unversionedStreams) {
		s << ver;
	} else {
		ver = 0;
	}

	s << n.index;
	s << n.x;
	s << n.y;
	s << n.nodeType;
	s << n.ASNumber;
	s << n.used;
	s << n.routes;

	if (ver >= 1) {
		s << n.customLabel;
	}
	if (ver >= 2) {
		s << n.tags;
	}
	if (ver >= 3) {
		s << n.bgColor;
	}
    if (ver >= 4) {
        s << n.web;
        s << n.vvoip;
        s << n.p2p;
        s << n.iptv;
    }
	if (ver >= 5) {
		s << n.server;
	}
	if (ver >= 6) {
		s << n.heavy;
	}
	if (ver >= 7) {
		s << n.customIp;
		s << n.customIpForeign;
	}
	return s;
}

QDataStream& operator>>(QDataStream& s, NetGraphNode& n)
{
	qint32 ver = 0;

	if (!unversionedStreams) {
		s >> ver;
	}

	s >> n.index;
	s >> n.x;
	s >> n.y;
	s >> n.nodeType;
	s >> n.ASNumber;
	s >> n.used;
	s >> n.routes;

	if (ver >= 1) {
		s >> n.customLabel;
	}
	if (ver >= 2) {
		s >> n.tags;
	}
	if (ver >= 3) {
		s >> n.bgColor;
	} else {
		n.bgColor = NetGraphNode::colorFromType(n.nodeType);
	}
    if (ver >= 4) {
        s >> n.web;
        s >> n.vvoip;
        s >> n.p2p;
        s >> n.iptv;
    } else {
        n.web = false;
        n.vvoip = false;
        n.p2p = false;
        n.iptv = false;
    }
	if (ver >= 5) {
		s >> n.server;
	} else {
		n.server = false;
	}
	if (ver >= 6) {
		s >> n.heavy;
	} else {
		n.heavy = false;
	}
	if (ver >= 7) {
		s >> n.customIp;
		s >> n.customIpForeign;
	} else {
		n.customIp.clear();
		n.customIpForeign.clear();
	}
	return s;
}

QString NetGraphNode::routeTooltip()
{
	QString result;
	result += QString("<b>Routing table for node %1 (%2)</b><br/>").arg(index + 1).arg(ip());
	result += QString("<table border='1' cellspacing='-1' cellpadding='2'>");
	result += QString("<tr><th>Destination</th><th>Next node</th></tr>");
	foreach (Route r, routes.localRoutes.values()) {
		result += QString("<tr><td>%1 (%2)</td><td>%3 (%4)</td></tr>").arg(r.destination + 1).arg(ip(r.destination)).arg(r.nextHop + 1).arg(ip(r.nextHop));
	}
#if AS_AGGREGATION
	foreach (Route r, routes.interASroutes.values()) {
		result += QString("<tr><td>AS %1</td><td>%2 (%3)</td></tr>").arg(r.destination + 1).arg(r.nextHop + 1).arg(ip(r.nextHop));
	}
#else
	foreach (Route r, routes.aggregateInterASroutes.values()) {
		result += QString("<tr><td>AS %1</td><td>%2 (%3)</td></tr>").arg(r.destination + 1).arg(r.nextHop + 1).arg(ip(r.nextHop));
	}
#endif
	result += QString("</table>");
	return result;
}

QString NetGraphNode::toText()
{
	QStringList result;
	result << QString("Node %1").arg(index + 1);
	result << QString("AS %1").arg(ASNumber + 1);
	result << QString("Type %1").arg(nodeType == NETGRAPH_NODE_HOST ? "host" :
																	  nodeType == NETGRAPH_NODE_GATEWAY ? "gateway" :
																										  nodeType == NETGRAPH_NODE_ROUTER ? "router" :
																																			 nodeType == NETGRAPH_NODE_BORDER ? "border" : "unknown");
	result << QString("Coords %1 %2").arg(x).arg(y);
	result << QString("Load-balancing %1").arg(loadBalancing ? "true" : "false");
	result << QString("IP %1").arg(ip());
	result << QString("Flags %1%2%3%4%5%6").
			  arg(web ? "web " : "").
			  arg(vvoip ? "vvoip " : "").
			  arg(p2p ? "p2p " : "").
			  arg(iptv ? "iptv " : "").
			  arg(server ? "server " : "").
			  arg(heavy ? "heavy " : "");
	foreach (QString key, tags) {
		result << QString("Tag \"%1\" \"%2\"").arg(key).arg(tags[key]);
	}
	return result.join("\n");
}

QRgb NetGraphNode::colorFromType(int nodeType)
{
	if (nodeType == NETGRAPH_NODE_HOST) {
		return 0xFFffFFff;
	} else if (nodeType == NETGRAPH_NODE_GATEWAY) {
		return 0xFFddDDdd;
	} else if (nodeType == NETGRAPH_NODE_ROUTER) {
		return 0xFFddDDdd;
	} else if (nodeType == NETGRAPH_NODE_BORDER) {
		return 0xFFccCCcc;
	}
	return 0xFFaaAAaa;
}
