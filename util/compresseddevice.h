/*
* Copyright (C) 2012-2014 qar
* License: http://www.gnu.org/licenses/gpl.html GPL version 2
*/

#ifndef CompressedDevice_H
#define CompressedDevice_H

#include <QtCore/QIODevice>
#include <ovector.h>

class CompressedDevice : public QIODevice
{
    Q_OBJECT

public:
    // Creates an CompressedDevice on top of an existing device (e.g. a QFile).
    CompressedDevice(QIODevice* rawDevice, OpenMode mode = QIODevice::NotOpen, QObject* parent = 0);

    virtual ~CompressedDevice();

    // Opens the compressed device.
    // If the raw device is not open, it will be opened using the same mode.
    // If the raw device is open, it must have the same open mode.
    // The flags QIODevice::Append and QIODevice::Text are forbidden.
    // Returns true for success.
    virtual bool open(OpenMode mode);

    // Closes the compressed device (not the raw device). Writes any buffered data and the ending sequence.
    virtual void close();

    // Returns true. Seeking is not supported.
    virtual bool isSequential() const;

    // Returns the position in the uncompressed stream.
    virtual qint64 pos() const;

    // Returns bytesAvailable(), NOT the size of the uncompressed stream.
    virtual qint64 size() const;

    // Seeking is not supported. Does nothing and returns false.
    virtual bool seek(qint64 pos);

    // Returns true if there is no more uncompressed data to be read. The rawDevice might still have data though.
    virtual bool atEnd() const;

    // Reset is not supported. Does nothing and returns false.
    virtual bool reset();

    // Returns the number of bytes that can be read immediately.
    virtual qint64 bytesAvailable() const;

    // Returns zero.
    virtual qint64 bytesToWrite() const;

protected:
    virtual qint64 writeData(const char* data, qint64 size);
    virtual qint64 readData(char* data, qint64 size);

    bool writeSegment();
    bool readSegment();

    qint64 writeRawData(QByteArray data);
    void readRawData(QByteArray &data, int len);

    qint64 maxUncompressedBufferSize() const;
    qint64 rawBlockSize() const;

    static qint32 minVersion();
    static qint32 maxVersion();

private:
    QIODevice* rawDevice;
    qint32 version;
    OVector<char> uncompressedBuffer;
    qint64 uncompressedPos;
    bool reachedEnd;

	QByteArray buffer1;
	QByteArray buffer2;
};

#endif // CompressedDevice_H
