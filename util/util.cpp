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

#include "util.h"

#include <QFile>
#include <QProcess>

#ifdef QT_GUI_LIB
#include <QtGui>
#endif

#include <execinfo.h>
#include <cxxabi.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug.h"

QList<QSharedPointer<QTextStream> > OutputStream::copies;

void writeNumberCompressed(QDataStream &s, qint64 d) {
	bool positive = d > 0;
	if (d < 0) {
		d = -d;
	}
	// Now d >= 0
	if (d == 0) {
		qint8 code = 0;
		s << code;
	} else if (d < (1LL << 7)) {
		qint8 code = positive ? 1 : 21;
		s << code;
		qint8 v = d;
		s << v;
	} else if (d < (1LL << 15)) {
		qint8 code = positive ? 2 : 22;
		s << code;
		qint16 v = d;
		s << v;
	} else if (d < (1LL << 31)) {
		qint8 code = positive ? 3 : 23;
		s << code;
		qint32 v = d;
		s << v;
	} else {
		qint8 code = positive ? 4 : 24;
		s << code;
		qint64 v = d;
		s << v;
	}
}

void writeNumberCompressed(QDataStream &s, qint32 d) {
	qint64 v = d;
	writeNumberCompressed(s, v);
}

void writeNumberCompressed(QDataStream &s, qint16 d) {
	qint64 v = d;
	writeNumberCompressed(s, v);
}

void writeNumberCompressed(QDataStream &s, qint8 d) {
	qint64 v = d;
	writeNumberCompressed(s, v);
}

void readNumberCompressed(QDataStream &s, qint64 &d) {
	qint8 code;
	s >> code;
	if (code == 0) {
		d = 0;
	} else if (code == 1 || code == 21) {
		qint8 v;
		s >> v;
		d = (code <= 20) ? v : -v;
	} else if (code == 2 || code == 22) {
		qint16 v;
		s >> v;
		d = (code <= 20) ? v : -v;
	} else if (code == 3 || code == 23) {
		qint32 v;
		s >> v;
		d = (code <= 20) ? v : -v;
	} else if (code == 4 || code == 24) {
        qint64 v;
		s >> v;
		d = (code <= 20) ? v : -v;
	} else {
		qDebug() << __FILE__ << __LINE__ << "File corrupted";
		Q_ASSERT_FORCE(0);
	}
}

void readNumberCompressed(QDataStream &s, qint32 &d) {
	qint64 v;
	readNumberCompressed(s, v);
	d = v;
}

void readNumberCompressed(QDataStream &s, qint16 &d) {
	qint64 v;
	readNumberCompressed(s, v);
	d = v;
}

void readNumberCompressed(QDataStream &s, qint8 &d) {
	qint64 v;
	readNumberCompressed(s, v);
	d = v;
}

void writeNumberCompressed(QDataStream &s, quint64 u) {
	qint8 code;
	if (u == 0) {
		code = 0;
		s << code;
	} else if (u < (1ULL << 8)) {
		code = 1;
		s << code;
		quint8 v = u;
		s << v;
	} else if (u < (1ULL << 16)) {
		code = 2;
		s << code;
		quint16 v = u;
		s << v;
	} else if (u < (1ULL << 32)) {
		code = 3;
		s << code;
		quint32 v = u;
		s << v;
	} else {
		code = 4;
		s << code;
		quint64 v = u;
		s << v;
	}
}

void writeNumberCompressed(QDataStream &s, quint32 u) {
	quint64 v = u;
	writeNumberCompressed(s, v);
}

void writeNumberCompressed(QDataStream &s, quint16 u) {
	quint64 v = u;
	writeNumberCompressed(s, v);
}

void writeNumberCompressed(QDataStream &s, quint8 u) {
	quint64 v = u;
	writeNumberCompressed(s, v);
}

void readNumberCompressed(QDataStream &s, quint64 &u) {
	qint8 code;
	s >> code;
	if (code == 0) {
		u = 0;
	} else if (code == 1) {
		quint8 v;
		s >> v;
		u = v;
	} else if (code == 2) {
		quint16 v;
		s >> v;
		u = v;
	} else if (code == 3) {
		quint32 v;
		s >> v;
		u = v;
	} else if (code == 4) {
		quint64 v;
		s >> v;
		u = v;
	} else {
		qDebug() << __FILE__ << __LINE__ << "File corrupted";
		Q_ASSERT_FORCE(0);
	}
}

void readNumberCompressed(QDataStream &s, quint32 &u) {
	quint64 v;
	readNumberCompressed(s, v);
	u = v;
}

void readNumberCompressed(QDataStream &s, quint16 &u) {
	quint64 v;
	readNumberCompressed(s, v);
	u = v;
}

void readNumberCompressed(QDataStream &s, quint8 &u) {
	quint64 v;
	readNumberCompressed(s, v);
	u = v;
}

void testReadWriteNumberCompressed() {
	QByteArray buffer;
	{
		QDataStream out(&buffer, QIODevice::WriteOnly);
        writeNumberCompressed(out, qint8(5));
        writeNumberCompressed(out, qint32(-123456789));
        writeNumberCompressed(out, quint8(200));
        writeNumberCompressed(out, qint16(-12));
        writeNumberCompressed(out, quint64(12345));
        writeNumberCompressed(out, quint16(0));
        writeNumberCompressed(out, qint64(-123456789012345LL));
        writeNumberCompressed(out, quint64(50000000000000ULL));
        writeNumberCompressed(out, qint64(0));
        writeNumberCompressed(out, qint32(-279));
        writeNumberCompressed(out, qint16(500));
        writeNumberCompressed(out, qint64(-12));
        writeNumberCompressed(out, quint32(127));
	}
	{
		QDataStream in(&buffer, QIODevice::ReadOnly);
		qint8 i8;
		qint16 i16;
		qint32 i32;
		qint64 i64;
		quint8 u8;
		quint16 u16;
		quint32 u32;
		quint64 u64;
        readNumberCompressed(in, i8); Q_ASSERT_FORCE(i8 == 5);
        readNumberCompressed(in, i32); Q_ASSERT_FORCE(i32 == -123456789);
        readNumberCompressed(in, u8); Q_ASSERT_FORCE(u8 == 200);
        readNumberCompressed(in, i16); Q_ASSERT_FORCE(i16 == -12);
        readNumberCompressed(in, u64); Q_ASSERT_FORCE(u64 == 12345);
        readNumberCompressed(in, u16); Q_ASSERT_FORCE(u16 == 0);
        readNumberCompressed(in, i64); Q_ASSERT_FORCE(i64 == -123456789012345LL);
        readNumberCompressed(in, u64); Q_ASSERT_FORCE(u64 == 50000000000000ULL);
        readNumberCompressed(in, i64); Q_ASSERT_FORCE(i64 == 0);
        readNumberCompressed(in, i32); Q_ASSERT_FORCE(i32 == -279);
        readNumberCompressed(in, i16); Q_ASSERT_FORCE(i16 == 500);
        readNumberCompressed(in, i64); Q_ASSERT_FORCE(i64 == -12);
        readNumberCompressed(in, u32); Q_ASSERT_FORCE(u32 == 127);
		Q_ASSERT_FORCE(in.atEnd() && in.status() == QDataStream::Ok);
	}
}

