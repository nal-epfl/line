/*
* Copyright (C) 2012-2014 qar
* License: http://www.gnu.org/licenses/gpl.html GPL version 2
*/

#include "compresseddevice.h"

#include <QtCore>
#include <QDebug>

CompressedDevice::CompressedDevice(QIODevice* rawDevice, OpenMode mode, QObject* parent) :
    QIODevice(parent),
    rawDevice(rawDevice),
    version(maxVersion()),
    uncompressedPos(0),
    reachedEnd(false)
{
	if (mode != QIODevice::NotOpen)
		open(mode);
}

CompressedDevice::~CompressedDevice()
{
    if (isOpen()) {
        close();
    }
}

bool CompressedDevice::open(OpenMode mode)
{
	uncompressedBuffer.reserve(256 * 1000 * 1000);

    uncompressedPos = 0;
    reachedEnd = false;
    if ((mode & QIODevice::Append) ||
        (mode & QIODevice::Text) ||
        (!(mode & QIODevice::ReadWrite)) ||
        ((mode & QIODevice::ReadOnly) && (mode & QIODevice::WriteOnly))) {
        qDebug() << "Incorrect open flags!";
        exit(-1);
        return false;
    }

    if (!rawDevice->isOpen()) {
        if (!rawDevice->open(mode)) {
            qDebug() << "Could not open file!";
            exit(-1);
            return false;
        }
    } else {
        // The read/write flags of the two devices must be identical.
        // The file must not be open in text mode.
        OpenMode mask = QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append;
        if ((rawDevice->openMode() & mask) != (mode & mask)) {
            qDebug() << "Mismatch between open flags!";
            exit(-1);
            return false;
        }
    }

    setOpenMode(mode | QIODevice::Unbuffered);

    if (mode & QIODevice::WriteOnly) {
        // If write, get enc mode as param and write it
        QDataStream out(rawDevice);

        version = maxVersion();
        out << version;
    } else if (mode & QIODevice::ReadOnly) {
        // If read, then read enc mode etc
        QDataStream in(rawDevice);

        in >> version;
        if (version < minVersion() || version > maxVersion()) {
            qDebug() << "Could not decode version number!";
            exit(-1);
            return false;
        }
    } else {
        qDebug() << "Incorrect open flags!";
        exit(-1);
        return false;
    }

    return true;
}

void CompressedDevice::close()
{
    if (openMode() & QIODevice::WriteOnly) {
        if (!uncompressedBuffer.isEmpty()) {
            writeSegment();
        }
        // Empty buffer means end of stream
		writeSegment();
    }
    setOpenMode(NotOpen);
}

bool CompressedDevice::isSequential() const
{
    return true;
}

qint64 CompressedDevice::pos() const
{
    return uncompressedPos;
}

qint64 CompressedDevice::size() const
{
    return bytesAvailable();
}

bool CompressedDevice::seek(qint64)
{
    qDebug() << "Could not seek! Not supported.";
    return false;
}

bool CompressedDevice::atEnd() const
{
    if (!uncompressedBuffer.isEmpty())
        return false;
    return reachedEnd;
}

bool CompressedDevice::reset()
{
    qDebug() << "Could not reset stream! Not supported.";
    return false;
}

qint64 CompressedDevice::bytesAvailable() const
{
    return uncompressedBuffer.size();
}

qint64 CompressedDevice::bytesToWrite() const
{
    return 0;
}

qint64 CompressedDevice::writeData(const char* data, qint64 size)
{
	for (qint64 i = 0; i < size; i++) {
		uncompressedBuffer.append(data[i]);
	}
    uncompressedPos += size;
    if (uncompressedBuffer.size() >= maxUncompressedBufferSize()) {
        writeSegment();
    }
    return size;
}

qint64 CompressedDevice::readData(char* data, qint64 size)
{
    while (!atEnd() && uncompressedBuffer.size() < size) {
        if (!readSegment())
            break;
    }

    size_t countRead = qMin(size, (qint64)uncompressedBuffer.size());
	for (size_t i = 0; i < countRead; i++) {
		data[i] = uncompressedBuffer.takeFirst();
	}
    uncompressedPos += countRead;

    if (uncompressedBuffer.isEmpty() && !atEnd())
        readSegment();

    if (countRead == 0 && atEnd())
        return -1;

    return countRead;
}

bool CompressedDevice::writeSegment()
{
	buffer1.resize(0);
	while (!uncompressedBuffer.isEmpty()) {
		buffer1.append(uncompressedBuffer.takeFirst());
	}

	if (1) {
		buffer2 = qCompress(buffer1);
	} else {
		buffer2 = buffer1;
	}

	QByteArray lengths;
	{
		QDataStream out(&lengths, QIODevice::WriteOnly);
		out << qint32(buffer1.length());
		out << qint32(buffer2.length());
	}

	// qDebug() << "Writing:" << buffer1.length() << buffer2.length();

    if (writeRawData(lengths) != lengths.length()) {
        qDebug() << "Could not write segment header!";
        return false;
    }

	if (writeRawData(buffer2) != buffer2.length()) {
		qDebug() << "Could not write segment!";
		return false;
	}

    return true;
}

bool CompressedDevice::readSegment()
{
    QByteArray lengths;
	readRawData(lengths, 2 * sizeof(qint32));

	qint32 uncompressedLength;
	qint32 compressedLength;
	{
		QDataStream in(&lengths, QIODevice::ReadOnly);
		in.setVersion(QDataStream::Qt_4_0);
		in >> uncompressedLength;
		in >> compressedLength;
	}

	buffer1.resize(uncompressedLength);
    buffer2.resize(compressedLength);

	// qDebug() << "Reading:" << buffer1.length() << buffer2.length();

	readRawData(buffer2, compressedLength);
	if (buffer2.length() != compressedLength) {
        qDebug() << "Could not read segment!";
        return false;
    }

	if (1) {
		buffer1 = qUncompress(buffer2);
	} else {
		buffer1 = buffer2;
	}

	uncompressedBuffer.reserve(uncompressedBuffer.size() + buffer1.length());

	for (int i = 0; i < buffer1.length(); i++) {
		uncompressedBuffer.append(buffer1[i]);
	}

    if (uncompressedLength == 0) {
        reachedEnd = true;
    }

    return true;
}

qint64 CompressedDevice::writeRawData(QByteArray data)
{
    qint64 totalWritten = 0;
    for (int i = 0; i < data.size();) {
        qint64 chunk = rawDevice->write(data.constData() + i, data.size() - i);
        if (chunk <= 0) {
            qDebug() << "Probably a write error!";
            break;
        }
        i += chunk;
        totalWritten += chunk;
    }
	// qDebug() << "Write raw:" << totalWritten;
    return totalWritten;
}

void CompressedDevice::readRawData(QByteArray &data, int len)
{
    data.resize(len);

    int totalRead = 0;
    while (totalRead < len) {
        qint64 chunk = rawDevice->read(data.data() + totalRead, len - totalRead);
        if (chunk <= 0) {
            qDebug() << "Probably a read error!";
            break;
        }
        totalRead += chunk;
    }
    data.resize(totalRead);
	// qDebug() << "Read raw:" << totalRead;
}

qint64 CompressedDevice::maxUncompressedBufferSize() const
{
    return 128 * 1024 * 1024;
}

qint32 CompressedDevice::minVersion()
{
    return 1;
}

qint32 CompressedDevice::maxVersion()
{
    return 1;
}
