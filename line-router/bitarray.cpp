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
        bits << 0ULL;
    }
    if (bit) {
        bits.last() = (bits.last() << 1) | 1ULL;
    } else {
        bits.last() = (bits.last() << 1);
    }
    bitCount++;
    return *this;
}

quint64 BitArray::count() {
    return bitCount;
}

QByteArray BitArray::serialize() {
    QByteArray result;
    quint64 bitsLeft = bitCount;
    foreach (quint64 word, bits) {
        if (bitsLeft < 64) {
            word <<= 64 - bitsLeft;
        }
        for (quint64 i = qMin(64ULL, bitsLeft); i > 0; i--) {
            result += (word & (1ULL << 63)) ? "1 " : "0 ";
            word <<= 1;
            bitsLeft--;
        }
    }
    return result;
}

QVector<quint8> BitArray::toVector() {
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

BitArray& BitArray::operator<< (int bit) {
    return append(bit);
}

void BitArray::test() {
    BitArray bits;
    QByteArray reference;

    for (int i = 0; i < 100; i++) {
        for (int c = i; c > 0; c--) {
            bits << 0;
            reference += "0 ";
            if (reference != bits.serialize()) {
                qDebug() << "FAIL";
                qDebug() << reference;
                qDebug() << bits.serialize();
                qDebug() << bits.count();
				qDebug() << __FILE__ << __LINE__;
				exit(EXIT_FAILURE);
            }
        }
        for (int c = i; c > 0; c--) {
            bits << 1;
            reference += "1 ";
            if (reference != bits.serialize()) {
                qDebug() << "FAIL";
                qDebug() << reference;
                qDebug() << bits.serialize();
                qDebug() << bits.count();
				qDebug() << __FILE__ << __LINE__;
				exit(EXIT_FAILURE);
            }
        }
    }
    qDebug() << "OK";
}