QString absolutePathFromPath(QString path)
{
	QFileInfo pathInfo(path);
	return pathInfo.canonicalFilePath();
}

QString absoluteDirPathFromFilePath(QString path)
{
	QFileInfo pathInfo(path);
	return pathInfo.canonicalPath();
}

QString parentDirFromDir(QString path) {
	path = absolutePathFromPath(path);
	QStringList tokens = path.split("/", QString::SkipEmptyParts);
	if (tokens.isEmpty()) {
		return "/";
	} else {
		tokens.removeLast();
		return "/" + tokens.join("/");
	}
}

quint64 bPerSec2bPerNanosec(quint64 value)
{
	return value * 1000ULL * 1000ULL * 1000ULL;
}

QString bits2DataSizeWithUnit(quint64 bits)
{
	QList<QString> multipliers = QList<QString>() << "b" << "kb" << "Mb"
												  << "Gb" << "Tb" << "Pb"
												  << "Eb" << "Zb" << "Yb";

	quint64 scale = 1ULL;
	for (int index = 0; index < multipliers.count(); index++) {
		if (bits % (scale * 1000ULL) != 0) {
			if (bits % (scale * 100ULL) == 0 &&
				index < multipliers.count() - 1) {
				// 0.1kb
				return QString("%1.%2%3")
						.arg(bits / (scale * 1000ULL))
						.arg((bits % (scale * 1000ULL)) / (scale * 100ULL))
						.arg(multipliers[index+1]);
			}
			// 123b
			return QString("%1%2").arg(bits / scale).arg(multipliers[index]);
		}
		if (scale > ULLONG_MAX / 1000ULL ||
			index >= multipliers.count() - 1) {
			return QString("%1%2").arg(bits / scale).arg(multipliers[index]);
		}
		scale *= 1000ULL;
	}
	return QString("%1%2").arg(bits).arg(multipliers[0]);
}

// Converts a number followed by [multiplier] and unit (b or B)
// to the corresponding number of bits.
// E.g. 200kB -> 1600000 bits
quint64 dataSizeWithUnit2bits(QString s, bool *ok)
{
	// [0-9]+[kmgtpezyKMGTPEZY]?[bB]
	// k K -> 10^3 = 1000
	// m M -> 10^6
	// g G -> 10^9
	// t T -> 10^12
	// p P -> 10^15
	// e E -> 10^18
	// z Z -> 10^21
	// y Y -> 10^24
	// b -> bits
	// B -> bytes

	if (ok) {
		*ok = false;
	}

	QRegExp rxAll("(([0-9]+)\\.?([0-9]+)?)([kmgtpezyKMGTPEZY]?)([bB])");
	if (!rxAll.exactMatch(s)) {
		if (ok) {
			*ok = false;
		}
		return 0;
	}

	QStringList tokens = rxAll.capturedTexts().mid(1);
	tokens.takeFirst();
	Q_ASSERT_FORCE(tokens.count() == 4);

	bool localOk;
	quint64 value = tokens.at(0).toLongLong(&localOk);
	if (!localOk) {
		return 0;
	}
	// prepend a "1" so that we do not lose the frontal zeroes
	quint64 fractionalPart = (QString("1") + tokens.at(1)).toLongLong(&localOk);
	if (!localOk) {
		return 0;
	}
	quint64 fractionalExp = 0;
	for (quint64 tmp = fractionalPart; tmp > 1; tmp /= 10ULL, fractionalExp++);
	for (quint64 tmpExp = 0, tmp = 1; tmpExp <= fractionalExp; tmpExp++, tmp *= 10ULL) {
		if (tmpExp == fractionalExp) {
			fractionalPart -= tmp;
		}
	}
	// at this point: result = (value + fractionalPart * 10 ^ -fractionalExp) * multiplier

	QString multiplierStr = tokens.at(2);
	quint64 multiplier = 1ULL;
	if (multiplierStr == "k" || multiplierStr == "K") {
		multiplier *= 1000ULL;
	} else if (multiplierStr == "m" || multiplierStr == "M") {
		multiplier *= 1000ULL * 1000ULL;
	} else if (multiplierStr == "g" || multiplierStr == "G") {
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
	} else if (multiplierStr == "t" || multiplierStr == "T") {
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL;
	} else if (multiplierStr == "p" || multiplierStr == "P") {
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL * 1000ULL;
	} else if (multiplierStr == "e" || multiplierStr == "E") {
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
	} else if (multiplierStr == "z" || multiplierStr == "Z") {
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL;
	} else if (multiplierStr == "y" || multiplierStr == "Y") {
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL * 1000ULL * 1000ULL;
		multiplier *= 1000ULL * 1000ULL;
	}
	value *= multiplier;
	fractionalPart *= multiplier;
	for (; fractionalExp > 0; fractionalExp--, fractionalPart /= 10ULL) ;
	value += fractionalPart;

	QString unit = tokens.at(3);
	if (unit == "B") {
		value *= 8ULL;
	}

	if (ok) {
		*ok = true;
	}
	return value;
}

void test_dataSizeWithUnit2bits()
{
	Q_ASSERT_FORCE(dataSizeWithUnit2bits("123.05Mb") == 123050000);
	Q_ASSERT_FORCE(dataSizeWithUnit2bits("123.5Mb") == 123500000);
	Q_ASSERT_FORCE(dataSizeWithUnit2bits("123.0Mb") == 123000000);
	Q_ASSERT_FORCE(dataSizeWithUnit2bits("123.Mb") == 123000000);
	Q_ASSERT_FORCE(dataSizeWithUnit2bits("123Mb") == 123000000);
}

void test_bits2DataSizeWithUnit()
{
	qDebug() << bits2DataSizeWithUnit(123050000);
	Q_ASSERT_FORCE(bits2DataSizeWithUnit(123050000) == "123050kb");
	Q_ASSERT_FORCE(bits2DataSizeWithUnit(123500000) == "123.5Mb");
	Q_ASSERT_FORCE(bits2DataSizeWithUnit(123000000) == "123Mb");
	Q_ASSERT_FORCE(bits2DataSizeWithUnit(1200) == "1.2kb");
	for (quint64 value = 12ULL; value < ULLONG_MAX / 100ULL; value *= 10ULL) {
		QString s = bits2DataSizeWithUnit(value);
		bool ok;
		quint64 value2 = dataSizeWithUnit2bits(s, &ok);
		Q_ASSERT_FORCE(ok);
		Q_ASSERT_FORCE(value == value2);
	}
}

