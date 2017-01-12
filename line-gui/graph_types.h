#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H

#include <QtCore>

typedef qint32 Link;
typedef qint32 Path;
typedef qint32 Node;
typedef qint32 Class;
typedef qint32 Interval;
typedef QPair<Link, Path> LinkPath;
typedef QPair<Path, Path> PathPair;
typedef QSet<Link> LinkSequence;
typedef QSet<Link> LinkSet;
typedef QSet<Path> PathSet;
typedef QList<Link> OrderedLinkSequence;
typedef QList<Link> LinkList;
typedef QPair<Node, Node> NodePair;
typedef QList<Path> PathList;
typedef QPair<PathPair, PathPair> PathPairPair;

typedef quint64 PacketId;
typedef quint64 Timestamp;

inline Timestamp ms() {
	return 1000000ULL;
}

inline bool isIdentical(const PathPair & pp1, const PathPair & pp2) {
	return (pp1.first == pp2.first && pp1.second == pp2.second) || (pp1.first == pp2.second && pp1.second == pp2.first);
}

inline bool isIdentical(const PathPairPair &ppp1, const PathPairPair &ppp2) {
	bool b1 = isIdentical(ppp1.first, ppp2.first) && isIdentical(ppp1.second, ppp2.second);
	bool b2 = isIdentical(ppp1.first, ppp2.second) && isIdentical(ppp1.second, ppp2.first);
	return (b1 || b2);
}

#endif // GRAPH_TYPES_H

