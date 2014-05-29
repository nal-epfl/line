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

#ifndef NETGRAPHCONNECTION_H
#define NETGRAPHCONNECTION_H

#include <QtCore>

#include "netgraphcommon.h"

#ifndef QT_GUI_LIB
typedef unsigned int QRgb;
#else
#include <qrgb.h>
#endif

class NetGraphConnection
{
public:
	NetGraphConnection();
	NetGraphConnection(int source, int dest, QString encodedType, QByteArray data);

	void setDefaultParams();
	bool setParamsFromType();
	void setTypeFromParams();
	void setLegacyTypeFromParams();

	qint32 index;
	qint32 source;
	qint32 dest;
	// put everything you want in type and data
	QString encodedType; // normally this is something meaningful, like "tcp" etc. with parameters
	QString basicType; // this is the first token of type
	QByteArray data; // this can hold the details (ports, transfer size etc.)
	// if type == "cmd"
	QString serverCmd;
	QString clientCmd;

	// various fields to be used freely by any code
	QString serverKey;
	QString clientKey;
	qint32 port;
    qint32 serverFD;
    qint32 clientFD;
	// <= 0 means OS default
	qint32 tcpReceiveWindowSize;
	QString tcpCongestionControl;

	QList<qint32> ports;
	QStringList serverKeys;
	QStringList clientKeys;
	QSet<qint32> serverFDs;
	QSet<qint32> clientFDs;

    // Traffic class: by default, 0.
    qint32 trafficClass;

    // If traffic is continuous or on-off.
    bool onOff;
	qreal onDurationMin;
	qreal onDurationMax;
	qreal offDurationMin;
	qreal offDurationMax;

	// Other params
	qint32 multiplier;
	qreal poissonRate;
	qreal paretoAlpha;
	quint64 paretoScale_b;
	qreal rate_Mbps;
	bool poisson;
	// For poisson connections, if sequential is true, do not create a new transfer before the old one has finished.
	// After the old one has finished, wait for a delay distributed exponentially according to the poissonRate.
	bool sequential;
    qreal bufferingRate_Mbps;
    qreal bufferingTime_s;
    qreal streamingPeriod_s;

	// 0 means auto color by index
	QRgb color;

	// only set in some cases, otherwise it should be -1
	qint32 pathIndex;

    // only set in line-traffic
	bool maskedOut;
	void *ev_loop;
	bool delayStart;

	QString tooltip();

	QString toText();

    bool isLight();

    static NetGraphConnection cmdConnection(int source, int dest, QString serverCmd, QString clientCmd, qint32 trafficClass = 0) {
		NetGraphConnection c (source, dest, QString("cmd class=%1").arg(trafficClass), "");
		c.serverCmd = serverCmd;
		c.clientCmd = clientCmd;
		return c;
	}
};

QDataStream& operator>>(QDataStream& s, NetGraphConnection& c);

QDataStream& operator<<(QDataStream& s, const NetGraphConnection& c);

#endif // NETGRAPHCONNECTION_H