int getNumCoresLinux()
{
	QFile file("/proc/cpuinfo");
	if (!file.open(QFile::ReadOnly)) {
		return QThread::idealThreadCount();
	}
	QTextStream stream(&file);

	QHash<int, int> cpuCores;
	int currectCpuId = -1;
	while (1) {
		QString line = stream.readLine();
		if (line.isNull())
			break;
		if (line.startsWith("physical id")) {
			bool ok;
			int v = line.split(":").last().toInt(&ok);
			if (ok)
				currectCpuId = v;
		} else if (line.startsWith("cpu cores")) {
			bool ok;
			int v = line.split(":").last().toInt(&ok);
			if (ok) {
				if (currectCpuId >= 0) {
					cpuCores[currectCpuId] = v;
				}
			}
		}
	}
	int total = 0;
	foreach (int cores, cpuCores.values()) {
		total += cores;
	}
	return total;
}

qreal median(QList<qreal> list)
{
	if (list.isEmpty())
		return 0;
	qSort(list);
	if (list.count() % 2 == 1) {
		return list.at(list.count() / 2);
	} else {
		return (list.at(list.count() / 2 - 1) + list.at(list.count() / 2)) / 2.0;
	}
}

qreal sum(QList<qreal> list)
{
    qreal result = 0;
    foreach (qreal e, list) {
        result += e;
    }
    return result;
}

qreal sphereDistance(qreal lat1, qreal long1, qreal lat2, qreal long2, qreal radius)
{
	qreal p1 = cos(M_PI/180.0*(long1-long2));
	qreal p2 = cos(M_PI/180.0*(lat1-lat2));
	qreal p3 = cos(M_PI/180.0*(lat1+lat2));
	qreal d = acos(((p1*(p2+p3))+(p2-p3))/2) * radius;
	if (qIsNaN(d)) {
		qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "distance is NaN!";
	}
	return d;
}

qreal effectiveRank_Vetterli(QList<qreal> svd)
{
	qreal sigma1Norm = 0;
	foreach (qreal s, svd) {
		sigma1Norm += fabs(s);
	}
	if (sigma1Norm == 0)
		return 0;
	QList<qreal> p = svd;
	for (int i = 0; i < p.count(); i++) {
		p[i] = p[i] / sigma1Norm;
	}

	qreal entropy = 0;
	foreach (qreal v, p) {
		entropy += - v * log(v);
	}

	qreal erank = exp(entropy);
	return erank;
}

QString timeToString(quint64 value)
{
	if (value == 0)
		return "0ns";
	if (value == ULONG_LONG_MAX)
		return "Infinity";

	quint64 s, ms, us, ns;

	s = (value / 1000ULL / 1000ULL / 1000ULL) % 60ULL;
	ms = (value / 1000ULL / 1000ULL) % 1000ULL;
	us = (value / 1000ULL) % 1000ULL;
	ns = value % 1000ULL;

	Q_UNUSED(s);

	if (ns) {
		return QString::number(value) + "ns";
	} else if (us) {
		return QString::number(value / 1000ULL) + "us";
	} else if (ms) {
		return QString::number(value / 1000ULL / 1000ULL) + "ms";
	} else {
		return QString::number(value / 1000ULL / 1000ULL / 1000ULL) + "s";
	}
}

QString toString(const QBitArray &b)
{
	QString result = "QBitArray(";
	for (int i = b.count() - 1; i >= 0; i--) {
		if (b.testBit(i)) {
			result += "1";
		} else {
			result += "0";
		}
	}
	result += ")";
	return result;
}

// generate number uniformely distributed between min..max inclusively by rejection sampling
int randInt(int min, int max)
{
	if (max - min >= RAND_MAX) {
		// you are in trouble
		return min + rand();
	}
	if (max <= min) {
		return min;
	}

	// this is the formula but it might overflow
	// int NEW_RAND_MAX = RAND_MAX - (RAND_MAX + 1) % (max-min+1);
	// so rewrite it safely like this:
	int NEW_RAND_MAX = RAND_MAX - (RAND_MAX % (max-min+1) + 1) % (max-min+1);

	while (1) {
		int x = rand();
		if (x > NEW_RAND_MAX)
			continue;
		// divide safely in case NEW_RAND_MAX is MAX_INT
		// (a+1)/b = a/b + (a%b == b-1 ? 1 : 0)
		// x = x / ((NEW_RAND_MAX+1)/(max-min+1));
		x = x / (NEW_RAND_MAX/(max-min+1) + ((NEW_RAND_MAX % (max-min+1) == (max-min)) ? 1 : 0) );
		return min + x;
	}
	return 0; // never happens
}

void testRandIntRange(int max)
{
	QMap<int, int> distribution;
	for (int i = 0; i < 10000; i++) {
		int x = randInt(0, max);
		distribution[x] = distribution[x] + 1;
	}
	qDebug() << "Test for range 0 -" << max;
	foreach (int x, distribution.uniqueKeys()) {
		qDebug() << "Value:" << x << "Frequency:" << distribution[x];
	}
	qDebug() << "";
}

void testRandInt()
{
	for (int i = 0; i < 10; i++) {
		testRandIntRange(i);
	}
}

int randDist(QList<qreal> densities)
{
	if (densities.isEmpty())
		return 0;
	// transform the densities into cumulative sum
	qreal sum = 0;
	for (int i = 0; i < densities.count(); i++) {
		sum += densities[i];
		densities[i] = sum;
	}

	// generate random number between 0 and sum
	qreal x = frandex() * sum;
	int i;
	for (i = 0; i < densities.count(); i++) {
		if (densities[i] > x)
			break;
	}
	return i;
}

void testRandDist()
{
	QList<qreal> pdf = QList<qreal>() << 2.0 << 1.0 << 1.0 << 95.0 << 1.0;

	QMap<int, int> distribution;
	for (int i = 0; i < 100000; i++) {
		int x = randDist(pdf);
		distribution[x] = distribution[x] + 1;
	}
	qDebug() << "Test for densities" << pdf;
	foreach (int x, distribution.uniqueKeys()) {
		qDebug() << "Value:" << x << "Frequency:" << distribution[x];
	}
	qDebug() << "";
}

bool fileInDirectory(QString fileName, QString dirName)
{
//	qDebug() << fileName;
	int p = fileName.lastIndexOf(QDir::separator());
	QString dir1 = p < 0 ? "." : fileName.mid(0, p);
//	qDebug() << dir1;
//	qDebug() << QDir::cleanPath(dir1);
//	qDebug() << QDir(QDir::cleanPath(dir1)).canonicalPath();
//	qDebug() << dirName;
//	qDebug() << QDir::cleanPath(dirName);
//	qDebug() << QDir(QDir::cleanPath(dirName)).canonicalPath();
	return QDir(QDir::cleanPath(dir1)).canonicalPath() == QDir(QDir::cleanPath(dirName)).canonicalPath();
}

