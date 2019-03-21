#ifndef END_TO_END_MEASUREMENTS_H
#define END_TO_END_MEASUREMENTS_H

#include <QtCore>
#include "../util/bitarray.h"
#include "util.h"
#include "graph_types.h"
#include "netgraphedge.h"

class LinkIntervalMeasurement
{
public:
    LinkIntervalMeasurement();
	qint32 numPacketsInFlight;
    qint32 numPacketsDropped;
	// All delays are in ms
	qint16 minDelay;
    qint16 maxDelay;
	qint32 sumDelay;
	qint64 sumSquaredDelay;

#ifndef LINE_EMULATOR
	// if the instance is obtained from "interval resample",
    //   binsCovered record the bins that has packets
    QList<bool> binsCovered;
    QList<qint32> binnedNumPacketsInFlight;
    QList<qint32> binnedNumPacketsDropped;
#endif

	void clear();
	qreal successRate() const;
	qreal lossRate() const;
#ifndef LINE_EMULATOR
	qreal lossBinRatio() const;
    qreal binCoverage() const;
#endif
	qreal averageDelay() const;
	qreal delayVariance() const;
	// The throughput in Mbps
	qreal throughput(Timestamp intervalSize) const;
#ifndef LINE_EMULATOR
	qreal binThroughput(Timestamp binSize) const;
#endif
	void recordPacket(bool forwarded, Timestamp delay);

	friend bool operator ==(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator !=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator <(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator <=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator >(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
	friend bool operator >=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);

	LinkIntervalMeasurement& operator+=(LinkIntervalMeasurement other);
    void dump(QString indent);
};

bool operator ==(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator !=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator <(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator <=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator >(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
bool operator >=(const LinkIntervalMeasurement &a, const LinkIntervalMeasurement &b);
QDataStream& operator>>(QDataStream& s, LinkIntervalMeasurement& d);
QDataStream& operator<<(QDataStream& s, const LinkIntervalMeasurement& d);
QString toJson(const LinkIntervalMeasurement& d);


class EndToEndMeasurements
{
public:
	virtual ~EndToEndMeasurements();

	virtual void initialize(Timestamp tsStart,
							Timestamp expectedDuration,
							Timestamp intervalSize,
							int numLinks,
							int numPaths,
							QList<LinkPath> sparseRoutingMatrixTransposed,
							int packetSizeThreshold);

	// Returns the interval that was updated, or -1.
	virtual bool recordPacketLink(PathPair pp, LinkPath ep, Timestamp tsIn, Timestamp tsOut, int size, bool forwarded, Timestamp delay) = 0;
	virtual bool recordPacketPath(PathPair pp, LinkPath ep, Timestamp tsIn, Timestamp tsOut, int size, bool forwarded, Timestamp delay) = 0;

	virtual LinkIntervalMeasurement readLink(PathPair pp, Link e, int i) const = 0;
	virtual LinkIntervalMeasurement readLinkPath(PathPair pp, LinkPath ep, int i) const = 0;
	virtual LinkIntervalMeasurement readPath(PathPair pp, LinkPath ep, int i) const = 0;

	virtual int timestampToOpenInterval(Timestamp ts) const;
	virtual int timestampToInterval(Timestamp ts) const;
	virtual Timestamp intervalToTimestamp(int i) const;

	virtual int intervalCount(PathPair pp) const;
	virtual int firstInterval(PathPair pp) const;
	virtual int firstIntervalNoTransient(PathPair pp) const;
	// Actually next after last
	virtual int lastInterval(PathPair pp) const;
	virtual int lastIntervalNoTransient(PathPair pp) const;

	virtual bool save(QString fileName);
    virtual bool load(QString fileName);

	virtual void trim();

	virtual void saveToStream(QDataStream& s);
    virtual void loadFromStream(QDataStream& s);

	virtual void saveToCSV(QString dirName);
	virtual void plotCSV(QString dirName);

	virtual QList<Link> linksUsed();
	virtual QList<Path> pathsUsed();
	virtual QList<Link> linksAll();
	virtual QList<Path> pathsAll();

	Timestamp tsStart;
	Timestamp tsLast;
    Timestamp intervalSize;
    int numLinks;
    int numPaths;
	int packetSizeThreshold;
	QList<LinkPath> sparseRoutingMatrixTransposed;

	// Path -> interval -> delay sample
	// The difference between this and readPath() is that this one also takes into accounts small packets
	QHash<Path, QHash<int, Timestamp> > pathDelays;

	// Key: path index or flow index.
	// Value: the last bin (packet timestamp/samplingPeriod) used for the key.
	QHash<int, quint64> lastSampleBin;

	virtual void recordPathDelay(Path p, Timestamp tsIn, Timestamp tsOut);
    virtual void dump(QString indent);

protected:
	int numIntervals;

	friend QDataStream& operator<<(QDataStream& s, const EndToEndMeasurements& d);
	friend QDataStream& operator>>(QDataStream& s, EndToEndMeasurements& d);
};
QDataStream& operator<<(QDataStream& s, const EndToEndMeasurements& d);
QDataStream& operator>>(QDataStream& s, EndToEndMeasurements& d);

#endif // END_TO_END_MEASUREMENTS_H
