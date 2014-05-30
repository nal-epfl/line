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

#ifndef BITARRAY_H
#define BITARRAY_H

#include <QtCore>

// Memory-efficient storage for an array of bits.
// The array can only increase in length.
class BitArray {
public:
    // Creates an empty array.
    BitArray();

    // Appends a bit to the end of the array.
    // Returns a reference to self.
    BitArray &append(int bit);

    // Returns the number of bits in the array.
    quint64 count() const;

    // Returns a vector of bytes holding the bits, one bit per byte.
    // A byte can be either 0 or 1.
    QVector<quint8> toVector() const;

    // Appends the bit to the end of the array.
    // Returns a reference to self.
    BitArray &operator<<(int bit);

    void reserve(int size);

    void clear();

    // Serializes the array into some representation (currently ASCII).
    QString toString() const;

    static void test();

protected:
    QVector<quint64> bits;
	quint64 bitCount;

    friend QDataStream& operator>>(QDataStream& s, BitArray& d);
    friend QDataStream& operator<<(QDataStream& s, const BitArray& d);
};

QDataStream& operator>>(QDataStream& s, BitArray& d);
QDataStream& operator<<(QDataStream& s, const BitArray& d);

#endif // BITARRAY_H