void printBacktrace()
{
	void *frames[30];
	size_t frame_count;

	// get void*'s for all entries on the stack
	frame_count = backtrace(frames, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Backtrace:\n");
	backtrace_symbols_fd(frames, frame_count, 2); // 2 is stderr
}
// Credit: Christopher Woods
// http://lists.trolltech.com/qt-interest/2006-07/msg00469.html
QStringList getBacktrace()
{
	QStringList result;
	void *frames[128];
	int stack_size = backtrace(frames, sizeof(frames) / sizeof(void *));
	char **symbols = backtrace_symbols(frames, stack_size);

	//need to extract the symbol from the output of 'backtrace_symbols'

	//a typical output from backtrace_symbols will look like;
	//a.out(_ZNK1A12getBackTraceEv+0x12) [0x804ad36]

	// This needs to be split into;
	//  (1) The program or library containing the symbol (a.out)
	//  (2) The symbol itself (_ZNK1A12getBackTraceEv)
	//  (3) The offset? +0x12
	//  (4) The symbol address ([0x804ad36])

	// This is achieved by the regexp below

	//              (unit )  (symbol)   (offset)          (address)
	QRegExp regexp("([^(]+)\\(([^)^+]+)(\\+[^)]+)\\)\\s(\\[[^]]+\\])");

	for (int i = stack_size - 1; i >= 0; --i) {
		if (regexp.indexIn(symbols[i]) != -1) {
			//get the library or app that contains this symbol
			QString unit = regexp.cap(1);
			//get the symbol
			QString symbol = regexp.cap(2);
			//get the offset
			QString offset = regexp.cap(3); Q_UNUSED(offset);
			//get the address
			QString address = regexp.cap(4);

			//now try and demangle the symbol
			int stat;
			char *demangled = abi::__cxa_demangle(qPrintable(symbol), 0, 0, &stat);

			if (demangled) {
				symbol = demangled;
				delete demangled;
			}

			//put this all together
			result << QString("%1: %2  %3").arg(unit, symbol, address);
		} else {
			//I don't recognise this string - just add the raw
			//string to the backtrace
			result << symbols[i];
		}
	}
	return result;
}

QString shortString(QString s, int length)
{
	if (s.length() <= length)
		return s;

	return s.mid(0, length) + " ...";
}

bool saveFile(QString name, QString content)
{
	QFile file(name);
	if (!file.open(QIODevice::WriteOnly)) {
		fdbg << "Could not open file in write mode, file name:" << name;
		return false;
	}

	QByteArray data = content.toAscii();

	while (data.length() > 0) {
		int bytes = file.write(data);
		if (bytes < 0) {
			fdbg << "Write error, file name:" << name;
			return false;
		}
		data = data.mid(bytes);
	}

	file.close();

	return true;
}

bool readFile(QString name, QString &content, bool silent)
{
	QFile file(name);
	if (!file.open(QIODevice::ReadOnly)) {
		if (!silent) {
			fdbg << "Could not open file in read mode, file name:" << name;
		}
		return false;
	}

	QByteArray data = file.readAll();

	if (data.isEmpty() && file.size() > 0)
		return false;

	content = data;

	file.close();

	return true;
}

bool saveFile(QString name, QByteArray content)
{
	QFile file(name);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		fdbg << "Could not open file in write mode, file name:" << name;
		return false;
	}

	while (!content.isEmpty()) {
		qint64 count = file.write(content);
		if (count < 0) {
			fdbg << "Write error, file name:" << name;
			return false;
		}
		content = content.mid(count);
	}

	file.close();

	return true;
}

bool saveTextArchive(QString name, const QStringList &fileNames, const QStringList &files)
{
	Q_ASSERT_FORCE(fileNames.count() == files.count());

	QFile out(name);
	if (!out.open(QIODevice::WriteOnly)) {
		fdbg << "Could not open file" << name << "in write mode";
		return false;
	}

	for (int i = 0; i < files.size(); ++i) {
		QString fileName = fileNames.at(i);

		if (i % 1000 == 0) {
			//qDebug() << "Progress:" << i << (i * 100.0 / files.size()) << "%";
		}

		QString text = files.at(i);
		int length = text.toAscii().length();
		text.prepend(fileName + " " + QString::number(length) + "\n");

		QByteArray data = text.toAscii();
		while (data.length() > 0) {
			int bytes = out.write(data);
			if (bytes < 0) {
				fdbg << "Write error";
				return false;
			}
			data = data.mid(bytes);
		}
	}

	out.close();
	return true;
}

bool readTextArchive(QString name, QStringList &fileNames, QStringList &files, bool silent)
{
	fileNames.clear();
	files.clear();

	QFile in(name);
	if (!in.open(QIODevice::ReadOnly)) {
		if (!silent) {
			fdbg << "Could not open file" << name << "in read mode";
		}
		return false;
	}

	QByteArray content = in.readAll();

	if (content.isEmpty() && in.size() > 0)
		return false;

	in.close();

	for (int i = 0; i < content.size();) {
		QByteArray line;
		while (i < content.size()) {
			if (content.at(i) == '\n') {
				i++;
				break;
			} else {
				line.append(content.at(i));
				i++;
			}
		}

		if (i >= content.size()) {
			fdbg << "Archive" << name << "damaged at byte" << i;
			return false;
		}

		QString s(line);
		QStringList tokens = s.split(' ');
		if (tokens.count() < 2) {
			fdbg << "Archive" << name << "damaged at byte" << i;
			return false;
		}
		QString fileSize = tokens.last();
		QString fileName = s.mid(0, s.length() - fileSize.length());
		bool ok;
		int size = fileSize.toInt(&ok);
		if (!ok) {
			fdbg << "Archive" << name << "damaged at byte" << i;
			return false;
		}

		QByteArray data;
		for (; size > 0 && i < content.size(); size--, i++) {
			data.append(content.at(i));
		}
		QString fileContent = data;

		fileNames << fileName;
		files << fileContent;
	}
	return true;
}

bool keyValHigherEqual(const KeyVal &s1, const KeyVal &s2)
{
	return s1.val >= s2.val;
}

bool randomLessThan(const QString &, const QString &)
{
	return rand() < (RAND_MAX >> 1);
}

bool runProcess(QString name, QStringList args, bool silent)
{
	QProcess p;
	p.setProcessChannelMode(QProcess::ForwardedChannels);
	p.start(name, args);
	if (!p.waitForStarted(-1)) {
		if (!silent) qDebug() << "ERROR: Could not start:" << name;
		return false;
	}

	if (!p.waitForFinished(-1)) {
		if (!silent) qDebug() << "ERROR:" << name << "failed (error or timeout).";
		return false;
	}
	return true;
}

