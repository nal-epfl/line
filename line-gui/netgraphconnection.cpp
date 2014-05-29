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

#include "netgraphconnection.h"
#include "util.h"

NetGraphConnection::NetGraphConnection()
{
	setDefaultParams();
}

bool readString(QStringList &tokens, QString &value) {
	if (tokens.isEmpty()) {
		return false;
	}
	value = tokens.takeFirst();
	return true;
}

bool readInt(QStringList &tokens, qint32 &value) {
	if (tokens.isEmpty()) {
		return false;
	}
	QString arg = tokens.takeFirst();
	bool ok;
	value = arg.toInt(&ok);
	if (!ok) {
		return false;
	}
	return true;
}

bool readDouble(QStringList &tokens, qreal &value) {
	if (tokens.isEmpty()) {
		return false;
	}
	QString arg = tokens.takeFirst();
	bool ok;
	value = arg.toDouble(&ok);
	if (!ok) {
		return false;
	}
	return true;
}

bool readDataSizeWithUnit2bits(QStringList &tokens, quint64 &value) {
	if (tokens.isEmpty()) {
		return false;
	}
	QString arg = tokens.takeFirst();
	bool ok;
	value = dataSizeWithUnit2bits(arg, &ok);
	if (!ok) {
		return false;
	}
	return true;
}

bool NetGraphConnection::setParamsFromType()
{
	QStringList tokens = encodedType.split(" ", QString::SkipEmptyParts);

	if (tokens.isEmpty()) {
		return false;
	}

	QString arg = tokens.takeFirst();
	basicType = arg;
	if (arg == "TCP") {
		// Nothing to do
	} else if (arg.startsWith("TCPx")) {
		QString s = arg.split('x').at(1);
		bool ok;
		multiplier = s.toInt(&ok);
		if (!ok) {
			return false;
		}
		basicType = "TCPx";
	} else if (arg == "TCP-Poisson-Pareto") {
		if (!readDouble(tokens, poissonRate)) {
			return false;
		}
		if (!readDouble(tokens, paretoAlpha)) {
			return false;
		}
		if (!readDataSizeWithUnit2bits(tokens, paretoScale_b)) {
			return false;
		}
		if (!tokens.isEmpty() && tokens.first() == "sequential") {
			tokens.takeFirst();
			sequential = true;
		}
    } else if (arg == "TCP-DASH") {
        if (!readDouble(tokens, rate_Mbps)) {
            return false;
        }
        if (!readDouble(tokens, bufferingRate_Mbps)) {
            return false;
        }
        if (!readDouble(tokens, bufferingTime_s)) {
            return false;
        }
        if (!readDouble(tokens, streamingPeriod_s)) {
            return false;
        }
    } else if (arg == "UDP-CBR") {
		if (!readDouble(tokens, rate_Mbps)) {
			return false;
		}
		if (!tokens.isEmpty() && tokens.first() == "poisson") {
			tokens.takeFirst();
			poisson = true;
		}
	} else if (arg == "UDP-VBR") {
		// Nothing to do
	} else if (arg == "UDP-VCBR") {
		if (!readDouble(tokens, rate_Mbps)) {
			return false;
		}
	} else if (arg == "cmd") {
		// Nothing to do
	} else if (arg == "dummy") {
		// Nothing to do
	} else if (arg == "UDP") {
		return false;
	}
	while (!tokens.isEmpty()) {
		arg = tokens.takeFirst();
		if (arg == "on-off") {
			onOff = true;
			if (!readDouble(tokens, onDurationMin)) {
				return false;
			}
			if (!readDouble(tokens, onDurationMax)) {
				return false;
			}
			if (!readDouble(tokens, offDurationMin)) {
				return false;
			}
			if (!readDouble(tokens, offDurationMax)) {
				return false;
			}
		} else if (arg == "class") {
			if (!readInt(tokens, trafficClass)) {
				return false;
			}
		} else if (arg == "x") {
			if (!readInt(tokens, multiplier)) {
				return false;
			}
		} else if (arg == "cc") {
			if (!readString(tokens, tcpCongestionControl)) {
				return false;
			}
		} else {
			return false;
		}
	}
	return true;
}

