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

#ifndef UDPVBR_H
#define UDPVBR_H

#include "evudp.h"

class UDPVBRSourceArg {
public:
	UDPVBRSourceArg(qreal baseRate_Bps, quint16 frameSize = 1400);
	// UDP payload size
	quint16 frameSize;
	// UDP payload data rate, B/s
	qreal baseRate;
	qreal lossThreshold1; // 0-1
	qreal lossThreshold2; // 0-1

	qreal rateRatio;
	qreal computedRate;

	quint64 tUpdate;
	qreal loss;
	qreal remoteRate;
	int feedback;

	qreal payloadRateBps();
	// loss is the effective frame loss rate (the logical loss is smaller due to dups)
	void updateRate();

	bool shouldSend(quint64 totalBytesSent, qreal time);
};

class UDPVBRSource : public UDPClient {
public:
	UDPVBRSource(int fd, UDPVBRSourceArg params);
	static UDPClient* makeUDPVBRSource(int fd, void *arg);
	void onRead(QByteArray b);
	virtual void onWrite();
	virtual void onStop();

	UDPVBRSourceArg params;
	quint64 seqNo;
};

class UDPVBRSink : public UDPClient {
public:
	UDPVBRSink(int fd);
	static UDPClient* makeUDPVBRSink(int fd, void *arg);
	virtual void onRead(QByteArray buffer);
	virtual void onWrite();
	virtual void onStop();

	quint64 nextSeqNo;
	quint64 lossCounter;
	quint64 lossSeqNoStart;
	quint64 lastBytesRead;
	qreal loss;
	qreal updateInterval;
	qreal sendInterval;
	quint64 tUpdate;
	quint64 tSend;
};

#endif // UDPVBR_H
