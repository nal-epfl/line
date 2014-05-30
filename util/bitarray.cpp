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

#include "bitarray.h"


BitArray::BitArray() {
    bitCount = 0;
}

BitArray& BitArray::append(int bit) {
    if (bitCount % 64 == 0) {
        // extend
        bits << 0ULL;
    }
    bits.last() = (bits.last() << 1) | (bit ? 1ULL : 0ULL);
    bitCount++;
    return *this;
}

quint64 BitArray::count() const {
    return bitCount;
}

QString BitArray::toString() const {
    QString result;
    quint64 bitsLeft = bitCount;
    foreach (quint64 word, bits) {
        if (!result.isEmpty()) {
            result += "\n";
        }
        if (bitsLeft < 64) {
            word <<= 64 - bitsLeft;
        }
        for (quint64 i = qMin(64ULL, bitsLeft); i > 0; i--) {
            result += (word & (1ULL << 63)) ? "1" : "0";
            word <<= 1;
            bitsLeft--;
        }
    }
    return result;
}

QVector<quint8> BitArray::toVector() const {
    QVector<quint8> result;
    quint64 bitsLeft = bitCount;
    foreach (quint64 word, bits) {
        if (bitsLeft < 64) {
            word <<= 64 - bitsLeft;
        }
        for (quint64 i = qMin(64ULL, bitsLeft); i > 0; i--) {
            result << ((word & (1ULL << 63)) ? 1 : 0);
            word <<= 1;
            bitsLeft--;
        }
    }
    return result;
}

BitArray& BitArray::operator<<(int bit) {
    return append(bit);
}

void BitArray::reserve(int size)
{
    bits.reserve(size / 64);
}

void BitArray::clear()
{
    bits.clear();
    bitCount = 0;
}

QDataStream& operator<<(QDataStream& s, const BitArray& d) {
    qint32 ver = 1;
    s << ver;

    if (ver == 1) {
        s << d.bits;
        s << d.bitCount;
    }

    return s;
}

QDataStream& operator>>(QDataStream& s, BitArray& d) {
    qint32 ver;
    s >> ver;

    if (ver == 1) {
        s >> d.bits;
        s >> d.bitCount;
    } else {
        qDebug() << __FILE__ << __LINE__ << "Read error";
        exit(-1);
    }

    return s;
}

void BitArray::test() {
    BitArray bits;
    QVector<quint8> reference;

    for (int i = 0; i < 100; i++) {
        for (int c = i; c > 0; c--) {
            bits << 0;
            reference << 0;
            if (reference != bits.toVector()) {
                qDebug() << "FAIL";
                qDebug() << reference;
                qDebug() << bits.toString();
                qDebug() << bits.count();
				qDebug() << __FILE__ << __LINE__;
				exit(EXIT_FAILURE);
            }
        }
        for (int c = i; c > 0; c--) {
            bits << 1;
            reference << 1;
            if (reference != bits.toVector()) {
                qDebug() << "FAIL";
                qDebug() << reference;
                qDebug() << bits.toString();
                qDebug() << bits.count();
				qDebug() << __FILE__ << __LINE__;
				exit(EXIT_FAILURE);
            }
        }
    }
    qDebug() << "OK";
}

