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

#include "gephilayoutnetgraph.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <QtXml>
#include "util.h"
#include "gephilayout.h"

bool GephiLayout::twoLevelLayout(NetGraph *g, ProgressCallback *progressCallback)
{
    // to make sure we have domains up to date
    g->computeASHulls();

    // build AS-level graph
    QVector<QList<int> > domainNodes;
    domainNodes.resize(g->domains.count());
    QVector<QList<QPointF> > domainNodePositions;
    domainNodePositions.resize(g->domains.count());
    QVector<QList<QPair<int, int> > > domainEdges;
    domainEdges.resize(g->domains.count());
    QVector<int> node2DomainNode;
    node2DomainNode.resize(g->nodes.count());
    foreach (NetGraphNode n, g->nodes) {
        node2DomainNode[n.index] = domainNodes[n.ASNumber].count();
        domainNodes[n.ASNumber] << n.index;
        domainNodePositions[n.ASNumber] << QPointF(n.x, n.y);
    }

    QSet<QPair<int, int> > interDomainEdges;
    foreach (NetGraphEdge e, g->edges) {
        if (g->nodes[e.source].ASNumber != g->nodes[e.dest].ASNumber) {
            interDomainEdges.insert(QPair<int, int>(g->nodes[e.source].ASNumber, g->nodes[e.dest].ASNumber));
        } else {
            domainEdges[g->nodes[e.source].ASNumber] << QPair<int, int>(node2DomainNode[e.source], node2DomainNode[e.dest]);
        }
    }

#if 1
    // run layout for router-level graph in each domain
    for (int id = 0; id < domainNodes.count(); id++) {
        if (progressCallback) {
            (*progressCallback)(id, 1 + domainNodes.count(), QString("Computing layout for AS %1").arg(id));
        }

        QVector<qreal> nodeSizes;
        nodeSizes.resize(domainNodePositions[id].count());
        nodeSizes.fill(NETGRAPH_NODE_RADIUS * 2);
        if (!runLayout(domainNodePositions[id], nodeSizes.toList(), domainEdges[id], "Random,YifanHu:100,ForceAtlas:1000,Center")) {
            return false;
        }
    }
#else
    for (int id = 0; id < domainNodes.count(); id++) {
        QPointF c;
        for (int in = 0; in < domainNodePositions[id].count(); in++) {
            c += QPointF(domainNodePositions[id][in].x() / domainNodePositions[id].count(), domainNodePositions[id][in].y() / domainNodePositions[id].count());
        }
        for (int in = 0; in < domainNodePositions[id].count(); in++) {
            domainNodePositions[id][in] -= c;
        }
    }
#endif

    // compute the size of each domain (radius of bounding circle centered in 0,0)
    QVector<qreal> domainSize;
    domainSize.resize(domainNodes.count());
    for (int id = 0; id < domainNodes.count(); id++) {
        domainSize[id] = NETGRAPH_NODE_RADIUS * 2;
        foreach (QPointF p, domainNodePositions[id]) {
            domainSize[id] = qMax(domainSize[id], sqrt(p.x() * p.x() + p.y() * p.y()));
        }
        // add a padding
        domainSize[id] += NETGRAPH_NODE_RADIUS * 5;
    }

#if 1
    qreal avgSize = 0;
    for (int i = 0; i < domainSize.count(); i++) {
        avgSize += domainSize[i] / domainSize.count();
    }
    for (int i = 1; i < domainSize.count(); i++) {
        domainSize[i] = qMax(domainSize[i], avgSize);
    }
#endif

    // run layout for domain-level graph
    if (progressCallback) {
        (*progressCallback)(domainNodes.count(), 1 + domainNodes.count(), QString("Computing layout for AS-level graph"));
    }
    QList<QPointF> domainPositions;
    for (int i = 0; i < domainNodes.count(); i++) {
        domainPositions << QPointF();
    }
    //if (!runLayout(domainPositions, domainSize.toList(), interDomainEdges.toList(), "Random,YifanHu:30,ForceAtlas:10000,Center")) {
    QString layoutConfig = "Random";
    for (int i = 0; i < 50; i++) {
        layoutConfig += ",YifanHu:10,ForceAtlas:10";
    }
    layoutConfig += ",ForceAtlas:1000,Center";
    if (!runLayout(domainPositions, domainSize.toList(), interDomainEdges.toList(), layoutConfig)) {
        return false;
    }

    // translate routers
    for (int id = 0; id < domainNodes.count(); id++) {
        for (int ir = 0; ir < domainNodePositions[id].count(); ir++) {
            domainNodePositions[id][ir] += domainPositions[id];
        }
    }

    // update netgraph
    for (int id = 0; id < domainNodes.count(); id++) {
        for (int ir = 0; ir < domainNodePositions[id].count(); ir++) {
            g->nodes[domainNodes[id][ir]].x = domainNodePositions[id][ir].x();
            g->nodes[domainNodes[id][ir]].y = domainNodePositions[id][ir].y();
        }
    }

    return true;
}

