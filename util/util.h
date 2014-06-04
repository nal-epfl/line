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

#ifndef UTIL_H
#define UTIL_H

#include <signal.h>
#include <QtCore>
#include "debug.h"
#include <limits.h>
#include <unistd.h>
#include <random>
#include "qtuples.h"

typedef QPair<qreal, qreal> RealRealPair;
typedef QPair<qreal, qreal> QRealPair;
typedef QPair<qint32, qint32> QInt32Pair;
typedef QPair<QString, QString> QStringPair;

typedef QTriple<qreal, qreal, qreal> QRealTriple;
typedef QTriple<qint32, qint32, qint32> QInt32Triple;
typedef QTriple<QString, QString, QString> QStringTriple;

typedef QQuad<qint32, qint32, qint32, qint32> QInt32Quad;
typedef QQuad<QString, QString, QString, QString> QStringQuad;

typedef QSet<qint32> QInt32Set;
typedef QList<qint32> QInt32List;

template <class T>
uint qHash(const QSet<T>& set) {
	uint seed = 0;

	QList<T> list = set.toList();
	qSort(list);
	for(auto x : list) {
		seed ^= qHash(x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	return seed;
}

// Similar to qDebug(), but without quotes and optionally without newlines.
// Use qOut() or qErr() to instantiate.
// For convenience, qDebug() is also replaced.
// Also, qWarn()/qWarning() and qError() are provided, which automatically prefix the line with "WARNING:" or "ERROR:".

class OutputStream
{
protected:
	static QList<QSharedPointer<QTextStream> > copies;
	class Stream {
	protected:
	public:
		Stream(FILE *file) :
			ts(file),
			addSpace(1),
			addNewline(1),
			ref(1) {}
		QTextStream ts;
		int addSpace;
		int addNewline;
		int ref;
	};
	Stream *stream;
public:
	inline OutputStream(FILE *file = stderr, QString prefix = QString())
		: stream(new Stream(file)) {
		if (!prefix.isEmpty()) {
			(*this) << prefix;
			foreach (QSharedPointer<QTextStream> s, copies) {
				(*s) << prefix;
			}
		}
	}
	inline OutputStream(const OutputStream &other)
		: stream(other.stream) {
		++stream->ref;
	}

	inline ~OutputStream() {
		if (!--stream->ref) {
			if (stream->addNewline > 0) {
				stream->ts << endl;
				foreach (QSharedPointer<QTextStream> s, copies) (*s) << endl;
			}
			delete stream;
		}
	}

	inline static void addCopy(QSharedPointer<QTextStream> s) { copies << s; }
	inline static void flushCopies() {
		foreach(QSharedPointer<QTextStream> s, copies) {
			s->flush();
			QIODevice *device = s->device();
			QFile *file = dynamic_cast<QFile*>(device);
			if (file) {
				file->flush();
			}
		}
	}

	inline OutputStream &noSpace() {
		stream->addSpace--;
		return *this;
	}

	inline OutputStream &nospace() {
		return noSpace();
	}

	inline OutputStream &enableSpace() {
		stream->addSpace++;
		return *this;
	}

	inline OutputStream &maybeSpace() {
		return space();
	}

	inline OutputStream &noNewline() {
		stream->addNewline--;
		return *this;
	}

	inline OutputStream &enableNewline() {
		stream->addNewline++;
		return *this;
	}

	inline OutputStream &space() {
		if (stream->addSpace > 0) {
			stream->ts << ' ';
			foreach (QSharedPointer<QTextStream> s, copies)
				(*s) << ' ';
		}
		return *this;
	}

	inline OutputStream &operator<<(QChar t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(QBool t) {
		QString v = (t != 0) ? "true" : "false";
		stream->ts << v;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << v;
		return space();
	}

	inline OutputStream &operator<<(bool t) {
		QString v = t ? "true" : "false";
		stream->ts << v;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << v;
		return space();
	}

	inline OutputStream &operator<<(char t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(signed short t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(unsigned short t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(signed int t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(unsigned int t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(signed long t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(unsigned long t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(qint64 t) {
		QString v = QString::number(t);
		stream->ts << v;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << v;
		return space();
	}

	inline OutputStream &operator<<(quint64 t) {
		QString v = QString::number(t);
		stream->ts << v;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << v;
		return space();
	}

	inline OutputStream &operator<<(float t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(double t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(const char* t) {
		QString v = QString::fromAscii(t);
		stream->ts << v;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << v;
		return space();
	}

	inline OutputStream &operator<<(const QString & t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(const QStringRef & t) {
		QString v = t.toString();
		stream->ts << v;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << v;
		return space();
	}

	inline OutputStream &operator<<(const QLatin1String &t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t.latin1();
		return space();
	}

	inline OutputStream &operator<<(const QByteArray & t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(const void * t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return space();
	}

	inline OutputStream &operator<<(QTextStreamFunction t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return *this;
	}

	inline OutputStream &operator<<(QTextStreamManipulator t) {
		stream->ts << t;
		foreach (QSharedPointer<QTextStream> s, copies)
			(*s) << t;
		return *this;
	}
};

template <class T>
inline OutputStream operator<<(OutputStream debug, const QList<T> &list) {
	debug.noSpace();
	debug << '[';
	for (Q_TYPENAME QList<T>::size_type i = 0; i < list.count(); ++i) {
		if (i > 0) {
			debug << ", ";
		}
		debug << list.at(i);
	}
	debug << ']';
	debug.enableSpace();
	return debug.space();
}

template <typename T>
inline OutputStream operator<<(OutputStream debug, const QVector<T> &vec) {
	debug.noSpace();
	debug << '[';
	for (Q_TYPENAME QVector<T>::size_type i = 0; i < vec.count(); ++i) {
		if (i > 0) {
			debug << ", ";
		}
		debug << vec.at(i);
	}
	debug << ']';
	debug.enableSpace();
	return debug.space();
}

template <class aKey, class aT>
inline OutputStream operator<<(OutputStream debug, const QMap<aKey, aT> &map) {
	debug.noSpace();
	debug << "{";
	for (typename QMap<aKey, aT>::const_iterator it = map.constBegin(); it != map.constEnd(); ++it) {
		if (it != map.constBegin()) {
			debug << ", ";
		}
		debug << it.key() << "->" << it.value();
	}
	debug << '}';
	debug.enableSpace();
	return debug.space();
}

template <class aKey, class aT>
inline OutputStream operator<<(OutputStream debug, const QHash<aKey, aT> &hash) {
	debug.noSpace();
	debug << "{";
	for (typename QHash<aKey, aT>::const_iterator it = hash.constBegin(); it != hash.constEnd(); ++it) {
		if (it != hash.constBegin()) {
			debug << ", ";
		}
		debug << it.key() << " -> " << it.value();
	}
	debug << '}';
	debug.enableSpace();
	return debug.space();
}

template <class T1, class T2>
inline OutputStream operator<<(OutputStream debug, const QPair<T1, T2> &pair) {
	debug.noSpace();
	debug << "(" << pair.first << ", " << pair.second << ')';
	debug.enableSpace();
	return debug.space();
}

template <typename T>
inline OutputStream operator<<(OutputStream debug, const QSet<T> &set) {
	debug.noSpace();
	debug << '{';
	for (typename QSet<T>::const_iterator it = set.constBegin(); it != set.constEnd(); ++it) {
		if (it != set.constBegin()) {
			debug << ", ";
		}
		debug << *it;
	}
	debug << '}';
	debug.enableSpace();
	return debug.space();
}

template <class T>
inline OutputStream operator<<(OutputStream debug, const QContiguousCache<T> &cache) {
	debug.noSpace();
	debug << "QContiguousCache(";
	for (int i = cache.firstIndex(); i <= cache.lastIndex(); ++i) {
		debug << cache[i];
		if (i != cache.lastIndex())
			debug << ", ";
	}
	debug << ')';
	debug.enableSpace();
	return debug.space();
}

template <class T>
inline OutputStream operator<<(OutputStream debug, const QFlags<T> &flags) {
	debug.noSpace();
	debug << "QFlags(";
	bool needSeparator = false;
	for (uint i = 0; i < sizeof(T) * 8; ++i) {
		if (flags.testFlag(T(1 << i))) {
			if (needSeparator) {
				debug << '|';
			} else {
				needSeparator = true;
			}
			debug << "0x" << QByteArray::number(T(1 << i), 16).constData();
		}
	}
	debug << ')';
	debug.enableSpace();
	return debug.space();
}

#define QDebug OutputStream
#define qOut() OutputStream(stdout)
#define qErr() OutputStream()
#define qWarning() OutputStream(stderr, "WARNING:")
#define qWarn() qWarning()
#define qError() OutputStream(stderr, "ERROR:")
#ifndef DEBUG
#define DEBUG 0
#endif
#define qDebugT() if (DEBUG) OutputStream(stderr, QString("[%1] %2:%3 %4: ") \
	.arg(QDateTime::currentDateTime().toString()) \
	.arg(__FILE__) \
	.arg(__LINE__) \
	.arg(__FUNCTION__))
#define qDebugF() OutputStream(stderr, QString("[%1] %2:%3 %4: ") \
	.arg(QDateTime::currentDateTime().toString()) \
	.arg(__FILE__) \
	.arg(__LINE__) \
	.arg(__FUNCTION__))
//#define qDebug() qDebugF()
#define qDebug() qErr()

bool saveFile(QString name, QString content);
bool readFile(QString name, QString &content, bool silent = false);
bool saveFile(QString name, QByteArray content);
bool readTextArchive(QString name, QStringList &fileNames, QStringList &files, bool silent = false);
bool saveTextArchive(QString name, const QStringList &fileNames, const QStringList &files);
bool runProcess(QString name, QStringList args, bool silent = false);
bool runProcess(QString name, QStringList args, QString &output, int timeoutsec = -1, bool silent = false, int *exitCode = NULL);
bool runCommand(QString command, QStringList args, QString description);
bool runCommand(QString command, QStringList args, QString description, bool showOutput, QString &output);

QString absolutePathFromPath(QString path);
QString absoluteDirPathFromFilePath(QString path);
QString parentDirFromDir(QString path);

inline quint64 bit_scan_forward_asm64(quint64 v)
{
	if (v == 0)
		return 0;
	quint64 r;
	asm volatile("bsfq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

// essentialy the integer base 2 logarithm
inline quint64 bit_scan_reverse_asm64(quint64 v)
{
	if (v == 0)
		return 0;
	quint64 r;
	asm volatile("bsrq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

#define EARTH_RADIUS_KM 6371
// Returns the distance between 2 points on a sphere with a specified radius
qreal sphereDistance(qreal lat1, qreal long1, qreal lat2, qreal long2, qreal radius = EARTH_RADIUS_KM);

// See The Effective Rank: A Measure of Effective Dimensionality - Roy, Olivier; Vetterli, Martin
qreal effectiveRank_Vetterli(QList<qreal> svd);

bool randomLessThan(const QString &, const QString &);

qreal median(QList<qreal> list);
qreal sum(QList<qreal> list);

// Applies 1D k-means clustering to the points.
// Returns a vector with the cluster assignment for each point (index: point index, value: 0 to k-1).
// If the k initial centroids are not specified, they are selected at random.
QVector<int> kMeans1D(QVector<qreal> points, int k,
                      int maxIterations = 1000,
                      QVector<qreal> initialCentroids = QVector<qreal>());

QVector<QVector<qreal> > clusterPoints1D(QVector<qreal> points, QVector<int> clusters, int k);

void kMeans1DTest();

quint64 bPerSec2bPerNanosec(quint64 value);

// Converts a number followed by [multiplier] and unit (b or B)
// to the corresponding number of bits.
// E.g. 200kB -> 1600000 bits
quint64 dataSizeWithUnit2bits(QString s, bool *ok = NULL);
void test_dataSizeWithUnit2bits();

QString bits2DataSizeWithUnit(quint64 bits);
void test_bits2DataSizeWithUnit();

// Random number in [0, 1]
#define frand() ((qreal)rand()/(qreal)RAND_MAX)
// Random number in [0, 1)
#define frandex() ((qreal)rand()/(qreal)(RAND_MAX + 1.0))
// Random number in (0, 1)
#define frandex2() ((qreal)(rand() + 1.0)/(qreal)(RAND_MAX + 2.0))
// Random number in [a, b]
#define frand2(a,b) ((a) + ((b)-(a)) * frand())

// Random number generators using MT (pass an instance of std::mt19937 as parameter g)
#define frandmt(g) ((qreal)((g)() - (g).min())/(qreal)((g).max() - (g).min()))
#define frand2mt(g,a,b) ((a) + ((b)-(a)) * (qreal)frandmt(g))
#define frandexmt(g) ((qreal)((g)() - (g).min())/(qreal)((g).max() - (g).min() + 1.0))
#define frandex2mt(g) ((qreal)((g)() - (g).min() + 1.0)/(qreal)((g).max() - (g).min() + 2.0))

// For size_t cast the parameter to quint64
#define withCommasStr(x) QLocale(QLocale::English).toString(x)

QString withMultiplierSuffix(qreal x);

// Careful with this, only use the pointer in a single expression.
#define withCommas(x) withCommasStr(x).toLatin1().constData()

#ifndef QT_GUI_LIB
class QColor {
public:
	QColor(int r, int g, int b, int a = 255);
	int red();
	int green();
	int blue();
	int alpha();
	int r;
	int g;
	int b;
	int a;
	static QColor fromRgb(int r, int g, int b, int a = 255);
};
#else
#include <QColor>
#endif

#ifdef QT_GUI_LIB
class QComboBox;
bool setCurrentItem(QComboBox *box, QString text);
#endif

class HTMLWriter {
public:
	static QString color2css(QColor c);
	void beginHtml();
	void endHtml();
	void writeHeading1(QString s);
	void writeHeading2(QString s);
	void writeHeading3(QString s);
	void writeHeading4(QString s);
	void writeText(QString s);
	void beginTable(QString style = QString());
	void endTable();
	void beginTableHeader();
	void endTableHeader();
	void writeTableHeaderCell(QString s, QColor textColor = QColor::fromRgb(0, 0, 0), QColor bgColor = QColor::fromRgb(255, 255, 255), QString tooltip = QString());
	void beginTableRow();
	void endTableRow();
	void writeTableCell(QString s, QColor textColor = QColor::fromRgb(0, 0, 0), QColor bgColor = QColor::fromRgb(255, 255, 255), QString tooltip = QString());
	QString html;

	bool save(QString fileName);
};

extern QColor green;
extern QColor red;
extern QColor black;
extern QColor white;
extern QColor yellow;
extern QColor blue;
extern QColor gray;

// Generate number uniformely distributed between min..max (inclusive) by rejection sampling.
int randInt(int min, int max);
void testRandIntRange(int max);
void testRandInt();

// Generate a random number between 0..n-1 given n probability density function values.
// The densities must be non-negative with positive sum. No need to normalize.
int randDist(QList<qreal> densities);
void testRandDist();

QString toString(const QBitArray &b);

bool fileInDirectory(QString fileName, QString dirName);

void writeNumberCompressed(QDataStream &s, qint64 d);
void writeNumberCompressed(QDataStream &s, qint32 d);
void writeNumberCompressed(QDataStream &s, qint16 d);
void writeNumberCompressed(QDataStream &s, qint8 d);

void readNumberCompressed(QDataStream &s, qint64 &d);
void readNumberCompressed(QDataStream &s, qint32 &d);
void readNumberCompressed(QDataStream &s, qint16 &d);
void readNumberCompressed(QDataStream &s, qint8 &d);

void writeNumberCompressed(QDataStream &s, quint64 u);
void writeNumberCompressed(QDataStream &s, quint32 u);
void writeNumberCompressed(QDataStream &s, quint16 u);
void writeNumberCompressed(QDataStream &s, quint8 u);

void readNumberCompressed(QDataStream &s, quint64 &u);
void readNumberCompressed(QDataStream &s, quint32 &u);
void readNumberCompressed(QDataStream &s, quint16 &u);
void readNumberCompressed(QDataStream &s, quint8 &u);

void testReadWriteNumberCompressed();

template<typename T>
void shuffle(QList<T> &list) {
	int n = list.count();
	for (int i = n-1; i >= 1; i--) {
		int j = randInt(0, i);
		qSwap(list[j], list[i]);
	}
}

template<typename T>
void shuffle(QVector<T> &list) {
	int n = list.count();
	for (int i = n-1; i >= 1; i--) {
		int j = randInt(0, i);
		qSwap(list[j], list[i]);
	}
}

QString timeToString(quint64 value);
quint64 timeFromString(QString text, bool *ok = NULL);

template<typename T>
T pickRandomElement(QList<T> list)
{
    if (list.isEmpty())
        return T();
    return list.at(randInt(0, list.count()-1));
}

// Only works if T and U implement operator<().
template<typename T, typename U>
uint qHash(QHash<T, U> hash)
{
    uint h = 0;

    QList<T> keys = hash.uniqueKeys();
    qSort(keys);
    foreach (T key, keys) {
        QList<U> values = hash.values(key);
        qSort(values);

        foreach (U value, values) {
            h = ((h << 16) | (h >> 16)) ^ qHash(key);
            h = ((h << 16) | (h >> 16)) ^ qHash(value);
        }
    }
    return h;
}

template <typename T>
class qRandomLess
{
public:
    inline bool operator()(const T &, const T &) const
    {
	   return rand() < (RAND_MAX >> 1);
    }
};

template<typename Container, typename T>
inline void qShuffleHelper(Container &c, T &)
{
	qSort(c.begin(), c.end(), qRandomLess<T>());
}

template<typename Container>
inline void qShuffle(Container &c)
{
	if (!c.isEmpty())
		qShuffleHelper(c, *c.begin());
}

template<typename ForwardIterator>
inline QString toString(ForwardIterator begin, ForwardIterator end)
{
	QString result;
	for (; begin != end; ++begin) {
		result += (result.isEmpty() ? QString() : QString(", ")) + QString("%1").arg(*begin);
	}
	return result;
}

template<typename Container>
inline QString toString(Container c)
{
	return toString(c.begin(), c.end());
}

template<typename Container>
inline Container sorted(Container c)
{
	qSort(c);
	return c;
}

template <typename ForwardIterator, typename T>
T qMinimum(ForwardIterator begin, ForwardIterator end, T defaultValue)
{
	if (begin == end) {
		return defaultValue;
	}
	ForwardIterator result = begin;
	while (begin != end) {
		if (*result > *begin)
			result = begin;
		++begin;
	}
	return *result;
}

template <typename Container, typename T>
T qMinimum(const Container &c, T defaultValue = T())
{
	return qMinimum(c.begin(), c.end(), defaultValue);
}

template <typename ForwardIterator, typename T>
T qMaximum(ForwardIterator begin, ForwardIterator end, T defaultValue)
{
	if (begin == end) {
		return defaultValue;
	}
	ForwardIterator result = begin;
	while (begin != end) {
		if (*begin > *result)
			result = begin;
		++begin;
	}
	return *result;
}

template <typename Container, typename T>
T qMaximum(const Container &c, T defaultValue)
{
	return qMaximum(c.begin(), c.end(), defaultValue);
}

class KeyVal {
public:
	int val;
	int key;
};

int getNumCoresLinux();

bool keyValHigherEqual(const KeyVal &s1, const KeyVal &s2);

void serialSigInt(int sig);

void printBacktrace();

QString shortString(QString s, int length = 40);

template <typename JobList, typename MapFunctor>
void parallel(MapFunctor functor, JobList &jobs, int maxThreads, bool silent)
{
	int oldMaxThreads = QThreadPool::globalInstance()->maxThreadCount();
	QThreadPool::globalInstance()->setMaxThreadCount(maxThreads);
	if (!silent)
		gray(QString() + "Job count: " + QString::number(jobs.count()) + " Thread pool size: " + QString::number(maxThreads));
	qDebug() << "[" + QString("=").repeated(jobs.count()) + "]";
	qDebug() << "[";
	(void) signal(SIGINT, serialSigInt);
	QtConcurrent::blockingMap(jobs.begin(), jobs.end(), functor);
	qDebug() << "]";
	(void) signal(SIGINT, SIG_DFL);
	QThreadPool::globalInstance()->setMaxThreadCount(oldMaxThreads);
}

extern bool stopSerial;

template <typename Iterator, typename MapFunctor>
void serialHelper(MapFunctor functor, Iterator begin, Iterator end)
{
	Iterator i;
	for (i = begin; i != end && !stopSerial; ++i)
		functor(*i);
	if (stopSerial && i != end) {
		qDebug() << "Interrupted";
	}
}

template <typename JobList, typename MapFunctor>
void serial(MapFunctor functor, JobList &jobs, bool silent)
{
	if (!silent)
		gray(QString() + "Job count: " + QString::number(jobs.count()));
	stopSerial = false;
	(void) signal(SIGINT, serialSigInt);
	qDebug() << "[" + QString("=").repeated(jobs.count()) + "]";
	qDebug() << "[";
	serialHelper(functor, jobs.begin(), jobs.end());
	qDebug() << "]";
	(void) signal(SIGINT, SIG_DFL);
}

void initRand();

#ifdef QT_NETWORK_LIB

QString resolveDNSName(QString hostname);
QString resolveDNSReverse(QString ip);
QString resolveDNSReverseCached(QString ip, bool silent = true);

#endif

typedef unsigned ipnum;
typedef QPair<ipnum, ipnum> Block;

// Returns true if ip is a valid IPv4 address in a.b.c.d format
bool ipnumOk(QString address);

QString ipnumToString(ipnum ip);
ipnum stringToIpnum(QString s);

QString blockToString(Block &block);

// Converts a.b.c.d/mask into a block (first address, last address)
Block prefixToBlock(QString prefix);

// Returns true if prefix is a valid IPv4 route in a.b.c.d/mask format
bool prefixOk(QString prefix);

QString getMyIp();
QString getMyIface();
QString getIfaceIp(QString iface);
QString downloadFile(QString webAddress);

QStringList getAllInterfaceIPs();

// Replaces non-printable ascii chars in s with '?'
void makePrintable(QString &s);

int levenshteinDistance(QString s, QString t, Qt::CaseSensitivity cs = Qt::CaseSensitive);
QString longestCommonSubstring(QString str1, QString str2, Qt::CaseSensitivity cs = Qt::CaseSensitive);

QStringList getBacktrace();

#define MUTEX_DBG_DEADLOCKS 1

#if MUTEX_DBG_DEADLOCKS
#define MUTEX_DBG_FAST 1
// Prints the backtrace if a lock() timeouts.
// If MUTEX_DBG_FAST is 0, also prints the trace of the thread that owns the lock.
class QMutexDbg
{
public:
	inline explicit QMutexDbg(QMutex::RecursionMode mode) {
		qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		memberMutex = new QMutex(mode);
		wrappedMutex = new QMutex(mode);
		defaultTimeout = 1000; // milliseconds
	}

	inline ~QMutexDbg() {
		// qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		delete memberMutex;
		delete wrappedMutex;
	}

	inline void lock() {
		// qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		bool neverFailed = true;
		while (!tryLock(defaultTimeout)) {
			if (neverFailed) {
				QStringList currentTrace = getBacktrace();
				QStringList otherTrace;
				{
					QMutexLocker locker(memberMutex); Q_UNUSED(locker);
					otherTrace = lockTrace;
				}
				QStringList msg;
				msg << QString("===== QMutexDbg::lock() possible deadlock =====");
				msg << QString("Could not acquire lock for %1 seconds").arg(defaultTimeout * 1.0e-3);
				msg << QString("Current backtrace (blocked thread):");
				msg << currentTrace;
				msg << QString("Lock backtrace (thread that did not release the mutex):");
				msg << lockTrace;
				msg << QString("===============================================");
				fprintf(stderr, "%s\n", qPrintable(msg.join("\n")));
				fflush(stderr);
			}
			neverFailed = false;
		}
	}

	inline void lockInline() {
		lock();
	}

	inline bool tryLockInline() {
		return tryLock();
	}

	inline bool tryLock() {
		// qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		if (wrappedMutex->tryLock()) {
			QMutexLocker locker(memberMutex); Q_UNUSED(locker);
			if (!MUTEX_DBG_FAST) {
				lockTrace = getBacktrace();
			} else {
				lockTrace = QStringList() << QString("(Optimized out - use MUTEX_DBG_FAST=0 to find this)");
			}
			return true;
		} else {
			return false;
		}
	}

	inline bool tryLock(int timeout) {
		// qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		if (wrappedMutex->tryLock(timeout)) {
			QMutexLocker locker(memberMutex); Q_UNUSED(locker);
			if (!MUTEX_DBG_FAST) {
				lockTrace = getBacktrace();
			} else {
				lockTrace = QStringList() << QString("(Optimized out - use MUTEX_DBG_FAST=0 to find this)");
			}
			return true;
		} else {
			return false;
		}
	}

	inline void unlockInline() {
		unlock();
	}

	inline void unlock() {
		// qDebug() << __FILE__ << __FUNCTION__ << __LINE__;
		QMutexLocker locker(memberMutex); Q_UNUSED(locker);
		lockTrace.clear();
		wrappedMutex->unlock();
	}

protected:
	QStringList lockTrace;
	mutable QMutex *memberMutex;
	mutable QMutex *wrappedMutex;
	int defaultTimeout;
};

// The only difference between this and QMutexLocker is that the type
// of the mutex is QMutexDbg instead of QMutex; everything else should
// be kept identical.
class QMutexLockerDbg
{
public:
	inline explicit QMutexLockerDbg(QMutexDbg *m, QString caller = QString())
		: caller(caller)
	{
		//qDebug() << QString("Trying to lock from %1, thread %2").arg(caller.isEmpty() ? "unknown" : caller).arg(QThread::currentThreadId());

		Q_ASSERT_X((reinterpret_cast<quintptr>(m) & quintptr(1u)) == quintptr(0),
				   "QMutexLocker", "QMutex pointer is misaligned");
		if (m) {
			m->lockInline();
			val = reinterpret_cast<quintptr>(m) | quintptr(1u);
		} else {
			val = 0;
		}

		//qDebug() << QString("Locked from %1, thread %2").arg(caller.isEmpty() ? "unknown" : caller).arg(QThread::currentThreadId());
	}
	inline ~QMutexLockerDbg() {
		unlock();
		//qDebug() << QString("Unlocked by %1, thread %2").arg(caller.isEmpty() ? "unknown" : caller).arg(QThread::currentThreadId());
	}

	inline void unlock()
	{
		if ((val & quintptr(1u)) == quintptr(1u)) {
			val &= ~quintptr(1u);
			mutex()->unlockInline();
		}
	}

	inline void relock()
	{
		if (val) {
			if ((val & quintptr(1u)) == quintptr(0u)) {
				mutex()->lockInline();
				val |= quintptr(1u);
			}
		}
	}

#if defined(Q_CC_MSVC)
#pragma warning( push )
#pragma warning( disable : 4312 ) // ignoring the warning from /Wp64
#endif

	inline QMutexDbg *mutex() const
	{
		return reinterpret_cast<QMutexDbg *>(val & ~quintptr(1u));
	}

#if defined(Q_CC_MSVC)
#pragma warning( pop )
#endif

private:
	Q_DISABLE_COPY(QMutexLockerDbg)

	quintptr val;
	QString caller;
};

#else

#define QMutexDbg QMutex

class QMutexLockerDbg
{
public:
	inline explicit QMutexLockerDbg(QMutexDbg *m, QString caller = QString())
		: caller(caller)
	{
		qDebug() << QString("Trying to lock from %1, thread %2").arg(caller.isEmpty() ? "unknown" : caller).arg(QThread::currentThreadId());

		locker = new QMutexLocker(m);

		qDebug() << QString("Locked from %1, thread %2").arg(caller.isEmpty() ? "unknown" : caller).arg(QThread::currentThreadId());
	}
	inline ~QMutexLockerDbg() {
		delete locker;
		qDebug() << QString("Unlocked by %1, thread %2").arg(caller.isEmpty() ? "unknown" : caller).arg(QThread::currentThreadId());
	}

	inline void unlock()
	{
		locker->unlock();
	}

	inline void relock()
	{
		locker->relock();
	}

	inline QMutexDbg *mutex() const
	{
		return locker->mutex();
	}

private:
	QMutexLocker *locker;
	QString caller;
};

#endif

#endif // UTIL_H
