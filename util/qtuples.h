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

#ifndef QTUPLES_H
#define QTUPLES_H

#include <QtCore/qdatastream.h>

template <class T1, class T2, class T3>
struct QTriple
{
    typedef T1 first_type;
    typedef T2 second_type;
    typedef T3 third_type;

    QTriple()
        : first(T1()),
          second(T2()),
          third(T3())
    {}

    QTriple(const T1 &t1, const T2 &t2, const T3 &t3)
        : first(t1),
          second(t2),
          third(t3)
    {}

    QTriple<T1, T2, T3> &operator=(const QTriple<T1, T2, T3> &other) {
        first = other.first;
        second = other.second;
        third = other.third;
        return *this;
    }

    T1 first;
    T2 second;
    T3 third;
};

template <class T1, class T2, class T3>
inline bool operator==(const QTriple<T1, T2, T3> &p1, const QTriple<T1, T2, T3> &p2)
{
    return p1.first == p2.first &&
            p1.second == p2.second &&
            p1.third == p2.third;
}

template <class T1, class T2, class T3>
inline bool operator!=(const QTriple<T1, T2, T3> &p1, const QTriple<T1, T2, T3> &p2)
{
    return !(p1 == p2);
}

template <class T1, class T2, class T3>
inline bool operator<(const QTriple<T1, T2, T3> &p1, const QTriple<T1, T2, T3> &p2)
{
    return p1.first < p2.first ||
            (!(p2.first < p1.first) && p1.second < p2.second ||
             (!(p2.second < p1.second) && p1.third < p2.third));
}

template <class T1, class T2, class T3>
inline bool operator>(const QTriple<T1, T2, T3> &p1, const QTriple<T1, T2, T3> &p2)
{
    return p2 < p1;
}

template <class T1, class T2, class T3>
inline bool operator<=(const QTriple<T1, T2, T3> &p1, const QTriple<T1, T2, T3> &p2)
{
    return !(p2 < p1);
}

template <class T1, class T2, class T3>
inline bool operator>=(const QTriple<T1, T2, T3> &p1, const QTriple<T1, T2, T3> &p2)
{
    return !(p1 < p2);
}

template <class T1, class T2, class T3>
QTriple<T1, T2, T3> qMakeTriple(const T1 &t1, const T2 &t2, const T3 &t3)
{
    return QTriple<T1, T2, T3>(t1, t2, t3);
}

template <class T1, class T2, class T3>
inline QDataStream& operator>>(QDataStream& s, QTriple<T1, T2, T3>& p)
{
    s >> p.first >> p.second >> p.third;
    return s;
}

template <class T1, class T2, class T3>
inline QDataStream& operator<<(QDataStream& s, const QTriple<T1, T2, T3>& p)
{
    s << p.first << p.second << p.third;
    return s;
}

///////// QUAD

template <class T1, class T2, class T3, class T4>
struct QQuad
{
    typedef T1 first_type;
    typedef T2 second_type;
    typedef T3 third_type;
    typedef T4 fourth_type;

    QQuad()
        : first(T1()),
          second(T2()),
          third(T3()),
          fourth(T4())
    {}