bool GephiLayout::oneLevelLayout(NetGraph *g)
{
    QList<QPointF> nodes;
    foreach (NetGraphNode n, g->nodes) {
        nodes << QPointF(n.x, n.y);
    }
    QList<QPair<int, int> > edges;
    foreach (NetGraphEdge e, g->edges) {
        edges << QPair<int, int>(e.source, e.dest);
    }
    QVector<qreal> nodeSizes;
    nodeSizes.resize(g->nodes.count());
    nodeSizes.fill(NETGRAPH_NODE_RADIUS * 2);

    if (!runLayout(nodes, nodeSizes.toList(), edges, "Random,ForceAtlas:10000,Center")) {
        return false;
    }

    foreach (NetGraphNode n, g->nodes) {
        g->nodes[n.index].x = nodes[n.index].x();
        g->nodes[n.index].y = nodes[n.index].y();
    }

    return true;
}

bool exportGexf(NetGraph *g, QString fileName, bool usedOnly)
{
    QDomDocument doc("");
    doc.appendChild(doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"ISO-8859-1\""));

    QDomElement gexf = doc.createElement("gexf");
    doc.appendChild(gexf);

    QDomElement graph = doc.createElement("graph");
    gexf.appendChild(graph);
    graph.setAttribute("mode", "static");
    graph.setAttribute("defaultedgetype", "directed");

    QDomElement nodes = doc.createElement("nodes");
    graph.appendChild(nodes);

    foreach (NetGraphNode n, g->nodes) {
        if (usedOnly && !n.used)
            continue;
        QDomElement node = doc.createElement("node");
        node.setAttribute("id", n.index);
        nodes.appendChild(node);
    }

    QDomElement edges = doc.createElement("edges");
    graph.appendChild(edges);

    foreach (NetGraphEdge e, g->edges) {
        if (usedOnly && !e.used)
            continue;
        QDomElement edge = doc.createElement("edge");
        edge.setAttribute("id", e.index);
        edge.setAttribute("source", e.source);
        edge.setAttribute("target", e.dest);
        if (g->nodes[e.source].ASNumber == g->nodes[e.dest].ASNumber) {
            edge.setAttribute("weight", 2.0);
        } else {
            edge.setAttribute("weight", 1.0);
        }
        edges.appendChild(edge);
    }

    return saveFile(fileName, doc.toString());
}

bool importGexfPositions(NetGraph *g, QString fileName)
{
    QDomDocument doc(fileName);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        fdbg << "Could not open file in read mode, file name:" << fileName;
        return false;
    }
    QString errorMsg;
    int errorLine;
    int errorColumn;
    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
        fdbg << "Could not read XML, file name:"
                << fileName + ":" + QString::number(errorLine) + ":" + QString::number(errorColumn) <<
                errorMsg;
        file.close();
        return false;
    }
    file.close();

    QDomElement gexf = doc.documentElement();

    QDomNodeList nodes = gexf.elementsByTagName("node");
    for (int iNode = 0; iNode < nodes.count(); iNode++) {
        QDomNode node = nodes.at(iNode);

        bool ok;
        int id = node.attributes().namedItem("id").toAttr().value().toInt(&ok);
        if (!ok || id < 0 || id >= g->nodes.count()) {
            qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
            return false;
        }

        QDomNodeList position = node.toElement().elementsByTagName("viz:position");
        if (position.count() != 1) {
            qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
            return false;
        }
        QDomNode pos = position.at(0);
        double x = pos.attributes().namedItem("x").toAttr().value().toDouble(&ok);
        if (!ok) {
            qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
            return false;
        }
        double y = pos.attributes().namedItem("y").toAttr().value().toDouble(&ok);
        if (!ok) {
            qDebug() << __FILE__ << __LINE__ << "gexf unreadable" << fileName << file.readAll();
            return false;
        }

        g->nodes[id].x = x;
        g->nodes[id].y = y;
    }

    return true;
}

