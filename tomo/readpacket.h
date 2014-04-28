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

#ifndef READPACKET_H
#define READPACKET_H

#include <QtCore>

class IPHeader {
public:
	quint8 source[4];
	quint8 dest[4];

	QString sourceString;
	QString destString;

	quint16 totalLength;
	quint16 headerLength;
	quint16 payloadLength;

	quint16 id;

	bool fragDontFragment;
	bool fragMoreFragments;
	quint16 fragOffset;

	quint8 ttl;
	quint8 protocol;
	QString protocolString;
};

class TCPHeader {
public:
	quint16 sourcePort;
	quint16 destPort;
	qint64 seq;
	qint64 ack;

	quint16 totalLength;
	quint16 headerLength;
	quint16 payloadLength;

	bool flagSyn;
	bool flagAck;
	bool flagRst;
	bool flagFin;
	bool flagPsh;
	bool flagUrg;

	quint32 windowScaleFactor;
	quint32 mss;
	quint32 windowRaw;

	QList<QPair<qint64, qint64> > sacks;
};

class UDPHeader {
public:
	quint16 sourcePort;
	quint16 destPort;

	quint16 totalLength;
	quint16 headerLength;
	quint16 payloadLength;
};

bool decodePacket(QByteArray buffer, IPHeader &iph, TCPHeader &tcph, UDPHeader &udph);

#endif // READPACKET_H
