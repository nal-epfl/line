#include "end_to_end_measurements.h"
#include "compresseddevice.h"

#include "../util/util.h"
#include "../util/json.h"

LinkIntervalMeasurement::LinkIntervalMeasurement()
{
	clear();
}

void LinkIntervalMeasurement::clear()
{
	numPacketsInFlight = 0;
	numPacketsDropped = 0;
	minDelay = 0;
	sumDelay = 0;
	sumSquaredDelay = 0;
    maxDelay = 0;
#ifndef LINE_EMULATOR
    binsCovered.clear();
    binnedNumPacketsInFlight.clear();
    binnedNumPacketsDropped.clear();
#endif
}

qreal LinkIntervalMeasurement::successRate() const
{
	// If # packets = 0, the transmission rate is 1.0
	if (numPacketsInFlight == 0) {
		return 1.0;
	}
	return 1.0 - qreal(numPacketsDropped) / qreal(numPacketsInFlight);
}

qreal LinkIntervalMeasurement::lossRate() const
{
	// If # packets = 0, the loss rate is 0.0
	return 1.0 - successRate();
}

#ifndef LINE_EMULATOR
qreal LinkIntervalMeasurement::lossBinRatio() const
{
    int numLossBins = 0;
    int numBins = 0;

    for (int i = 0; i < binnedNumPacketsDropped.count(); i++) {
        numLossBins += (binnedNumPacketsDropped[i] > 0);
        numBins += (binnedNumPacketsInFlight[i] > 0);
    }
    return qreal(numLossBins) / numBins;
}

qreal LinkIntervalMeasurement::binCoverage() const
{
    return binsCovered.count(true) / qreal(binsCovered.count());
}
#endif

qreal LinkIntervalMeasurement::averageDelay() const
{
	qreal n = numPacketsInFlight - numPacketsDropped;
	if (n == 0)
		return 0;
	return sumDelay / n;
}

qreal LinkIntervalMeasurement::delayVariance() const
{
	qreal n = numPacketsInFlight - numPacketsDropped;
	if (n == 0)
		return 0;
	return sumSquaredDelay / n - sumDelay * sumDelay / n / n;
}

qreal LinkIntervalMeasurement::throughput(Timestamp intervalSize) const
{
	// Here we assume that all packets are full size, reasonable in our TCP experiments
	qreal averageBits = 8 * ETH_FRAME_LEN * qreal(numPacketsInFlight - numPacketsDropped);
	// ns to s and b to Mb => divide by 1000
	return averageBits / (intervalSize * 1.0e-3);
}

#ifndef LINE_EMULATOR
qreal LinkIntervalMeasurement::binThroughput(Timestamp binSize) const
{
    QList<qreal> throughputAllBins;
    throughputAllBins.reserve(binsCovered.count(true));
    for (int i=0; i<binsCovered.count(); i++) {
        if (binsCovered[i]) {
            qreal t = 8 * ETH_FRAME_LEN * qreal(binnedNumPacketsInFlight[i] - binnedNumPacketsDropped[i]) / (binSize * 1.0e-3);
            throughputAllBins.append(t);
        }
    }
    return average(throughputAllBins);
}
#endif

void LinkIntervalMeasurement::recordPacket(bool forwarded, Timestamp delayNs)
{
	numPacketsInFlight++;
	numPacketsDropped += forwarded ? 0 : 1;
	qint16 delay = delayNs /= ms();
	if (delay > 0) {
		if (minDelay == 0)
			minDelay = delay;
		else
			minDelay = qMin(minDelay, delay);
		sumDelay += delay;
		sumSquaredDelay += delay * delay;
        maxDelay = qMax(delay, maxDelay);
    }
}

LinkIntervalMeasurement& LinkIntervalMeasurement::operator+=(LinkIntervalMeasurement other)
{
	this->numPacketsInFlight += other.numPacketsInFlight;
	this->numPacketsDropped += other.numPacketsDropped;
	this->minDelay = this->minDelay ? qMin(this->minDelay, other.minDelay) : other.minDelay;
	this->maxDelay = this->maxDelay ? qMax(this->maxDelay, other.maxDelay) : other.maxDelay;
	this->sumDelay += other.sumDelay;
	this->sumSquaredDelay += other.sumSquaredDelay;

#ifndef LINE_EMULATOR
    this->binsCovered.append(other.binsCovered);
    this->binnedNumPacketsInFlight.append(other.binnedNumPacketsInFlight);
    this->binnedNumPacketsDropped.append(other.binnedNumPacketsDropped);
#endif
    return *this;
}