bool GephiLayout::layout(NetGraph *g, bool usedOnly)
{
    // first try a tree layout
    if (treeLayout(g))
        return true;

    QTemporaryFile fileIn("qt_tempXXXXXX_in.gexf");
    if (!fileIn.open()) {
        qDebug() << __FUNCTION__ << "Could not open temp file";
        return false;
    }
    fileIn.close();

    QTemporaryFile fileOut("qt_tempXXXXXX_out.gexf");
    if (!fileOut.open()) {
        qDebug() << __FUNCTION__ << "Could not open temp file";
        return false;
    }
    fileOut.close();

    QString inputFileName = fileIn.fileName();
    QString outputFileName = fileOut.fileName();

    if (!exportGexf(g, inputFileName, usedOnly)) {
        qDebug() << "Could not export gexf";
        return false;
    }

    qDebug() << inputFileName << outputFileName;

    QString output;
    int exitCode;
    if (!runProcess("java", QStringList() << "-jar" << "../line-layout/gephi-toolkit-demos/dist/ToolkitDemos.jar" << inputFileName << outputFileName <<
                    "Random,OpenOrd,YifanHu:0",
                 output, -1, false, &exitCode) || exitCode != 0) {
        qDebug() << "Could not layout gexf" << output;
        return false;
    } else {
        // qDebug() << "Gephi says:" << output;
    }

    if (!importGexfPositions(g, outputFileName))
        return false;

#if 0
    // rescale if necessary
    const qreal lowerDistLimit = 50.0;
    qreal minNodeDist = 1.0e100;
    foreach (NetGraphEdge e, g->edges) {
        if (usedOnly && !e.used)
            continue;
        int i = e.source;
        int j = e.dest;
        qreal d = sqrt((g->nodes.at(i).x - g->nodes.at(j).x) * (g->nodes.at(i).x - g->nodes.at(j).x) + (g->nodes.at(i).y - g->nodes.at(j).y) * (g->nodes.at(i).y - g->nodes.at(j).y));
        if (d > 0) {
            minNodeDist = qMin(minNodeDist, d);
        }
    }
    if (minNodeDist < lowerDistLimit) {
        qreal rescale = lowerDistLimit/minNodeDist;
        qDebug() << "rescale" << rescale;
        for (int i = 0; i < g->nodes.count(); i++) {
            if (usedOnly && !g->nodes[i].used)
                continue;
            g->nodes[i].x *= rescale;
            g->nodes[i].y *= rescale;
        }
    }
#endif
    // center
    if (!g->nodes.isEmpty()) {
        qreal xc, yc;
        qreal count = 0;
        xc = yc = 0.0;
        for (int i = 0; i < g->nodes.count(); i++) {
            if (usedOnly && !g->nodes[i].used)
                continue;
            xc += g->nodes[i].x;
            yc += g->nodes[i].y;
            count++;
        }
        xc /= count;
        yc /= count;
        for (int i = 0; i < g->nodes.count(); i++) {
            if (usedOnly && !g->nodes[i].used)
                continue;
            g->nodes[i].x -= xc;
            g->nodes[i].y -= yc;
        }
    }
    return true;
}

