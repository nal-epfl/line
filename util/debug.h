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

#ifndef DEBUG_H
#define DEBUG_H

#include <QtDebug>
#include <qglobal.h>

#define DBG 1
#define fdbg if (DBG) qDebug() << __FILE__ << __LINE__ << "-" << __FUNCTION__ << ":"
#define fdbgf(X) if (DBG) qDebug() << __FILE__ << __LINE__ << "-" << __FUNCTION__ << ":" << X
#define fdbgv(X) if (DBG) qDebug() << __FILE__ << __LINE__ << "-" << __FUNCTION__ << ":" << #X << "=" << X
#define fdbgv2(X) if (DBG >= 2) qDebug() << __FILE__ << __LINE__ << "-" << __FUNCTION__ << ":" << #X << "=" << X

#define str(X) (#X)

#define fdbg2 if (DBG >= 2) qDebug() << __FILE__ << __LINE__ << "-" << __FUNCTION__ << ":"

#define simplePrint(X) fprintf(stdout, "%s",(QString() + X).toAscii().data());

#if 0
#ifndef PRINT_HTML
#	define setBlue() fprintf(stdout, "\033[34m")
#	define setCyan() fprintf(stdout, "\033[36m")
#	define setBrown() fprintf(stdout, "\033[33m")
#	define setYellow() fprintf(stdout, "\033[1;33m")
#	define setRed() fprintf(stdout, "\033[31m")
#	define setGreen() fprintf(stdout, "\033[1;32m")
#	define setOrange() fprintf(stdout, "\033[1;31m")
#	define resetColor() fprintf(stdout, "\033[0m")
#	define BR() fprintf(stdout, "\n")
#	define startOutput() ;
#	define endOutput() ;
#else
#	define setBlue() fprintf(stdout, "<span style=\"color:blue\">")
#	define setBrown() fprintf(stdout, "<span style=\"color:brown\">")
#	define setYellow() fprintf(stdout, "<span style=\"font-size:150%%;text-decoration:underline;font-weight:bold;\">")
#	define setRed() fprintf(stdout, "<span style=\"color:red\">")
#	define setGreen() fprintf(stdout, "<span style=\"color:green\">")
#	define setOrange() fprintf(stdout, "<span style=\"color:orangered\">")
#	define resetColor() fprintf(stdout, "</span>")
#	define BR() fprintf(stdout, "<br />\n")
#	define startOutput() fprintf(stdout, "<html><body style=\"font-family:monospace\">\n")
#	define endOutput() fprintf(stdout, "<html><body>\n")
#endif

#define blueSilentNobr(X) { setBlue(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define blueSilent(X) { blueSilentNobr(X); BR(); } qt_noop()
#define FLF() { blueSilent(__FILE__ + " " + QString::number(__LINE__) + " " + __FUNCTION__ + "() :"); } qt_noop()
#define blue(X) { FLF(); blueSilent(X); }

#define cyanSilentNobr(X) { setCyan(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define cyanSilent(X) { cyanSilentNobr(X); BR(); } qt_noop()
#define cyan(X) { FLF(); cyanSilent(X); }

#define graySilentNobr(X) { simplePrint(X); fflush(stdout); } qt_noop()
#define graySilent(X) { graySilentNobr(X); BR(); } qt_noop()
#define gray(X) { FLF(); graySilent(X); } qt_noop()
#define grayiSilent(X) { graySilent(#X + " = " + QString::number(X)); } qt_noop()
#define grayvSilent(X) { graySilent(#X + " = " + X); } qt_noop()

#define brownSilentNobr(X) { setBrown(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define brownSilent(X) { brownSilentNobr(X); BR(); } qt_noop()
#define brown(X) { FLF(); brownSilent(X); } qt_noop()

#define yellowSilentNobr(X) {setYellow(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define yellowSilent(X) { yellowSilentNobr(X); BR(); } qt_noop()
#define yellow(X) { FLF(); yellowSilent(X); } qt_noop()

#define redSilentNobr(X) { setRed(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define redSilent(X) { redSilentNobr(X); BR(); } qt_noop()
#define red(X) { FLF(); redSilent(X); } qt_noop()

#define greenSilentNobr(X) { setGreen(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define greenSilent(X) { greenSilentNobr(X); BR(); } qt_noop()
#define green(X) { FLF(); greenSilent(X); } qt_noop()

#define orangeSilentNobr(X) { setOrange(); simplePrint(X); resetColor(); fflush(stdout); } qt_noop()
#define orangeSilent(X) { orangeSilentNobr(X); BR(); } qt_noop()
#define orange(X) { FLF(); orangeSilent(X); } qt_noop()
#define orangeiSilent(X) { orangeSilent(#X + " = " + QString::number(X)); } qt_noop()
#define orangevSilent(X) { orangeSilent(#X + " = " + X); } qt_noop()
#endif

// Qt's Q_ASSERT is removed in release mode, so use this instead
#define Q_ASSERT_FORCE(cond) ((!(cond)) ? qt_assert(#cond,__FILE__,__LINE__) : qt_noop())


#endif // DEBUG_H