bool runProcess(QString name, QStringList args, QString &output, int timeoutsec, bool silent, int *exitCode)
{
	output.clear();

	QProcess p;
	p.setProcessChannelMode(QProcess::MergedChannels);
	p.start(name, args);
    if (!p.waitForStarted(timeoutsec == -1 ? 30 * 1000 : timeoutsec * 1000)) {
		if (!silent) qDebug() << "ERROR: Could not start:" << name;
		return false;
	}

	if (!p.waitForFinished(timeoutsec == -1 ? -1 : timeoutsec * 1000)) {
		if (!silent) qDebug() << "ERROR:" << name << "failed (error or timeout). Command:" << name << args;
		p.kill();
		p.waitForFinished(timeoutsec * 1000 < 5000 ? timeoutsec * 1000 : 5000);
		output = p.readAllStandardOutput();
		return false;
	}
	output = p.readAllStandardOutput();

	if (exitCode)
		*exitCode = p.exitCode();
	return true;
}

bool runCommand(QString command, QStringList args, QString description)
{
	QString output;
	return runCommand(command, args, description, true, output);
}

bool runCommand(QString command, QStringList args, QString description, bool showOutput, QString &output)
{
	int exitCode;
	bool runOk;

	if (!description.isEmpty()) {
		qDebug() << QString("Running: %1...").arg(description);
	}

	qDebug() << command + " " + args.join(" ");
	runOk = runProcess(command, args, output, -1, false, &exitCode);
	if (output.endsWith('\n'))
		output = output.mid(0, output.length() - 1);
	if (showOutput && !output.isEmpty()) {
		qDebug() << output;
	}
	if (!runOk || exitCode) {
		qError() << "FAIL.";
		return false;
	}

	emit qDebug() << "OK.";
	return true;
}

void initRand()
{
	FILE *frand = fopen("/dev/random", "rt");
	int seed;
	int unused = 0;
	unused += fscanf(frand, "%c", ((char*)&seed));
	unused += fscanf(frand, "%c", ((char*)&seed)+1);
	unused += fscanf(frand, "%c", ((char*)&seed)+2);
	unused += fscanf(frand, "%c", ((char*)&seed)+3);
	fclose(frand);

	srand(seed);
}

bool stopSerial;
void serialSigInt(int )
{
	stopSerial = true;
}


#ifdef QT_NETWORK_LIB
#include <QHostInfo>
QString resolveDNSName(QString hostname)
{
	QHostInfo hInfo = QHostInfo::fromName(hostname);
	if (hInfo.addresses().count() > 0) {
		if (hInfo.addresses().count() == 0) {
			return QString();
		}
		return hInfo.addresses()[0].toString();
	}
	return QString();
}

QString resolveDNSReverse(QString ip)
{
	if (!ipnumOk(ip)) {
		qDebug() << __FILE__ << __LINE__ << ip;
		exit(EXIT_FAILURE);
	}
	QHostInfo hInfo = QHostInfo::fromName(ip);
	return hInfo.hostName();
}

QMap<QString, QString> reverseDNSCache;
QMutex reverseDNSCacheMutex;
#define REV_DNS_CACHE "/home/ovi/src/tracegraph/geoipdata/revdns.cache"
QString resolveDNSReverseCached(QString ip, bool silent)
{
	if (!ipnumOk(ip)) {
		qDebug() << __FILE__ << __LINE__ << ip;
		exit(EXIT_FAILURE);
	}

	reverseDNSCacheMutex.lock();
	if (reverseDNSCache.isEmpty()) {
		QFile file(REV_DNS_CACHE);
		if (file.open(QIODevice::ReadOnly)) {
			QDataStream in(&file);
			in >> reverseDNSCache;
		}
	}
	if (reverseDNSCache.contains(ip)) {
		QString dns = reverseDNSCache[ip];
		reverseDNSCacheMutex.unlock();
		return dns;
	}
	reverseDNSCacheMutex.unlock();

	if (!silent) {
		qDebug() << "Querying reverse DNS for" << ip << "...";
	}
	QHostInfo hInfo = QHostInfo::fromName(ip);
	if (!silent) {
		if (hInfo.hostName().isEmpty()) {
			qDebug() << "failed";
		} else {
			qDebug() << "found " + hInfo.hostName();
		}
	}
	if (!hInfo.hostName().isEmpty()) {
		reverseDNSCacheMutex.lock();
		reverseDNSCache[ip] = hInfo.hostName();
		QFile file(REV_DNS_CACHE);
		if (file.open(QIODevice::WriteOnly)) {
			QDataStream out(&file);
			out << reverseDNSCache;
		}
		reverseDNSCacheMutex.unlock();
	}
	return hInfo.hostName();
}

#endif

#define CHECK(cond) if (!(cond)) return false;

// Returns true if ip is a valid IPv4 address in a.b.c.d format
bool ipnumOk(QString address)
{
	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	CHECK(bytes.count() == 4);

	unsigned a, b, c, d;
	bool ok;
	a = bytes[0].toInt(&ok); CHECK(ok); CHECK(a <= 255);
	b = bytes[1].toInt(&ok); CHECK(ok); CHECK(b <= 255);
	c = bytes[2].toInt(&ok); CHECK(ok); CHECK(c <= 255);
	d = bytes[3].toInt(&ok); CHECK(ok); CHECK(d <= 255);

	return true;
}

QString ipnumToString(ipnum ip)
{
	ipnum a, b, c, d;
	d = ip & 0xFFU;
	ip >>= 8;
	c = ip & 0xFFU;
	ip >>= 8;
	b = ip & 0xFFU;
	ip >>= 8;
	a = ip & 0xFFU;
	return QString::number(a) + '.' + QString::number(b) + '.' +
			QString::number(c) + '.' + QString::number(d);
}

ipnum stringToIpnum(QString address)
{
	ipnum ip;

	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	Q_ASSERT_FORCE(bytes.count() == 4);

	ipnum a, b, c, d;
	bool ok;
	a = bytes[0].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(a <= 255);
	b = bytes[1].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(b <= 255);
	c = bytes[2].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(c <= 255);
	d = bytes[3].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(d <= 255);

	ip = (a << 24) | (b << 16) | (c << 8) | d;
	return ip;
}

QString blockToString(Block &block)
{
	return ipnumToString(block.first) + " - " + ipnumToString(block.second);
}

#define CHECK(cond) if (!(cond)) return false;

