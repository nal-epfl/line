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

#ifndef QRGBLINE_H
#define QRGBLINE_H

#ifndef QT_GUI_LIB

typedef unsigned int QRgb;

const QRgb  RGB_MASK    = 0x00ffffff;                // masks RGB values

inline int qRed(QRgb rgb)                // get red part of RGB
{ return ((rgb >> 16) & 0xff); }

inline int qGreen(QRgb rgb)                // get green part of RGB
{ return ((rgb >> 8) & 0xff); }

inline int qBlue(QRgb rgb)                // get blue part of RGB
{ return (rgb & 0xff); }

inline int qAlpha(QRgb rgb)                // get alpha part of RGBA
{ return rgb >> 24; }

inline QRgb qRgb(int r, int g, int b)// set RGB value
{ return (0xffu << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff); }

inline QRgb qRgba(int r, int g, int b, int a)// set RGBA value
{ return ((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff); }

inline int qGray(int r, int g, int b)// convert R,G,B to gray 0..255
{ return (r*11+g*16+b*5)/32; }

inline int qGray(QRgb rgb)                // convert RGB to gray 0..255
{ return qGray(qRed(rgb), qGreen(rgb), qBlue(rgb)); }

inline bool qIsGray(QRgb rgb)
{ return qRed(rgb) == qGreen(rgb) && qRed(rgb) == qBlue(rgb); }

#else
#include <qrgb.h>
#endif

#endif // QRGBLINE_H