#define DBG_TREE_LAYOUT 1
bool GephiLayout::treeLayout(NetGraph *g)
{
    const qreal rStep = 70.0;

    if (g->rootId < 0 || g->rootId >= g->nodes.count())
        return false;

    // the root is the node that is an endpoint for every path
    int root = g->rootId;

    // for every path starting from root, fill:
    // nodeChildren has the children of a node
    QVector<QSet<int> > nodeChildren;
    nodeChildren.resize(g->nodes.count());
    // nodeHeight is the distance from the path end (leaves have height == 0)
    QVector<int> nodeHeight;
    nodeHeight.resize(g->nodes.count());
    // nodeDepth is the distance from the root (the root has depth == 0)
    QVector<int> nodeDepth;
    nodeDepth.resize(g->nodes.count());
    nodeDepth.fill(INT_MAX);

    {
        QList<QList<NetGraphEdge> > adj = g->toAdjacencyList();

        QList<int> bfsQueue;
        QVector<int> parent;
        parent.resize(g->nodes.count());
        parent.fill(-1);

        QVector<bool> visited;
        visited.resize(g->nodes.count());

        bfsQueue << root;
        visited[root] = true;
        nodeDepth[root] = 0;
        while (!bfsQueue.isEmpty()) {
            int n = bfsQueue.takeFirst();
            foreach (NetGraphEdge e, adj[n]) {
                if (!visited[e.dest]) {
                    bfsQueue << e.dest;
                    visited[e.dest] = true;
                    nodeDepth[e.dest] = nodeDepth[n] + 1;
                    parent[e.dest] = n;
                    nodeChildren[n].insert(e.dest);
                }
            }
        }

        for (int i = 0; i < g->nodes.count(); i++) {
            if (nodeChildren[i].isEmpty() && visited[i]) {
                nodeHeight[i] = 0;
                for (int n = i; parent[n] >= 0; n = parent[n]) {
                    int newh = nodeHeight[n] + 1;
                    if (nodeHeight[parent[n]] < newh) {
                        nodeHeight[parent[n]] = newh;
                    } else {
                        break;
                    }
                }
            }
        }
    }



#if 0
    foreach (NetGraphPath p, g->paths) {
        if (p.source != root)
            continue;
        if (p.edgeList.isEmpty()) {
            if (DBG_TREE_LAYOUT) qDebug() << __FILE__ << __LINE__ << __FUNCTION__;
            return false;
        }
        for (int i = 0; i < p.edgeList.count(); i++) {
            int h = p.edgeList.count() - i;
            int d = i;
            int n = p.edgeList[i].source;
            int c = p.edgeList[i].dest;
            nodeHeight[n] = qMax(nodeHeight[n], h);
            nodeDepth[n] = qMin(nodeDepth[n], d);
            nodeChildren[n].insert(c);
            nodeDepth[c] = qMin(nodeDepth[c], d+1);
        }
    }
    for (int i = 0; i < g->nodes.count(); i++) {
        if (nodeDepth[i] == INT_MAX) {
            nodeDepth[i] = 0;
        }
    }
#endif

    // horizontal position in range [-0.5, 0.5]
    QVector<qreal> nodePos;
    nodePos.resize(g->nodes.count());

    {
        // node widths
        QVector<qreal> nodeWidth;
        nodeWidth.resize(g->nodes.count());
        for (int i = 0; i < g->nodes.count(); i++)
            nodeWidth[i] = 1.0;

        // do a BFS over the tree to get a node order
        QList<int> ordered;
        {
            QList<int> queue;
            QSet<int> visited;
            queue << root;
            visited.insert(root);
            while (!queue.isEmpty()) {
                int n = queue.takeFirst();
                ordered << n;
                QList<int> children = nodeChildren[n].toList();
                qSort(children);
                foreach (int c, children) {
                    if (!visited.contains(c)) {
                        queue << c;
                        visited.insert(c);
                    }
                }
            }
        }
        // go over the nodes in reverse to compute node width bottom up
        for (int i = ordered.count() - 1; i >= 0; i--) {
            int n = ordered[i];
            if (nodeChildren[n].isEmpty()) {
                nodeWidth[n] = 20.0;
                continue;
            }
            nodeWidth[n] = 0;
            foreach (int c, nodeChildren[n]) {
                nodeWidth[n] += nodeWidth[c];
            }
        }

        // go over the nodes to compute node pos top bottom
        for (int i = 0; i < ordered.count(); i++) {
            int n = ordered[i];
            if (n == root) {
                nodePos[n] = 0.0;
            }
            if (nodeChildren[n].isEmpty())
                continue;
            qreal left = nodePos[n] - nodeWidth[n]/2.0;
            QList<int> children = nodeChildren[n].toList();
            qSort(children);
            for (int ic = 0; ic < children.count(); ic++) {
                int c = children[ic];
                nodePos[c] = left + nodeWidth[c]/2.0;
                left += nodeWidth[c];
            }
        }

        if (nodeWidth[root] > 0) {
            for (int i = 0; i < g->nodes.count(); i++) {
                nodePos[i] /= nodeWidth[root];
            }
        }
    }

    // now convert nodePos to x y
    for (int i = 0; i < g->nodes.count(); i++) {
        qreal r = nodeDepth[i] * rStep;
        qreal alpha = nodePos[i] * 2 * M_PI;
        qreal x = r * cos(alpha);
        qreal y = r * sin(alpha);
        g->nodes[i].x = x;
        g->nodes[i].y = y;
    }

    return true;
}