// Returns true if prefix is a valid IPv4 route in a.b.c.d/mask format
// also accepts a.b.c/mask, a.b/mask, a/mask
bool prefixOk(QString prefix)
{

	QStringList tokens = prefix.split('/', QString::SkipEmptyParts);
	CHECK(tokens.count() == 2);

	QString address = tokens[0];
	QString mask = tokens[1];
	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	CHECK(bytes.count() >= 1);
	CHECK(bytes.count() <= 4);

	ipnum a, b, c, d, m;
	bool ok;
	a = b = c = d = 0;
	if (bytes.count() >= 1) {
		a = bytes[0].toInt(&ok); CHECK(ok); CHECK(a <= 255);
	}
	if (bytes.count() >= 2) {
		b = bytes[1].toInt(&ok); CHECK(ok); CHECK(b <= 255);
	}
	if (bytes.count() >= 3) {
		c = bytes[2].toInt(&ok); CHECK(ok); CHECK(c <= 255);
	}
	if (bytes.count() >= 4) {
		d = bytes[3].toInt(&ok); CHECK(ok); CHECK(d <= 255);
	}
	m = mask.toInt(&ok); CHECK(ok);
	CHECK(m <= 32);

	return true;
}

// Converts a.b.c.d/mask into a block (first address, last address)
Block prefixToBlock(QString prefix)
{
	/*
 // masks generation:
 ipnum ip = 0xffFFffFFU, m = 0;
 while (m <= 32) {
  printf("%uU,\n", ip);
  m++;
  ip >>= 1;
 }
 */
	static ipnum masks[33] = {4294967295U,
							  2147483647U,
							  1073741823U,
							  536870911U,
							  268435455U,
							  134217727U,
							  67108863U,
							  33554431U,
							  16777215U,
							  8388607U,
							  4194303U,
							  2097151U,
							  1048575U,
							  524287U,
							  262143U,
							  131071U,
							  65535U,
							  32767U,
							  16383U,
							  8191U,
							  4095U,
							  2047U,
							  1023U,
							  511U,
							  255U,
							  127U,
							  63U,
							  31U,
							  15U,
							  7U,
							  3U,
							  1U,
							  0U
							 };
	ipnum first, last;

	QStringList tokens = prefix.split('/', QString::SkipEmptyParts);
	Q_ASSERT_FORCE(tokens.count() == 2);

	QString address = tokens[0];
	QString mask = tokens[1];
	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	Q_ASSERT_FORCE(bytes.count() >= 1);
	Q_ASSERT_FORCE(bytes.count() <= 4);

	ipnum a, b, c, d, m;
	bool ok;
	a = b = c = d = 0;
	if (bytes.count() >= 1) {
		a = bytes[0].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(a <= 255);
	}
	if (bytes.count() >= 2) {
		b = bytes[1].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(b <= 255);
	}
	if (bytes.count() >= 3) {
		c = bytes[2].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(c <= 255);
	}
	if (bytes.count() >= 4) {
		d = bytes[3].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(d <= 255);
	}
	m = mask.toInt(&ok); Q_ASSERT_FORCE(ok);
	Q_ASSERT_FORCE(m <= 32);

	first = (a << 24) | (b << 16) | (c << 8) | d;
	first &= ~masks[m];
	last = first | masks[m];
	return Block (first, last);
}

QString downloadFile(QString webAddress)
{
	QString output;
	qDebug() << ("Downloading...");
	if (!runProcess("wget", QStringList() << "-q" << "-O" << "-" << webAddress, output, -1)) {
		qDebug() << ("wget failed.");
		return QString();
	}
	qDebug() << (" done.");
	return output;
}

QString getMyIp()
{
	QString myip;
	qDebug() << ("Finding my IP address...");
	if (!runProcess("wget", QStringList() << "-q" << "-O" << "-" << "http://www.interfete-web-club.com/demo/php/ip.php", myip, 30)) {
		qDebug() << ("wget failed.");
		return QString();
	}
	if (!ipnumOk(myip)) {
		qDebug() << ("received garbage: " + myip);
		return QString();
	}
	qDebug() << (myip);
	return myip;
}

QString getMyIface()
{
	QString iface;
	int metric = 99999;
	qDebug() << ("Finding the default interface...");
	QString routes;
	if (!runProcess("sh", QStringList() << "-c" << "/sbin/route -n | grep \"^0.0.0.0\"", routes, 30)) {
		qDebug() << ("failed.");
		return QString();
	}
	QStringList lines = routes.split('\n', QString::SkipEmptyParts);
	foreach (QString line, lines) {
		QString newIface = line.section(" ", -1, -1, QString::SectionSkipEmpty).trimmed();
		bool ok;
		int newMetric = line.section(" ", -4, -4, QString::SectionSkipEmpty).trimmed().toInt(&ok);
		Q_ASSERT_FORCE(ok);
		if (iface.isEmpty() || newMetric < metric) {
			iface = newIface;
			metric = newMetric;
		}
	}
	if (iface.isEmpty()) {
		qDebug() << ("failed.");
		return QString();
	}
	qDebug() << (iface);
	return iface;
}

QString getIfaceIp(QString iface)
{
	qDebug() << ("Finding IP for " + iface + "...");
	QString output;
	if (!runProcess("/sbin/ifconfig", QStringList() << iface, output, 30)) {
		qDebug() << ("failed.");
		return QString();
	}
	QStringList lines = output.split('\n', QString::SkipEmptyParts);
	QString ip;
	foreach (QString line, lines) {
		ip = line.section("inet addr:", 1).section("Bcast", 0, 0).trimmed();
		if (!ipnumOk(ip))
			ip = "";
		else
			break;
	}
	if (ip.isEmpty()) {
		qDebug() << ("failed.");
		return QString();
	}
	qDebug() << (ip);
	return ip;
}

void makePrintable(QString &s)
{
	s.replace(QRegExp("[^\x20-\x7f]"), "?");
}

// int LevenshteinDistance(char s[1..m], char t[1..n])
int levenshteinDistance(QString s, QString t, Qt::CaseSensitivity cs)
{
	if (cs == Qt::CaseInsensitive) {
		s = s.toUpper();
		t = t.toUpper();
	}

	// d is a table with m+1 rows and n+1 columns
	int m = s.length();
	int n = t.length();

	// adapt for indexing from 1
	s.prepend(' ');
	t.prepend(' ');

	QVarLengthArray<int, 1024> d1(n + 1);
	QVarLengthArray<int, 1024> d2(n + 1);

	int i, j;

	for (j = 0; j <= n; j++)
		d1[j] = j;    // insertion
	d2[0] = 0;

	for (i = 1; i <= m; i++) {
		for (j = 1; j <= n; j++) {
			if (s[i] == t[j]) {
				d2[j] = d1[j-1];
			} else {
				int v;
				v = d1[j] + 1;      // deletion
				d2[j] = v;
				v = d2[j-1] + 1;    // insertion
				if (v < d2[j])
					d2[j] = v;
				v = d1[j-1] + 1;    // substitution
				if (v < d2[j])
					d2[j] = v;
			}
		}
		for (j = 1; j <= n; j++)
			d1[j] = d2[j];
	}

	return d1[n];
}

