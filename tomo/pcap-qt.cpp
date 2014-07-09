#include "pcap-qt.h"

#include <byteswap.h>

#include <QtCore>

PcapReader::PcapReader(QString fileName)
{
	ok = true;
	this->device = NULL;
	QFile *file = new QFile(fileName);
	this->device = file;
	ownsDevice = true;
	if (!file->open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		ok = false;
	} else {
		readHeader();
	}
}

PcapReader::PcapReader(QIODevice *device)
{
	this->device = device;
	ownsDevice = false;
	ok = true;
	readHeader();
}

PcapReader::~PcapReader()
{
	if (ownsDevice) {
		delete device;
		device = NULL;
	}
}

bool PcapReader::isOk()
{
	return ok;
}

bool PcapReader::atEnd()
{
	if (!ok)
		return true;
	return device->atEnd();
}

void bswap(quint8 &) {}

void bswap(qint8 &) {}

void bswap(quint16 &x) {
	x = __bswap_16(x);
}

void bswap(qint16 &x) {
	x = __bswap_16(x);
}

void bswap(quint32 &x) {
	x = __bswap_32(x);
}

void bswap(qint32 &x) {
	x = __bswap_32(x);
}

void bswap(quint64 &x) {
	x = __bswap_64(x);
}

void bswap(qint64 &x) {
	x = __bswap_64(x);
}

bool PcapReader::readHeader()
{
	if (!ok)
		return false;
	if (!read(&header, sizeof(PcapHeader))) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}
	if (header.magic_number == kPcapMagicNativeMicrosec) {
		mustSwap = false;
		tsFracMultiplier = 1000;
	} else if (header.magic_number == kPcapMagicSwappedMicrosec) {
		mustSwap = true;
		tsFracMultiplier = 1000;
	} else if (header.magic_number == kPcapMagicNativeNanosec) {
		mustSwap = false;
		tsFracMultiplier = 1;
	} else if (header.magic_number == kPcapMagicSwappedNanosec) {
		mustSwap = true;
		tsFracMultiplier = 1;
	} else {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}

	if (mustSwap) {
		bswap(header.version_major);
		bswap(header.version_minor);
		bswap(header.thiszone);
		bswap(header.sigfigs);
		bswap(header.snaplen);
		bswap(header.network);
	}

	if (header.version_major != 2 || header.version_minor != 4) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}

	if (header.network != LINKTYPE_RAW &&
		header.network != LINKTYPE_ETHERNET &&
		header.network != LINKTYPE_LINUX_SLL) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}

	return true;
}

bool PcapReader::readPacket(PcapPacketHeader &packetHeader, QByteArray &packet)
{
	if (!ok || atEnd())
		return false;

	if (!read(&packetHeader, sizeof(packetHeader))) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}

	if (mustSwap) {
		bswap(packetHeader.ts_sec);
		bswap(packetHeader.ts_nsec);
		bswap(packetHeader.orig_len);
		bswap(packetHeader.incl_len);
	}

	packetHeader.ts_nsec *= tsFracMultiplier;

	if (packetHeader.orig_len > 65536 ||
		packetHeader.incl_len > 65536) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}

	packet.resize(packetHeader.incl_len);
	if (!read(packet.data(), packetHeader.incl_len)) {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		ok = false;
		return false;
	}

	return true;
}

PcapHeader PcapReader::getPcapHeader()
{
	return header;
}

bool PcapReader::read(void *p, qint64 len)
{
	if (!ok)
		return false;
	char *b = (char *)p;
	while (len > 0) {
		qint64 count = device->read(b, len);
		if (count < 0) {
			qDebug() << __FILE__ << __LINE__ << "Read error";
			ok = false;
			return false;
		}
		b += count;
		len -= count;
	}
	return true;
}


PcapWriter::PcapWriter(QString fileName, quint32 network)
{
	ok = true;
	this->device = NULL;
	QFile *file = new QFile(fileName);
	this->device = file;
	ownsDevice = true;
	if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		ok = false;
	} else {
		writeHeader(network);
	}
}

PcapWriter::PcapWriter(QIODevice *device, quint32 network)
{
	this->device = device;
	ownsDevice = false;
	ok = true;
	writeHeader(network);
}

PcapWriter::~PcapWriter()
{
	close();
	if (ownsDevice) {
		delete device;
		device = NULL;
	}
}

bool PcapWriter::isOk()
{
	return ok;
}

bool PcapWriter::writePacket(quint64 timestampNanosec, int originalLength, QByteArray packet)
{
	PcapPacketHeader pcapPacketHeader;
	pcapPacketHeader.ts_nsec = quint32(timestampNanosec % 1000000000ULL);
	pcapPacketHeader.ts_sec = quint32(timestampNanosec / 1000000000ULL);
	pcapPacketHeader.incl_len = packet.length();
	pcapPacketHeader.orig_len = originalLength;
	writePacket(pcapPacketHeader, packet);
	return isOk();
}

bool PcapWriter::writePacket(PcapPacketHeader pcapPacketHeader, QByteArray packet)
{
	write(&pcapPacketHeader, sizeof(pcapPacketHeader));
	write(packet.constData(), packet.length());
	return isOk();
}

bool PcapWriter::writeHeader(quint32 network)
{
	PcapHeader pcapHeader;
	pcapHeader.magic_number = kPcapMagicNativeNanosec;
	pcapHeader.version_major = 2;
	pcapHeader.version_minor = 4;
	pcapHeader.thiszone = 0;
	pcapHeader.sigfigs = 0;
	pcapHeader.snaplen = 0xffFFffFF;
	pcapHeader.network = network;
	write(&pcapHeader, sizeof(pcapHeader));
	return isOk();
}

void PcapWriter::close()
{
	device->close();
}

bool PcapWriter::write(const void *p, qint64 len)
{
	if (!ok)
		return false;
	char *b = (char *)p;
	while (len > 0) {
		qint64 count = device->write(b, len);
		if (count < 0) {
			qDebug() << __FILE__ << __LINE__ << "Write error";
			ok = false;
			return false;
		}
		b += count;
		len -= count;
	}
	return true;
}
