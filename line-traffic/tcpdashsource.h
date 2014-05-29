/*
 *	Copyright (C) 2014 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license which this program may be distributed under.
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

#ifndef TCPDASHSOURCE_H
#define TCPDASHSOURCE_H

#include "evtcp.h"

class TCPDashSourceArg {
public:
    enum State {
        Connecting,
        Buffering,
        Streaming
    };

    TCPDashSourceArg(qreal rate_Bps, qreal bufferingRate_Bps,
                     qreal bufferingTime_s, qreal streamingPeriod_s);
    qreal rate_Bps; // average data rate
    qreal bufferingRate_Bps; // buffering rate (source-throttling for 0 <= t <= bufferingTime_s)
    qreal bufferingTime_s;
    qreal streamingPeriod_s; // period to send chunks without source-throttling during Streaming
    State state;
    qint64 bytesSentBuffering;

    qint64 shouldSend(quint64 totalBytesSent, qreal time);
};

// DASH source for a TCP stream.
class TCPDashSource : public TCPClient {
public:
    TCPDashSource(int fd, TCPDashSourceArg params);
    static TCPClient* makeTCPDashSource(int fd, void *arg);
    virtual void onWrite();
    virtual void onStop();

protected:
    TCPDashSourceArg params;
};

#endif // TCPDASHSOURCE_H