QString longestCommonSubstring(QString str1, QString str2, Qt::CaseSensitivity cs)
{
	if (str1.isEmpty() || str2.isEmpty()) {
		return 0;
	}

	QString str1Orig = str1;
	QString str2Orig = str2;

	if (cs == Qt::CaseInsensitive) {
		str1 = str1.toUpper();
		str2 = str2.toUpper();
	}

	QVarLengthArray<int, 1024> curr(str2.size());
	QVarLengthArray<int, 1024> prev(str2.size());

	int maxSubstr = 0;
	int endSubstr = 0;
	for (int i = 0; i < str1.size(); ++i) {
		for (int j = 0; j < str2.size(); ++j) {
			if (str1[i] != str2[j]) {
				curr[j] = 0;
			} else {
				if (i == 0 || j == 0) {
					curr[j] = 1;
				} else {
					curr[j] = 1 + prev[j-1];
				}
				if (maxSubstr < curr[j]) {
					maxSubstr = curr[j];
					endSubstr = j;
				}
			}
		}
		for (int j = 0; j < str2.size(); ++j)
			prev[j] = curr[j];
	}
	return str2Orig.mid(endSubstr - maxSubstr + 1, maxSubstr);
}

#ifndef QT_GUI_LIB
QColor::QColor(int r, int g, int b, int a) : r(r), g(g), b(b), a(a) {}
int QColor::red() { return r; }
int QColor::green() { return g; }
int QColor::blue() { return b; }
int QColor::alpha() { return a; }
QColor QColor::fromRgb(int r, int g, int b, int a) { return QColor(r, g, b, a); }
#endif

#ifdef QT_GUI_LIB
bool setCurrentItem(QComboBox *box, QString text)
{
	for (int i = 0; i < box->count(); i++) {
		if (box->itemText(i) == text) {
			box->setCurrentIndex(i);
            return true;
		}
	}
    return false;
}
#endif

QString HTMLWriter::color2css(QColor c)
{
	return QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

void HTMLWriter::beginHtml()
{
	html += "<!DOCTYPE html>"
			"<html><head><style type='text/css'>"
			"table, th, td {border: 1px solid black; padding: 6px;}table{border-collapse:collapse;text-align:center;}"
			"h1 {border-top: dashed 2px #AAA; padding-top: 0.5em;}"
			"h2 {border-top: dashed 1px #AAA; padding-top: 0.5em;}"
			"th {font-weight:normal;}"
			"body {font-size: small;font-family: arial,sans-serif;}"
			"</style>"
			"<script type='text/javascript' src='mootools-core-1.4.5-full-compat.js'></script>"
			"<script type='text/javascript' src='dynamic-table-of-contents.js'></script>"
			"</head><body>\n";
}

void HTMLWriter::endHtml()
{
	html += "</body></html>\n";
}

void HTMLWriter::writeHeading1(QString s)
{
	html += QString("<h1>%1</h1>\n").arg(s);
}

void HTMLWriter::writeHeading2(QString s)
{
	html += QString("<h2>%1</h2>\n").arg(s);
}

void HTMLWriter::writeHeading3(QString s)
{
	html += QString("<h3>%1</h3>\n").arg(s);
}

void HTMLWriter::writeHeading4(QString s)
{
	html += QString("<h4>%1</h4>\n").arg(s);
}

void HTMLWriter::writeText(QString s)
{
	html += QString("<p>%1</p>\n").arg(s);
}

void HTMLWriter::beginTable(QString style)
{
	html += QString("<table style='%1'>\n").arg(style);
}

void HTMLWriter::endTable()
{
	html += "</table>\n";
}

void HTMLWriter::beginTableHeader()
{
	html += "<tr>\n";
}

void HTMLWriter::endTableHeader()
{
	html += "</tr>\n";
}

void HTMLWriter::writeTableHeaderCell(QString s, QColor textColor, QColor bgColor, QString tooltip)
{
	QString style = QString("color:%1;background-color:%2;").arg(color2css(textColor)).arg(color2css(bgColor));
	html += QString("<th style='%1' title='%3'>%2</th>").arg(style).arg(s).arg(tooltip);
}

void HTMLWriter::beginTableRow()
{
	html += "<tr>\n";
}

void HTMLWriter::endTableRow()
{
	html += "</tr>\n";
}

void HTMLWriter::writeTableCell(QString s, QColor textColor, QColor bgColor, QString tooltip)
{
	QString style = QString("color:%1;background-color:%2;").arg(color2css(textColor)).arg(color2css(bgColor));
	html += QString("<td style='%1' title='%3'>%2</td>").arg(style).arg(s).arg(tooltip);
}

bool HTMLWriter::save(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
		qDebug() << __FILE__ << __LINE__ << "Could not write file" << file.fileName();
		return false;
	}
	QTextStream out(&file);
	out << html;
	return true;
}

QString withMultiplierSuffix(qreal x)
{
	if (x < 1.0e-24) {
		// very small
		return withCommasStr(x);
	} else if (x < 1.0e-21) {
		// yocto
		return withCommasStr(x * 1.0e24) + "y";
	} else if (x < 1.0e-18) {
		// zepto
		return withCommasStr(x * 1.0e21) + "z";
	} else if (x < 1.0e-15) {
		// atto
		return withCommasStr(x * 1.0e18) + "a";
	} else if (x < 1.0e-12) {
		// femto
		return withCommasStr(x * 1.0e15) + "f";
	} else if (x < 1.0e-9) {
		// pico
		return withCommasStr(x * 1.0e12) + "p";
	} else if (x < 1.0e-6) {
		// nano
		return withCommasStr(x * 1.0e9) + "n";
	} else if (x < 1.0e-3) {
		// micro
		return withCommasStr(x * 1.0e6) + "u";
	} else if (x < 1.0e0) {
		// milli
		return withCommasStr(x * 1.0e3) + "m";
	} else if (x < 1.0e3) {
		// no suffix
		return withCommasStr(x * 1.0e0);
	} else if (x < 1.0e6) {
		// kilo
		return withCommasStr(x * 1.0e-3) + "k";
	} else if (x < 1.0e9) {
		// mega
		return withCommasStr(x * 1.0e-6) + "M";
	} else if (x < 1.0e12) {
		// giga
		return withCommasStr(x * 1.0e-9) + "G";
	} else if (x < 1.0e15) {
		// tera
		return withCommasStr(x * 1.0e-12) + "T";
	} else if (x < 1.0e18) {
		// peta
		return withCommasStr(x * 1.0e-15) + "P";
	} else if (x < 1.0e21) {
		// exa
		return withCommasStr(x * 1.0e-18) + "E";
	} else if (x < 1.0e24) {
		// zetta
		return withCommasStr(x * 1.0e-21) + "Z";
	} else if (x < 1.0e27) {
		// yotta
		return withCommasStr(x * 1.0e-24) + "Y";
	} else {
		// very large
		return withCommasStr(x);
	}
}