void NetGraphConnection::setLegacyTypeFromParams()
{
	if (encodedType.startsWith("TCP")) {
		// Nothing to do
	} else if (encodedType.startsWith("TCPx")) {
		encodedType = QString("TCPx%1").arg(multiplier);
	} else if (encodedType.startsWith("TCP-Poisson-Pareto")) {
		encodedType = QString("TCP-Poisson-Pareto %1 %2 %3b %4").
					  arg(poissonRate).
					  arg(paretoAlpha).
					  arg(paretoScale_b).
					  arg(sequential ? "sequential" : "");
	} else if (encodedType.startsWith("UDP-CBR") || encodedType.startsWith("UDP CBR")) {
		encodedType = QString("UDP-CBR %1 %2").
					  arg(rate_Mbps).
					  arg(poisson ? "poisson" : "");
	} else if (encodedType.startsWith("UDP-VBR") || encodedType.startsWith("UDP VBR")) {
		encodedType = QString("UDP-VBR");
	} else if (encodedType.startsWith("UDP-VCBR") || encodedType.startsWith("UDP VCBR")) {
		encodedType = QString("UDP-VCBR %1").
					  arg(rate_Mbps);
	}

	if (trafficClass != 0) {
		encodedType = QString("%1 class %2")
					  .arg(encodedType)
					  .arg(trafficClass);
	}

	if (onOff) {
		encodedType = QString("%1 on-off %2 %3 %4 %5")
					  .arg(encodedType)
					  .arg(onDurationMin)
					  .arg(onDurationMax)
					  .arg(offDurationMin)
					  .arg(offDurationMax);
	}

	if (multiplier > 1) {
		encodedType = QString("%1 x %2")
					  .arg(encodedType)
					  .arg(multiplier);
	}
}

void NetGraphConnection::setTypeFromParams()
{
	if (basicType == "TCP") {
		// Nothing to do
		encodedType = basicType;
	} else if (basicType == "TCPx") {
		encodedType = QString("%1%2").arg(basicType).arg(multiplier);
	} else if (basicType == "TCP-Poisson-Pareto") {
		encodedType = QString("%1 %2 %3 %4 %5")
					  .arg(basicType)
					  .arg(poissonRate)
					  .arg(paretoAlpha)
					  .arg(bits2DataSizeWithUnit(paretoScale_b))
					  .arg(sequential ? "sequential" : "");
    } else if (basicType == "TCP-DASH") {
        encodedType = QString("%1 %2 %3 %4 %5")
                      .arg(basicType)
                      .arg(rate_Mbps)
                      .arg(bufferingRate_Mbps)
                      .arg(bufferingTime_s)
                      .arg(streamingPeriod_s);
    } else if (basicType == "UDP-CBR") {
		encodedType = QString("%1 %2 %3")
					  .arg(basicType)
					  .arg(rate_Mbps)
					  .arg(poisson ? "poisson" : "");
	} else if (basicType == "UDP-VBR") {
		encodedType = QString("%1").arg(basicType);
	} else if (basicType == "UDP-VCBR") {
		encodedType = QString("%1 %2")
					  .arg(basicType)
					  .arg(rate_Mbps);
	}

	if (trafficClass != 0) {
		encodedType = QString("%1 class %2")
					  .arg(encodedType)
					  .arg(trafficClass);
	}

	if (onOff) {
		encodedType = QString("%1 on-off %2 %3 %4 %5")
					  .arg(encodedType)
					  .arg(onDurationMin)
					  .arg(onDurationMax)
					  .arg(offDurationMin)
					  .arg(offDurationMax);
	}

	if (!tcpCongestionControl.isEmpty()) {
		encodedType = QString("%1 cc %2")
					  .arg(encodedType)
					  .arg(tcpCongestionControl);
	}

	if (multiplier > 1) {
		encodedType = QString("%1 x %2")
					  .arg(encodedType)
					  .arg(multiplier);
	}
}

NetGraphConnection::NetGraphConnection(int source, int dest, QString encodedType, QByteArray data)
{
	setDefaultParams();
	this->source = source;
	this->dest = dest;
	this->encodedType = encodedType;
	this->data = data;
	Q_ASSERT_FORCE(setParamsFromType());
}