bool operator ==(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() == b.successRate();
}

bool operator !=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() != b.successRate();
}

bool operator <(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() < b.successRate();
}

bool operator <=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() <= b.successRate();
}

bool operator >(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() > b.successRate();
}

bool operator >=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b)
{
	return a.successRate() >= b.successRate();
}

QDataStream& operator<<(QDataStream& s, const LinkIntervalMeasurement& d) {
    qint32 ver = 12;
	s << ver;

	s << d.numPacketsInFlight;
	s << d.numPacketsDropped;
	s << d.minDelay;
	s << d.sumDelay;
	s << d.sumSquaredDelay;
	s << d.maxDelay;

	return s;
}

void LinkIntervalMeasurement::dump(QString indent)
{
    printf("%snumPacketsInFlight: %d\n", indent.toLatin1().constData(), numPacketsInFlight);
    printf("%snumPacketsDropped: %d\n", indent.toLatin1().constData(), numPacketsDropped);
    printf("%sminDelay: %d\n", indent.toLatin1().constData(), minDelay);
    printf("%ssumDelay: %d\n", indent.toLatin1().constData(), sumDelay);
    printf("%ssumSquaredDelay: %lld\n", indent.toLatin1().constData(), sumSquaredDelay);
    printf("%smaxDelay: %d\n", indent.toLatin1().constData(), maxDelay);
}

QDataStream& operator>>(QDataStream& s, LinkIntervalMeasurement& d) {
    qint32 ver;
    s >> ver;

    d.clear();
	if (ver == 12) {
		s >> d.numPacketsInFlight;
		s >> d.numPacketsDropped;
		s >> d.minDelay;
		s >> d.sumDelay;
		s >> d.sumSquaredDelay;
		s >> d.maxDelay;
#ifndef LINE_EMULATOR
        d.binnedNumPacketsInFlight.append(d.numPacketsInFlight);
        d.binnedNumPacketsDropped.append(d.numPacketsDropped);
        if (d.numPacketsInFlight > 0) {
            d.binsCovered.append(true);
        } else {
            d.binsCovered.append(false);
        }
#endif
    } else {
		qDebug() << __FILE__ << __LINE__ << "Read error";
		exit(-1);
	}

	return s;
}

QString toJson(const LinkIntervalMeasurement &d)
{
	JsonObjectPrinter p;
	jsonObjectPrinterAddMember(p, d.numPacketsInFlight);
	jsonObjectPrinterAddMember(p, d.numPacketsDropped);
	jsonObjectPrinterAddMember(p, d.minDelay);
	jsonObjectPrinterAddMember(p, d.sumDelay);
	jsonObjectPrinterAddMember(p, d.sumSquaredDelay);
	jsonObjectPrinterAddMember(p, d.maxDelay);
	return p.json();
}

EndToEndMeasurements::~EndToEndMeasurements()
{}

void EndToEndMeasurements::initialize(Timestamp tsStart,
									  Timestamp expectedDuration,
									  Timestamp intervalSize,
									  int numLinks,
									  int numPaths,
									  QList<LinkPath> sparseRoutingMatrixTransposed,
									  int packetSizeThreshold)
{
	this->tsStart = tsStart;
	this->tsLast = tsStart;
	this->intervalSize = intervalSize;
	this->numLinks = numLinks;
	this->numPaths = numPaths;
	this->numIntervals = 2 * expectedDuration / intervalSize + 1;
	this->packetSizeThreshold = packetSizeThreshold;
	this->sparseRoutingMatrixTransposed = sparseRoutingMatrixTransposed;
}

Timestamp EndToEndMeasurements::intervalToTimestamp(int i) const
{
	return tsStart + i * intervalSize + 1;
}

int EndToEndMeasurements::intervalCount(PathPair pp) const
{
	Q_UNUSED(pp);
	return numIntervals;
}

int EndToEndMeasurements::firstInterval(PathPair pp) const
{
	Q_UNUSED(pp);
	return 0;
}

int EndToEndMeasurements::firstIntervalNoTransient(PathPair pp) const
{
	Q_UNUSED(pp);
	const qreal transientCutSec = 10;
	return transientCutSec * 1.0e9 / intervalSize;
}