QStringList getAllInterfaceIPs()
{
	QStringList result;

	struct ifaddrs *ifaddr;
	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return result;
	}

	for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}

		if (ifa->ifa_addr->sa_family == AF_INET ||
			ifa->ifa_addr->sa_family == AF_INET6) {
			char name[NI_MAXHOST];
			int ret = getnameinfo(ifa->ifa_addr,
								  (ifa->ifa_addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) :
																		  sizeof(struct sockaddr_in6),
								  name,
								  NI_MAXHOST,
								  NULL,
								  0,
								  NI_NUMERICHOST);
			if (ret != 0) {
				qDebug() << "getnameinfo() failed:" << gai_strerror(ret);
				continue;
			}
			result << name;
		}
	}
	freeifaddrs(ifaddr);
	return result;
}


quint64 timeFromString(QString text, bool *ok)
{
	quint64 multiplier = 1;
	if (text.endsWith("ns")) {
		text = text.replace("ns", "");
	} else if (text.endsWith("us")) {
		text = text.replace("us", "");
		multiplier = 1000;
	} else if (text.endsWith("ms")) {
		text = text.replace("ms", "");
		multiplier = 1000000;
	} else if (text.endsWith("s")) {
		text = text.replace("s", "");
		multiplier = 1000000000;
	} else {
		if (ok)
			*ok = false;
		return 0;
	}
	return text.toULongLong(ok) * multiplier;
}

QVector<int> kMeans1DAssignPointsToClusters(QVector<qreal> points, QVector<qreal> centroids)
{
    QVector<int> clusters;
    clusters.resize(points.count());

    for (int iPoint = 0; iPoint < points.count(); iPoint++) {
        clusters[iPoint] = 0;
        qreal distMin = fabs(centroids[0] - points[iPoint]);
        for (int c = 0; c < centroids.count(); c++) {
            qreal dist = fabs(centroids[c] - points[iPoint]);
            if (dist < distMin) {
                clusters[iPoint] = c;
                distMin = dist;
            }
        }
    }

    return clusters;
}

QVector<qreal> kMeans1DComputeCentroids(QVector<qreal> points, QVector<int> clusters, int k)
{
    QVector<qreal> centroids;
    centroids.fill(0.0, k);
    QVector<int> centroidPointCount;
    centroidPointCount.fill(0, k);

    for (int iPoint = 0; iPoint < points.count(); iPoint++) {
        Q_ASSERT_FORCE(0 <= clusters[iPoint] && clusters[iPoint] < k);
        centroids[clusters[iPoint]] += points[iPoint];
        centroidPointCount[clusters[iPoint]]++;
    }

    for (int c = 0; c < k; c++) {
        centroids[c] = centroidPointCount[c] > 0 ?
                           centroids[c] / centroidPointCount[c] :
                           1.0e99;
    }

    return centroids;
}

QVector<int> kMeans1D(QVector<qreal> points, int k, int maxIterations, QVector<qreal> initialCentroids)
{
    Q_ASSERT_FORCE(k >= 1);

    if (initialCentroids.count() != k) {
        QVector<int> pointIndices;
        pointIndices.resize(points.count());
        for (int iPoint = 0; iPoint < points.count(); iPoint++) {
            pointIndices[iPoint] = iPoint;
        }
        qShuffle(pointIndices);

        initialCentroids.fill(1.0e99, k);
        for (int c = 0; c < initialCentroids.count(); c++) {
            if (c < pointIndices.count()) {
                initialCentroids[c] = points[pointIndices[c]];
            }
        }
    }

    Q_ASSERT_FORCE(initialCentroids.count() == k);

    QVector<qreal> centroids = initialCentroids;
    qSort(centroids);
    QVector<int> clusters = kMeans1DAssignPointsToClusters(points, centroids);

    for (int i = 0; i < maxIterations; i++) {
        centroids = kMeans1DComputeCentroids(points, clusters, k);
        qSort(centroids);
        QVector<int> newClusters = kMeans1DAssignPointsToClusters(points, centroids);
        if (newClusters == clusters) {
            qDebug() << "Result found after" << (i + 1) << "iterations";
            break;
        }
        clusters = newClusters;
    }

    return clusters;
}

QVector<QVector<qreal> > clusterPoints1D(QVector<qreal> points, QVector<int> clusters, int k)
{
    QVector<QVector<qreal> > clusteredPoints;
    clusteredPoints.resize(k);
    for (int iPoint = 0; iPoint < points.count(); iPoint++) {
        clusteredPoints[clusters[iPoint]].append(points[iPoint]);
    }
    for (int c = 0; c < clusteredPoints.count(); c++) {
        qSort(clusteredPoints[c]);
    }
    return clusteredPoints;
}

void kMeans1DTest()
{
    //
    int k = 2;
    QVector<qreal> points;
    QVector<int> trueClusters;
    QVector<int> clusters;
    QVector<QVector<qreal> > clusteredPoints;

    /////////
    // Test 1
    points = QVector<qreal>()     << 0.1 << 0.2 << 0.3 << 0.4 << 0.5 << 0.6 << 2.5 << 3 << 4;
    trueClusters = QVector<int>() << 0   << 0   << 0   << 0   << 0   << 0   << 1   << 1 << 1;
    clusters = kMeans1D(points, k);

    Q_ASSERT_FORCE(clusters.count() == points.count());

    clusteredPoints = clusterPoints1D(points, clusters, k);
    qDebug() << "Points:" << points;
    qDebug() << "Clusters:" << clusteredPoints;

    Q_ASSERT_FORCE(clusters == trueClusters);

    /////////
    // Test 2
    points = QVector<qreal>()     << 0.1 << 0.2 << 0.3 << 0.4 << 0.5 << 0.6 << 2.5 << 3 << 4 << 10;
    trueClusters = QVector<int>() << 0   << 0   << 0   << 0   << 0   << 0   << 0   << 0 << 1 << 1;
    clusters = kMeans1D(points, k);

    Q_ASSERT_FORCE(clusters.count() == points.count());

    clusteredPoints = clusterPoints1D(points, clusters, k);
    qDebug() << "Points:" << points;
    qDebug() << "Clusters:" << clusteredPoints;

    Q_ASSERT_FORCE(clusters == trueClusters);

    /////////
    // Test 3
    points = QVector<qreal>()     << 0.1 << 0.2 << 0.3 << 0.4 << 0.5 << 0.6 << 2.5 << 3 << 4 << 100;
    trueClusters = QVector<int>() << 0   << 0   << 0   << 0   << 0   << 0   << 0   << 0 << 0 << 1;
    clusters = kMeans1D(points, k);

    Q_ASSERT_FORCE(clusters.count() == points.count());

    clusteredPoints = clusterPoints1D(points, clusters, k);
    qDebug() << "Points:" << points;
    qDebug() << "Clusters:" << clusteredPoints;

    Q_ASSERT_FORCE(clusters == trueClusters);

    exit(0);
}