    QQuad(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
        : first(t1),
          second(t2),
          third(t3),
          fourth(t4)
    {}

    QQuad<T1, T2, T3, T4> &operator=(const QQuad<T1, T2, T3, T4> &other) {
        first = other.first;
        second = other.second;
        third = other.third;
        fourth = other.fourth;
        return *this;
    }

    T1 first;
    T2 second;
    T3 third;
    T4 fourth;
};

template <class T1, class T2, class T3, class T4>
inline bool operator==(const QQuad<T1, T2, T3, T4> &p1, const QQuad<T1, T2, T3, T4> &p2)
{
    return p1.first == p2.first &&
            p1.second == p2.second &&
            p1.third == p2.third &&
            p1.fourth == p2.fourth;
}

template <class T1, class T2, class T3, class T4>
inline bool operator!=(const QQuad<T1, T2, T3, T4> &p1, const QQuad<T1, T2, T3, T4> &p2)
{
    return !(p1 == p2);
}

template <class T1, class T2, class T3, class T4>
inline bool operator<(const QQuad<T1, T2, T3, T4> &p1, const QQuad<T1, T2, T3, T4> &p2)
{
    return p1.first < p2.first ||
            (!(p2.first < p1.first) && p1.second < p2.second ||
             (!(p2.second < p1.second) && p1.third < p2.third ||
              (!(p2.third < p1.third) && p1.fourth < p2.fourth)));
}

template <class T1, class T2, class T3, class T4>
inline bool operator>(const QQuad<T1, T2, T3, T4> &p1, const QQuad<T1, T2, T3, T4> &p2)
{
    return p2 < p1;
}

template <class T1, class T2, class T3, class T4>
inline bool operator<=(const QQuad<T1, T2, T3, T4> &p1, const QQuad<T1, T2, T3, T4> &p2)
{
    return !(p2 < p1);
}

template <class T1, class T2, class T3, class T4>
inline bool operator>=(const QQuad<T1, T2, T3, T4> &p1, const QQuad<T1, T2, T3, T4> &p2)
{
    return !(p1 < p2);
}

template <class T1, class T2, class T3, class T4>
QQuad<T1, T2, T3, T4> qMakeQuad(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
{
    return QQuad<T1, T2, T3, T4>(t1, t2, t3, t4);
}

template <class T1, class T2, class T3, class T4>
inline QDataStream& operator>>(QDataStream& s, QQuad<T1, T2, T3, T4>& p)
{
    s >> p.first >> p.second >> p.third >> p.fourth;
    return s;
}

template <class T1, class T2, class T3, class T4>
inline QDataStream& operator<<(QDataStream& s, const QQuad<T1, T2, T3, T4>& p)
{
    s << p.first << p.second << p.third << p.fourth;
    return s;
}

///////// Quint

template <class T1, class T2, class T3, class T4, class T5>
struct QQuint
{
    typedef T1 first_type;
    typedef T2 second_type;
    typedef T3 third_type;
    typedef T4 fourth_type;
    typedef T5 fifth_type;

    QQuint()
        : first(T1()),
          second(T2()),
          third(T3()),
          fourth(T4()),
          fifth(T5())
    {}

    QQuint(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5)
        : first(t1),
          second(t2),
          third(t3),
          fourth(t4),
          fifth(t5)
    {}

    QQuint<T1, T2, T3, T4, T5> &operator=(const QQuint<T1, T2, T3, T4, T5> &other) {
        first = other.first;
        second = other.second;
        third = other.third;
        fourth = other.fourth;
        fifth = other.fifth;
        return *this;
    }

    T1 first;
    T2 second;
    T3 third;
    T4 fourth;
    T5 fifth;
};

template <class T1, class T2, class T3, class T4, class T5>
inline bool operator==(const QQuint<T1, T2, T3, T4, T5> &p1, const QQuint<T1, T2, T3, T4, T5> &p2)
{
    return p1.first == p2.first &&
            p1.second == p2.second &&
            p1.third == p2.third &&
            p1.fourth == p2.fourth &&
            p1.fifth == p2.fifth;
}

template <class T1, class T2, class T3, class T4, class T5>
inline bool operator!=(const QQuint<T1, T2, T3, T4, T5> &p1, const QQuint<T1, T2, T3, T4, T5> &p2)
{
    return !(p1 == p2);
}

template <class T1, class T2, class T3, class T4, class T5>
inline bool operator<(const QQuint<T1, T2, T3, T4, T5> &p1, const QQuint<T1, T2, T3, T4, T5> &p2)
{
    return p1.first < p2.first ||
            (!(p2.first < p1.first) && p1.second < p2.second ||
             (!(p2.second < p1.second) && p1.third < p2.third ||
              (!(p2.third < p1.third) && p1.fourth < p2.fourth) ||
              (!(p2.fourth < p1.fourth) && p1.fifth < p2.fifth)));
}

template <class T1, class T2, class T3, class T4, class T5>
inline bool operator>(const QQuint<T1, T2, T3, T4, T5> &p1, const QQuint<T1, T2, T3, T4, T5> &p2)
{
    return p2 < p1;
}

template <class T1, class T2, class T3, class T4, class T5>
inline bool operator<=(const QQuint<T1, T2, T3, T4, T5> &p1, const QQuint<T1, T2, T3, T4, T5> &p2)
{
    return !(p2 < p1);
}

template <class T1, class T2, class T3, class T4, class T5>
inline bool operator>=(const QQuint<T1, T2, T3, T4, T5> &p1, const QQuint<T1, T2, T3, T4, T5> &p2)
{
    return !(p1 < p2);
}

template <class T1, class T2, class T3, class T4, class T5>
QQuint<T1, T2, T3, T4, T5> qMakeQuint(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5)
{
    return QQuint<T1, T2, T3, T4, T5>(t1, t2, t3, t4, t5);
}

template <class T1, class T2, class T3, class T4, class T5>
inline QDataStream& operator>>(QDataStream& s, QQuint<T1, T2, T3, T4, T5>& p)
{
    s >> p.first >> p.second >> p.third >> p.fourth >> p.fifth;
    return s;
}

template <class T1, class T2, class T3, class T4, class T5>
inline QDataStream& operator<<(QDataStream& s, const QQuint<T1, T2, T3, T4, T5>& p)
{
    s << p.first << p.second << p.third << p.fourth << p.fifth;
    return s;
}

#endif // QTUPLES_H
