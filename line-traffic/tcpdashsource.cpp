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

#include "tcpdashsource.h"
#include "util.h"
#include "chronometer.h"

TCPDashSourceArg::TCPDashSourceArg(qreal rate_Bps, qreal bufferingRate_Bps,
                                   qreal bufferingTime_s, qreal streamingPeriod_s)
    : rate_Bps(rate_Bps),
      bufferingRate_Bps(bufferingRate_Bps),
      bufferingTime_s(bufferingTime_s),
      streamingPeriod_s(streamingPeriod_s),
      state(TCPDashSourceArg::Connecting),
      bytesSentBuffering(0)
{}

qint64 TCPDashSourceArg::shouldSend(quint64 totalBytesSent, qreal time)
{
    // Pre-actions
    if (state == TCPDashSourceArg::Buffering) {
        bytesSentBuffering = totalBytesSent;
    }

    // State transitions
    if (state == TCPDashSourceArg::Connecting) {
        state = TCPDashSourceArg::Buffering;
    } else if (state == TCPDashSourceArg::Buffering) {
        if (time >= bufferingTime_s) {
            state = TCPDashSourceArg::Streaming;
        }
    } else if (state == TCPDashSourceArg::Streaming) {
        // Nothing to do
    }

    // Post-actions
    if (state == TCPDashSourceArg::Buffering) {
        qreal idealOffset = time * bufferingRate_Bps;
        return idealOffset - totalBytesSent;
    } else if (state == TCPDashSourceArg::Streaming) {
        qreal idealOffset = bytesSentBuffering +
                            (1 + qFloor((time - bufferingTime_s) / streamingPeriod_s)) * streamingPeriod_s * rate_Bps;
        return idealOffset - totalBytesSent;
    }
    return 0;
}

TCPDashSource::TCPDashSource(int fd, TCPDashSourceArg params)
    : TCPClient(fd),
      params(params)
{}

TCPClient *TCPDashSource::makeTCPDashSource(int fd, void *arg)
{
    Q_ASSERT(arg);
    TCPDashSourceArg *params = (TCPDashSourceArg*)arg;
    return new TCPDashSource(fd, *params);
}

void TCPDashSource::onWrite()
{
    TCPClient::onWrite();

    qint64 size = params.shouldSend(totalWritten, (getCurrentTimeNanosec() - tConnect) * 1.0e-9);
    if (size > 0) {
        QByteArray b;
        b.fill('z', size);
        write(b);
    }
}

void TCPDashSource::onStop()
{
    TCPClient::onStop();
    printUploadStats();
}