void NetGraphConnection::setDefaultParams()
{
	trafficClass = 0;
	onOff = false;
	onDurationMin = 1.0;
	onDurationMax = 5.0;
	offDurationMin = 1.0;
	offDurationMax = 5.0;
	color = 0x00000000;
	pathIndex = -1;
	multiplier = 1;
	poissonRate = 0.1;
	paretoAlpha = 1.5;
	paretoScale_b = 5000000;
	rate_Mbps = 1.0;
	poisson = false;
    bufferingRate_Mbps = 10 * 1.0e6;
    bufferingTime_s = 60;
    streamingPeriod_s = 5;
	sequential = false;
	encodedType = "";
	basicType = "";
	maskedOut = false;
	delayStart = false;
	ev_loop = NULL;
	tcpCongestionControl = "";
}

QString NetGraphConnection::tooltip()
{
	return encodedType;
}

QString NetGraphConnection::toText()
{
	QStringList result;
	result << QString("Connection %1").arg(index + 1);
	result << QString("Source %1").arg(source + 1);
	result << QString("Dest %1").arg(dest + 1);
	result << QString("Type %1").arg(encodedType);
    return result.join("\n");
}

bool NetGraphConnection::isLight()
{
    return (basicType == "UDP-CBR" && rate_Mbps < 1.0);
}

QDataStream& operator<<(QDataStream& s, const NetGraphConnection& c)
{
    qint32 ver = 8;

	if (!unversionedStreams) {
		s << ver;
	} else {
		ver = 0;
	}

	s << c.index;
	s << c.source;
	s << c.dest;
	s << c.encodedType;
	s << c.data;

	s << c.serverKey;
	s << c.clientKey;
	s << c.port;
	s << c.ports;

	if (ver >= 1) {
		s << c.color;
	}

	if (ver >= 2) {
		s << c.trafficClass;
	}

	if (ver >= 3) {
		s << c.onOff;
	}

	if (ver >= 4) {
		s << c.tcpReceiveWindowSize;
	}

	if (ver >= 5) {
		s << c.onDurationMin;
		s << c.onDurationMax;
		s << c.offDurationMin;
		s << c.offDurationMax;
	}

	if (ver >= 6) {
		s << c.basicType;
		s << c.multiplier;
		s << c.poissonRate;
		s << c.paretoAlpha;
		s << c.paretoScale_b;
		s << c.rate_Mbps;
		s << c.poisson;
	}

	if (ver >= 7) {
		s << c.tcpCongestionControl;
	}

    if (ver >= 8) {
        s << c.bufferingRate_Mbps;
        s << c.bufferingTime_s;
        s << c.streamingPeriod_s;
    }

	return s;
}

QDataStream& operator>>(QDataStream& s, NetGraphConnection& c)
{
	c.setDefaultParams();

	qint32 ver = 0;

	if (!unversionedStreams) {
		s >> ver;
	}

	s >> c.index;
	s >> c.source;
	s >> c.dest;
	s >> c.encodedType;
	s >> c.data;

	s >> c.serverKey;
	s >> c.clientKey;
	s >> c.port;
	s >> c.ports;

	if (ver >= 1) {
		s >> c.color;
	}

	if (ver >= 2) {
		s >> c.trafficClass;
	} else {
		c.trafficClass = 0;
	}

	if (ver >= 3) {
		s >> c.onOff;
	} else {
		c.onOff = false;
	}

	if (ver >= 4) {
		s >> c.tcpReceiveWindowSize;
	} else {
		c.tcpReceiveWindowSize = 0;
	}

	if (ver >= 5) {
		s >> c.onDurationMin;
		s >> c.onDurationMax;
		s >> c.offDurationMin;
		s >> c.offDurationMax;
	} else {
		c.onDurationMin = 1.0;
		c.onDurationMax = 5.0;
		c.offDurationMin = 1.0;
		c.offDurationMax = 5.0;
	}

	if (ver >= 6) {
		s >> c.basicType;
		s >> c.multiplier;
		s >> c.poissonRate;
		s >> c.paretoAlpha;
		s >> c.paretoScale_b;
		s >> c.rate_Mbps;
		s >> c.poisson;
	}

	if (ver >= 7) {
		s >> c.tcpCongestionControl;
	}

    if (ver >= 8) {
        s >> c.bufferingRate_Mbps;
        s >> c.bufferingTime_s;
        s >> c.streamingPeriod_s;
    }

	if (ver < 6) {
		c.setLegacyTypeFromParams();
		Q_ASSERT_FORCE(c.setParamsFromType());
	}
	c.setParamsFromType();

    Q_ASSERT_FORCE(ver <= 8);

	return s;
}
