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

#include "udpvbr.h"
#include "util.h"
#include "chronometer.h"

UDPVBRSourceArg::UDPVBRSourceArg(qreal baseRate_Bps, quint16 frameSize) :
	frameSize(frameSize), baseRate(baseRate_Bps)
{
	frameSize = qMax(frameSize, (quint16)sizeof(quint64));
	rateRatio = 1;
	lossThreshold1 = 0.01;
	lossThreshold2 = 0.05;
	rateRatio = 1;
	loss = 0.0;
	remoteRate = 0.0;
	feedback = 0;
	computedRate = baseRate * rateRatio;
}

qreal UDPVBRSourceArg::payloadRateBps()
{
	return qreal(frameSize) / (frameSize + UDP_HEADER_OVERHEAD) * computedRate;
}

void UDPVBRSourceArg::updateRate()
{
	const qreal maxRateRatio = 1.0e99;
	const qreal minRateRatio = 1.0;
	if (feedback == 5) {
		if (loss < lossThreshold1) {
			rateRatio = rateRatio * 1.1;
		} else if (loss < lossThreshold2) {
			rateRatio = rateRatio / 1.1;
		} else {
			rateRatio = rateRatio / 1.1;
		}
		feedback--;
	} else if (feedback <= 0) {
		rateRatio = rateRatio / 10.0;
	} else {
		rateRatio = rateRatio;
		feedback--;
	}
	rateRatio = qMin(maxRateRatio, qMax(minRateRatio, rateRatio));
	qreal computedRate_Mbps = baseRate * rateRatio * 8.0 / 1.0e6;
	computedRate_Mbps = floor(computedRate_Mbps * 2.0) / 2.0;
	computedRate_Mbps = qMax(computedRate_Mbps, 0.1);
	computedRate = computedRate_Mbps * 1.0e6 / 8.0;
	qDebug() << "Rate ratio:" << rateRatio
			 << "loss:" << loss
			 << "feedback:" << feedback
			 << "computedRate (Mbps):" << withCommasStr(computedRate * 8.0 / 1000000.0)
			 << "remoteRate (Mbps):" << withCommasStr(remoteRate * 8.0 / 1000000.0);
	tUpdate = getCurrentTimeNanosec();
}

bool UDPVBRSourceArg::shouldSend(quint64 totalBytesSent, qreal time)
{
	if (getCurrentTimeNanosec() - tUpdate > 0.050 * 1.0e9) {
		updateRate();
	}

	qreal expectedSent = udpRawRate2PayloadRate(computedRate, frameSize) * time;
	return (totalBytesSent + frameSize < expectedSent);
}

UDPVBRSource::UDPVBRSource(int fd, UDPVBRSourceArg params) : UDPClient(fd), params(params)
{
	seqNo = 0;
}

UDPClient* UDPVBRSource::makeUDPVBRSource(int fd, void *arg)
{
	Q_ASSERT(arg);
	UDPVBRSourceArg *params = (UDPVBRSourceArg*)arg;
	return new UDPVBRSource(fd, *params);
}

void UDPVBRSource::onRead(QByteArray b)
{
	QDataStream in(b);
	in >> params.loss;
	in >> params.remoteRate;
	params.feedback = 5;
	params.updateRate();
}

void UDPVBRSource::onWrite()
{
	UDPClient::onWrite();

	if (params.shouldSend(totalWritten, (getCurrentTimeNanosec() - tConnect) * 1.0e-9)) {
		QByteArray b;
		{
			QDataStream out(&b, QIODevice::WriteOnly);
			out << seqNo;
			seqNo++;
		}
		b.append(QByteArray().fill('z', params.frameSize - b.count()));
		write(b);
	}
}

void UDPVBRSource::onStop()
{
	UDPClient::onStop();
	printUploadStats();
}

UDPVBRSink::UDPVBRSink(int fd) : UDPClient(fd)
{
	nextSeqNo = 0;
	lossCounter = 0;
	lossSeqNoStart = 0;
	lastBytesRead = 0;
	loss = 0;
	updateInterval = 0.100; // seconds
	sendInterval = 0.001; // seconds
	tUpdate = getCurrentTimeNanosec();
	tSend = getCurrentTimeNanosec();
}

UDPClient* UDPVBRSink::makeUDPVBRSink(int fd, void *arg)
{
	Q_UNUSED(arg);
	return new UDPVBRSink(fd);
}

void UDPVBRSink::onRead(QByteArray b)
{
	quint64 seqNo;
	QDataStream in(b);
	in >> seqNo;
	if (seqNo >= nextSeqNo) {
		lossCounter += seqNo - nextSeqNo;
		nextSeqNo = seqNo + 1;
	}
	if ((getCurrentTimeNanosec() - tUpdate) * 1.0e-9 >= updateInterval) {
		tUpdate = getCurrentTimeNanosec();
		if (nextSeqNo - lossSeqNoStart != 0) {
			loss = lossCounter / (qreal)(nextSeqNo - lossSeqNoStart);
		} else {
			loss = 0;
		}
		lossCounter = 0;
		lossSeqNoStart = nextSeqNo;
	}
}

void UDPVBRSink::onWrite()
{
	if ((getCurrentTimeNanosec() - tSend) * 1.0e-9 >= sendInterval) {
		qreal downloadRate = (getTotalBytesRead() - lastBytesRead)/((getCurrentTimeNanosec() - tSend));
		lastBytesRead = getTotalBytesRead();
		tSend = getCurrentTimeNanosec();
		QByteArray b;
		{
			QDataStream out(&b, QIODevice::WriteOnly);
			out << loss;
			out << downloadRate;
		}
		write(b);
	}
}

void UDPVBRSink::onStop()
{
	UDPClient::onStop();
	printDownloadStats();
}