int EndToEndMeasurements::lastInterval(PathPair pp) const
{
	Q_UNUSED(pp);
	return numIntervals;
}

int EndToEndMeasurements::lastIntervalNoTransient(PathPair pp) const
{
	Q_UNUSED(pp);
	const qreal transientCutSec = 10;
	return numIntervals - transientCutSec * 1.0e9 / intervalSize;
}

int EndToEndMeasurements::timestampToOpenInterval(quint64 ts) const
{
	int result;
    if (ts == tsStart) {
		result = 0;
    } else if ((ts - tsStart) % intervalSize != 0) {
		result = timestampToInterval(ts);
    } else {
		result = timestampToInterval(ts) - 1;
    }
	return result;
}

int EndToEndMeasurements::timestampToInterval(quint64 ts) const
{
    int interval = (int)((ts - tsStart) / intervalSize);
    if (interval < numIntervals)
		return interval;
	return -1;
}

bool EndToEndMeasurements::save(QString fileName)
{
	QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
        return false;
    }

	CompressedDevice device(&file);
	if (!device.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
        return false;
	}

	QDataStream out(&device);
	out.setVersion(QDataStream::Qt_4_0);
	this->saveToStream(out);

    if (out.status() != QDataStream::Ok) {
        qDebug() << __FILE__ << __LINE__ << "Error writing file:" << file.fileName();
        return false;
    }
    return true;
}

bool EndToEndMeasurements::load(QString fileName)
{
	QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
		QDir dir;
		qDebug() << "Current dir:" << dir.canonicalPath();
        return false;
    }

	CompressedDevice device(&file);
	if (!device.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << file.fileName();
        return false;
	}

    QDataStream in(&device);
    in.setVersion(QDataStream::Qt_4_0);

	this->loadFromStream(in);
	trim();

    if (in.status() != QDataStream::Ok) {
        qDebug() << __FILE__ << __LINE__ << "Error reading file:" << file.fileName();
        return false;
    }

	return true;
}

void EndToEndMeasurements::trim()
{
	while (numIntervals > 0) {
		int i = numIntervals - 1;
		foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
			PathPair dummy;
			if (readLink(dummy, ep.first, i).numPacketsInFlight > 0 ||
				readLinkPath(dummy, ep, i).numPacketsInFlight > 0 ||
				readPath(dummy, ep, i).numPacketsInFlight > 0)
				return;
		}
		numIntervals--;
	}
}

void EndToEndMeasurements::saveToStream(QDataStream &s)
{
	s << *this;
}

void EndToEndMeasurements::loadFromStream(QDataStream &s)
{
	s >> *this;
}

void EndToEndMeasurements::saveToCSV(QString dirName)
{
	{
		QDir dir;
		dir.mkpath(dirName);
	}

	foreach (Link e, linksUsed()) {
		QFile f(QString("%1/link-%2.csv").arg(dirName).arg(e + 1));
		if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
			continue;
		}
		QTextStream out(&f);
		out << "Time";
		out << ",%e" << (e+1);
		out << ",#e" << (e+1);
		foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
			if (ep.first != e)
				continue;
			out << ",%ep" << (ep.second + 1);
			out << ",#ep" << (ep.second + 1);
			out << ",%p" << (ep.second + 1);
			out << ",#p" << (ep.second + 1);
		}
		out << "\n";

		for (int i = 0; i < numIntervals; i++) {
			qreal time = (i * intervalSize) * 1.0e-9;
			out << time;
			PathPair dummy;
			LinkIntervalMeasurement me = readLink(dummy, e, i);
			out << "," << (me.successRate());
			out << "," << (me.numPacketsInFlight);
			foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
				if (ep.first != e)
					continue;
				LinkIntervalMeasurement mep = readLinkPath(dummy, ep, i);
				out << "," << (mep.successRate());
				out << "," << (mep.numPacketsInFlight);

				LinkIntervalMeasurement mp = readPath(dummy, ep, i);
				out << "," << (mp.successRate());
				out << "," << (mp.numPacketsInFlight);
			}
			out << "\n";
		}
	}
}

