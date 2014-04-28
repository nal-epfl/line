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

#ifndef UDPCBR_H
#define UDPCBR_H

#include "evudp.h"

class UDPCBRSourceArg {
public:
	UDPCBRSourceArg(qreal rate_Bps, quint16 frameSize = 1400, bool poisson = false);
	qreal rate_Bps; // UDP payload data rate
	quint16 frameSize; // UDP payload size
	bool poisson;

	qreal timeNextSend;

	qreal payloadRateBps();

	bool shouldSend(quint64 totalBytesSent, qreal time);
};

class UDPCBRSource : public UDPClient {
public:
	UDPCBRSource(int fd, UDPCBRSourceArg params);
	static UDPClient* makeUDPCBRSource(int fd, void *arg);
	virtual void onWrite();
	virtual void onStop();

	UDPCBRSourceArg params;
};

class UDPVCBRSourceArg {
public:
	UDPVCBRSourceArg(qreal rate_Bps, quint16 frameSize = 1400);
	qreal rate_Bps; // UDP payload data rate
	qreal actualRate;
	quint16 frameSize; // UDP payload size

	void changeRate();
	bool shouldSend(quint64 totalBytesSent, qreal time);
};

class UDPVCBRSource : public UDPClient {
public:
	UDPVCBRSource(int fd, UDPVCBRSourceArg params);
	static UDPClient* makeUDPVCBRSource(int fd, void *arg);
	virtual void onWrite();
	virtual void onStop();

	UDPVCBRSourceArg params;
};

#endif // UDPCBR_H
