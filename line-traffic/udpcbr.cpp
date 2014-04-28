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

#include "udpcbr.h"
#include "util.h"
#include "chronometer.h"

UDPCBRSourceArg::UDPCBRSourceArg(qreal rate_Bps, quint16 frameSize, bool poisson) :
	rate_Bps(rate_Bps),
	frameSize(frameSize),
	poisson(poisson)
{
	timeNextSend = 0;
}

bool UDPCBRSourceArg::shouldSend(quint64 totalBytesSent, qreal time)
{
	if (!poisson) {
		qreal expectedSent = udpRawRate2PayloadRate(rate_Bps, frameSize) * time;
		return (totalBytesSent + frameSize < expectedSent);
	} else {
		if (time >= timeNextSend) {
			qreal delta = -log(1.0 - frandex()) / (udpRawRate2PayloadRate(rate_Bps, frameSize) / (frameSize));
			timeNextSend = (timeNextSend == 0) ? time : (timeNextSend + delta);
			return true;
		} else {
			return false;
		}
	}
}

UDPCBRSource::UDPCBRSource(int fd, UDPCBRSourceArg params) :
	UDPClient(fd),
	params(params)
{
}

UDPClient* UDPCBRSource::makeUDPCBRSource(int fd, void *arg)
{
	Q_ASSERT(arg);
	UDPCBRSourceArg *params = (UDPCBRSourceArg*)arg;
	return new UDPCBRSource(fd, *params);
}

void UDPCBRSource::onWrite()
{
	UDPClient::onWrite();

	if (params.shouldSend(totalWritten, (getCurrentTimeNanosec() - tConnect) * 1.0e-9)) {
		QByteArray b;
		b.fill('z', params.frameSize);
		write(b);
	}
}

void UDPCBRSource::onStop()
{
	UDPClient::onStop();
	printUploadStats();
}

UDPVCBRSourceArg::UDPVCBRSourceArg(qreal rate_Bps, quint16 frameSize) :
	rate_Bps(rate_Bps), frameSize(frameSize)
{
	changeRate();
}

void UDPVCBRSourceArg::changeRate()
{
	actualRate = frand() * rate_Bps;
}

bool UDPVCBRSourceArg::shouldSend(quint64 totalBytesSent, qreal time)
{
	qreal expectedSent = udpRawRate2PayloadRate(rate_Bps, frameSize) * time;
	return (totalBytesSent + frameSize < expectedSent);
}

UDPVCBRSource::UDPVCBRSource(int fd, UDPVCBRSourceArg params) :
	UDPClient(fd),
	params(params)
{
}

UDPClient* UDPVCBRSource::makeUDPVCBRSource(int fd, void *arg)
{
	Q_ASSERT(arg);
	UDPVCBRSourceArg *params = (UDPVCBRSourceArg*)arg;
	return new UDPVCBRSource(fd, *params);
}

void UDPVCBRSource::onWrite()
{
	UDPClient::onWrite();

	if (params.shouldSend(totalWritten, (getCurrentTimeNanosec() - tConnect) * 1.0e-9)) {
		QByteArray b;
		b.fill('z', params.frameSize);
		write(b);
	}
}

void UDPVCBRSource::onStop()
{
	UDPClient::onStop();
	printUploadStats();
}