void EndToEndMeasurements::plotCSV(QString dirName)
{
	foreach (Link e, linksUsed()) {
		QString path = QString("%1/link-%2.csv").arg(dirName).arg(e + 1);
		// for r in 1 2 3 5 10 ; do '%3/line-runner/plot-scripts-new/plot-csv-intervals.py' --in $f --resample $r
		foreach (int r, QList<int>() << 1) {
			QProcess::execute(QString("%1/line-runner/plot-scripts-new/plot-csv-intervals.py").arg(sourceDir()),
							  QStringList()
							  << "--in" << path
							  << "--resample" << QString::number(r));
		}
	}
}

QList<Link> EndToEndMeasurements::linksUsed()
{
	QSet<Link> links;
	foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
		links.insert(ep.first);
	}
	QList<Link> result = links.toList();
	qSort(result);
	return result;
}

QList<Link> EndToEndMeasurements::linksAll()
{
	QList<Link> result;
	for (Link e = 0; e < numLinks; e++)
		result << e;
	return result;
}

QList<Path> EndToEndMeasurements::pathsUsed()
{
	QSet<Path> paths;
	foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
		paths.insert(ep.second);
	}
	QList<Path> result = paths.toList();
	qSort(result);
	return result;
}

QList<Path> EndToEndMeasurements::pathsAll()
{
	QList<Path> result;
	for (Path p = 0; p < numPaths; p++)
		result << p;
	return result;
}

void EndToEndMeasurements::recordPathDelay(Path p, Timestamp tsIn, Timestamp tsOut)
{
	int iIn = timestampToOpenInterval(tsIn);
	if (iIn < 0)
		return;
	pathDelays[p][iIn] = tsOut - tsIn;
}

QDataStream& operator<<(QDataStream& s, const EndToEndMeasurements& d)
{
	qint32 ver = 2;

    s << ver;
    s << d.tsStart;
	s << d.tsLast;
    s << d.intervalSize;
    s << d.numIntervals;
    s << d.packetSizeThreshold;
	s << d.numLinks;
	s << d.numPaths;
	s << d.sparseRoutingMatrixTransposed;

	s << d.pathDelays;

    return s;
}

void EndToEndMeasurements::dump(QString indent)
{
    printf("%stsStart: %llu\n", indent.toLatin1().constData(), tsStart);
    printf("%stsLast: %llu\n", indent.toLatin1().constData(), tsLast);
    printf("%sintervalSize: %llu\n", indent.toLatin1().constData(), intervalSize);
    printf("%snumIntervals: %d\n", indent.toLatin1().constData(), numIntervals);
    printf("%spacketSizeThreshold: %d\n", indent.toLatin1().constData(), packetSizeThreshold);
    printf("%snumLinks: %d\n", indent.toLatin1().constData(), numLinks);
    printf("%snumPaths: %d\n", indent.toLatin1().constData(), numPaths);
    printf("%ssparseRoutingMatrixTransposed: %d items\n", indent.toLatin1().constData(), sparseRoutingMatrixTransposed.count());
    foreach (LinkPath ep, sparseRoutingMatrixTransposed) {
        printf("%s link: %d\n", indent.toLatin1().constData(), ep.first);
        printf("%s path: %d\n", indent.toLatin1().constData(), ep.second);
    }
    printf("%spathDelays: %d paths\n", indent.toLatin1().constData(), pathDelays.count());
    foreach (Path p, pathDelays.keys()) {
        printf("%s path: %d\n", indent.toLatin1().constData(), p);
        printf("%s intervals: %d items\n", indent.toLatin1().constData(), pathDelays[p].count());
        foreach (int i, pathDelays[p].keys()) {
            printf("%s  interval: %d\n", indent.toLatin1().constData(), i);
            printf("%s  delay: %llu\n", indent.toLatin1().constData(), pathDelays[p][i]);
        }
    }
}

QDataStream& operator>>(QDataStream& s, EndToEndMeasurements& d)
{
    qint32 ver;

    s >> ver;
	Q_ASSERT_FORCE(1 <= ver && ver <= 2);

	s >> d.tsStart;
	s >> d.tsLast;
    s >> d.intervalSize;
    s >> d.numIntervals;
    s >> d.packetSizeThreshold;
	s >> d.numLinks;
	s >> d.numPaths;
	s >> d.sparseRoutingMatrixTransposed;

	if (ver >= 2) {
		s >> d.pathDelays;
	} else {
		d.pathDelays.clear();
	}

    return s;
}
