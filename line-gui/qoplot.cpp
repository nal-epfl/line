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

#include "qoplot.h"

#include <QtOpenGL>
#include <QSvgGenerator>
#include <limits>
#include <math.h>

#include "nicelabel.h"
#include "util.h"

void invertLigthness(QColor &c)
{
	c = QColor::fromHslF(c.hueF(), c.saturationF(), 1.0 - c.lightnessF(), c.alphaF());
}

static QString escapeLatexJson(QString s) {
	return s.replace("\\", "\\\\textbackslash")
			.replace("%", "\\\\%")
			.replace("$", "\\\\$")
			.replace("_", "\\\\_")
			.replace("{", "\\\\{")
			.replace("}", "\\\\}")
			.replace("&", "\\\\&")
			.replace("#", "\\\\#")
			.replace("|", "\\\\textbar")
			.replace("<", "\\\\textless")
			.replace(">", "\\\\textgreater");
}

QOPlot::QOPlot()
{
	xGridVisible = true;
	xSISuffix = false;
	yGridVisible = true;
	ySISuffix = false;
	legendVisible = true;
	legendPosition = BottomOutside;
	legendAlpha = 200;

	backgroundColor = Qt::white;
	foregroundColor = Qt::black;
	legendBackground = QColor(240, 240, 240);

	drag_x_enabled = drag_y_enabled = zoom_x_enabled = zoom_y_enabled = true;
	autoAdjusted = true;
	autoAdjustedFixedAspect = fixedAxes = false;
	includeOriginX = includeOriginY = true;
}

QOPlot::~QOPlot()
{
	data.clear();
}

void QOPlot::differentColors()
{
	const qreal hueInitial = 0;
	const qreal hueStep = 0.618033988749895;
	const qreal satInitial = 0.95;
	const qreal satStep = -0.05;
	const qreal satMin = 0.5;
	const qreal valInitial = 0.75;
	const qreal valStep = -0.05;
	const qreal valMin = 0.50;
	qreal hue = hueInitial;
	qreal sat = satInitial;
	qreal val = valInitial;
	foreach (QSharedPointer<QOPlotData> dataItem, data) {
		QColor color = QColor::fromHsvF(hue, sat, val, 1);
		hue += hueStep;
		if (hue > 1.0) {
			hue -= 1.0;
			sat += satStep;
			if (sat < satMin) {
				sat = satInitial;
				val += valStep;
				if (val < valMin) {
					val = valInitial;
				}
			}
		}
		if (dataItem->getDataType() == "line") {
			QSharedPointer<QOPlotCurveData> item = qSharedPointerCast<QOPlotCurveData>(dataItem);
			if (!item)
				continue;
			item->pen.setColor(color);
		} else if (dataItem->getDataType() == "stem") {
			QSharedPointer<QOPlotStemData> item = qSharedPointerDynamicCast<QOPlotStemData>(dataItem);
			if (!item)
				continue;
			item->pen.setColor(color);
		} else if (dataItem->getDataType() == "scatter") {
			QSharedPointer<QOPlotScatterData> item = qSharedPointerDynamicCast<QOPlotScatterData>(dataItem);
			if (!item)
				continue;
			item->pen.setColor(color);
		} else if (dataItem->getDataType() == "boxplot") {
			QSharedPointer<QOPlotBoxPlotData> item = qSharedPointerDynamicCast<QOPlotBoxPlotData>(dataItem);
			if (!item)
				continue;
			// TODO
		} else if (dataItem->getDataType() == "band") {
			QSharedPointer<QOPlotBandData> item = qSharedPointerCast<QOPlotBandData>(dataItem);
			if (!item)
				continue;

			QColor medianColor = color;
			QColor bandColor = QColor::fromRgbF(color.redF(), color.greenF(), color.blueF(), 0.5);
			QColor bandWhiskersColor = QColor::fromRgbF(color.redF(), color.greenF(), color.blueF(), 0.25);
			item->centralPen.setColor(medianColor);
			item->brush.setColor(bandColor);
			item->brushWhiskers.setColor(bandWhiskersColor);
		}
	}
}

QOPlotHit QOPlot::getHits(qreal x, qreal y, qreal aspectRatio, qreal distanceThreshold)
{
	QOPlotHit result;
	result.plot = this;
	for (int i = 0; i < data.count(); i++) {
		QOPlotDataHit hit = data[i]->getHits(x, y, aspectRatio);
		if (hit.pointIndex < 0)
			continue;
		if (hit.distance <= distanceThreshold) {
			hit.dataIndex = i;
			if (data[i]->pointUserData.count() > hit.pointIndex) {
				hit.userData = data[i]->pointUserData[hit.pointIndex];
			}
			result.hits << hit;
		}
	}
	if (!result.hits.isEmpty()) {
		result.bestHit = result.hits.first();
	}
	for (int i = 0; i < result.hits.count(); i++) {
		if (result.hits[i].distance < result.bestHit.distance) {
			result.bestHit = result.hits[i];
		}
	}
	return result;
}

QVector<QPointF> QOPlot::makePoints(QList<qreal> v)
{
	QVector<QPointF> result;
	result.reserve(v.count());
	for (int i = 0; i < v.count(); i++)
		result.append(QPointF(i, v[i]));
	return result;
}

QVector<QPointF> QOPlot::makeCDF(QList<qreal> v)
{
	if (v.isEmpty())
		return makeCDF(v, 0, 0);
	qreal min, max;
	min = max = v.first();
	for (int i = 0; i < v.count(); i++) {
		min = qMin(min, v.at(i));
		max = qMax(max, v.at(i));
	}
	return makeCDF(v, min, max);
}

QVector<QPointF> QOPlot::makeCDF(QList<qreal> v, qreal min)
{
	if (v.isEmpty())
		return makeCDF(v, min, min);
	qreal max;
	max = v.first();
	for (int i = 0; i < v.count(); i++) {
		max = qMax(max, v.at(i));
	}
	return makeCDF(v, min, max);
}

QVector<QPointF> QOPlot::makeCDF(QList<qreal> v, qreal min, qreal max)
{
	QVector<QPointF> cdf;
	cdf.reserve(v.count());
	if (v.isEmpty())
		return cdf;

	qSort(v);
	min = qMin(min, v.first());
	max = qMax(max, v.last());
	// add a line from (min,0)
	cdf.append(QPointF(min, 0.0));
	for (int i = 0; i < v.count(); i++) {
		if (cdf.last().x() != v[i] || cdf.count() == 1) {
			// horizontal line to the right
			if (cdf.last().x() != v[i]) {
				cdf.append(QPointF(v[i], cdf.last().y()));
			}
			// vertical line up
			cdf.append(QPointF(v[i], (i+1.0)/v.count()));
		} else {
			cdf.last().setY((i+1.0)/v.count());
		}
	}
	// the last y is always 1.0
	// add a line to (max,1)
	if (cdf.last().x() != max) {
		cdf.append(QPointF(max, cdf.last().y()));
	}

	return cdf;
}

QVector<QPointF> QOPlot::makeCDFSampled(QList<qreal> v, int binCount)
{
	if (v.isEmpty())
		return QVector<QPointF>();
	qreal min, max;
	min = max = v.first();
	for (int i = 0; i < v.count(); i++) {
		min = qMin(min, v.at(i));
		max = qMax(max, v.at(i));
	}

	return makeCDFSampled(v, min, max, binCount);
}

QVector<QPointF> QOPlot::makeCDFSampled(QList<qreal> v, qreal min, int binCount)
{
	if (v.isEmpty())
		return QVector<QPointF>();
	qreal max;
	max = v.first();
	for (int i = 0; i < v.count(); i++) {
		max = qMax(max, v.at(i));
	}

	return makeCDFSampled(v, min, max, binCount);
}

QVector<QPointF> QOPlot::makeCDFSampled(QList<qreal> v, qreal min, qreal max, int binCount)
{
	qreal binWidth;
	QVector<QPointF> h = makeHistogram(v, binWidth, binCount);
	binCount = h.count();

	QVector<QPointF> cdf;
	cdf.reserve(binCount+1);
	if (v.isEmpty())
		return cdf;

	cdf.append(QPointF(min, 0.0));
	if (!qFuzzyCompare(min, h.first().x() - binWidth/2.0)) {
		cdf.append(QPointF(h.first().x() - binWidth/2.0, 0.0));
	}
	for (int i = 0; i < h.count(); i++) {
		cdf.append(QPointF(h[i].x() - binWidth/2.0, cdf.last().y() + h[i].y()));
	}
	cdf.append(QPointF(max, cdf.last().y()));
	// normalize
	for (int i = 0; i < cdf.count(); i++) {
		cdf[i].setY(cdf[i].y() / cdf.last().y());
	}

	return cdf;
}

QVector<QPointF> QOPlot::makeHistogram(QList<qreal> v, qreal &binWidth, int binCount)
{
	QVector<QPointF> h;
	// compute number of bins
	if (binCount <= 0) {
		binCount = qMax(1, qRound(ceil(log2(v.count()))));
	}
	if (binCount <= 0)
		return h;
	h.reserve(binCount);

	// compute bin width
	qreal min, max;
	min = max = v.first();
	for (int i = 0; i < v.count(); i++) {
		min = qMin(min, v.at(i));
		max = qMax(max, v.at(i));
	}
	binWidth = (max - min)/(qreal)binCount;

	// initialize
	for (int iBin = 0; iBin < binCount; iBin++) {
		h.append(QPointF(min + binWidth * (iBin + 0.5), 0.0));
	}

	// fill
	for (int i = 0; i < v.count(); i++) {
		int iBin = qMax(qMin(qRound((v[i] - min) / binWidth), binCount - 1), 0);
		h[iBin].setY(h[iBin].y() + 1.0);
	}
	return h;
}

qreal QOPlot::medianSorted(QList<qreal> v)
{
	if (v.isEmpty())
		return 0.0;
	if (v.count() % 2 == 1) {
		return v[v.count() / 2];
	} else {
		return (v[v.count() / 2 - 1] + v[v.count() / 2])/2.0;
	}
}

qreal QOPlot::median(QList<qreal> v)
{
	qSort(v);
	return medianSorted(v);
}

void QOPlot::quartiles(QList<qreal> v, qreal &q1, qreal &q2, qreal &q3)
{
	qSort(v);
	q2 = medianSorted(v);
	if (v.count() % 2 == 1) {
		q1 = medianSorted(v.mid(0, v.count() / 2 + 1));
		q3 = medianSorted(v.mid(v.count() / 2));
	} else {
		q1 = medianSorted(v.mid(0, v.count() / 2));
		q3 = medianSorted(v.mid(v.count() / 2));
	}
}

QOPlotBoxPlotItem QOPlot::makeBoxWhiskers(QList<qreal> v, QString title, QColor color, bool withHist, bool withCDF)
{
	QOPlotBoxPlotItem box;
	box.data = v.toVector();
	box.legendLabel = title;
	box.pen.setColor(color);

	if (!v.isEmpty()) {
		quartiles(v, box.quartileLow, box.quartileMed, box.quartileHigh);
		qreal iqr = box.quartileHigh - box.quartileLow;
		// outlier bounds
		qreal lowerLimit = box.quartileLow - 1.5 * iqr;
		qreal upperLimit = box.quartileHigh + 1.5 * iqr;
		// whiskers: the lowest datum still within 1.5 IQR of the lower quartile
		// and the highest datum still within 1.5 IQR of the upper quartile
		box.whiskerLow = v.first();
		box.whiskerHigh = v.first();
		for (int i = 0; i < v.count(); i++) {
			if (box.quartileLow < v[i] && v[i] < box.quartileHigh)
				continue;
			if (v[i] < lowerLimit) {
				box.outliers.append(v[i]);
			} else if (lowerLimit <= v[i] && v[i] <= box.quartileLow) {
				if (qIsNaN(box.whiskerLow)) {
					box.whiskerLow = v[i];
				} else {
					if (v[i] < box.whiskerLow) {
						box.whiskerLow = v[i];
					}
				}
			} else if (box.quartileHigh <= v[i] && v[i] <= upperLimit) {
				if (qIsNaN(box.whiskerHigh)) {
					box.whiskerHigh = v[i];
				} else {
					if (box.whiskerHigh < v[i]) {
						box.whiskerHigh = v[i];
					}
				}
			} else if (v[i] > upperLimit) {
				box.outliers.append(v[i]);
			}
		}
	}

	if (withCDF) {
		box.cdf = makeCDFSampled(v);
	}
	if (withHist && !v.isEmpty()) {
		box.histogram = makeHistogram(v, box.histogramBinWidth);
		qreal hMax = box.histogram.first().y();
		for (int i = 0; i < box.histogram.count(); i++) {
			hMax = qMax(hMax, box.histogram[i].y());
		}
		// normalize
		if (hMax > 0) {
			for (int i = 0; i < box.histogram.count(); i++) {
				box.histogram[i].setY(box.histogram[i].y() / hMax);
			}
		}
	}
	return box;
}

QOPlotCurveData *QOPlotCurveData::fromPoints(QVector<QPointF> points, QString legendLabel, QColor color, QString pointSymbol)
{
	QOPlotCurveData *curve = new QOPlotCurveData;
	for (int i = 0; i < points.count(); i++) {
		curve->x.append(points[i].x());
		curve->y.append(points[i].y());
	}
	curve->legendLabel = legendLabel;
	curve->pen.setColor(color);
	curve->pointSymbol = pointSymbol;
	return curve;
}

QOPlotCurveData *QOPlot::makeLine(qreal x1, qreal y1, qreal x2, qreal y2, QColor c)
{
	QOPlotCurveData *line = new QOPlotCurveData();
	line->legendVisible = false;
	line->pen.setColor(c);
	line->pen.setDashPattern(QVector<qreal>(2, 5.0));
	line->x.append(x1);
	line->y.append(y1);
	line->x.append(x2);
	line->y.append(y2);
	return line;
}

QOPlotDataHit::QOPlotDataHit()
{
	dataIndex = -1;
	pointIndex = -1;
	userData = -1;
	distance = 1.0e99;
}

QOPlotData::QOPlotData()
{
	legendVisible = true;
	legendLabel = "";
	legendLabelColor = Qt::black;
}

QOPlotDataHit QOPlotData::getHits(qreal x, qreal y, qreal aspectRatio)
{
	Q_UNUSED(x);Q_UNUSED(y);Q_UNUSED(aspectRatio);
	QOPlotDataHit result;
	return result;
}

QOPlotCurveData::QOPlotCurveData() : QOPlotData()
{
	dataType = "line";
    pen = QPen(Qt::black, 1.0, Qt::SolidLine);
	pen.setCosmetic(true);
	pointSymbol = QString();
	symbolSize = 6.0;
	x = QVector<qreal>();
	y = QVector<qreal>();
}

QOPlotDataHit QOPlotCurveData::getHits(qreal cx, qreal cy, qreal aspectRatio)
{
	QOPlotDataHit result;
	if (x.isEmpty())
		return result;

	result.pointIndex = 0;
	result.distance = sqrt((x[0] - cx)*(x[0] - cx) + aspectRatio*aspectRatio * (y[0] - cy)*(y[0] - cy));
	for (int i = 0; i < x.count(); i++) {
		qreal distance = sqrt((x[i] - cx)*(x[i] - cx) + aspectRatio*aspectRatio * (y[i] - cy)*(y[i] - cy));
		if (distance < result.distance) {
			result.pointIndex = i;
			result.distance = distance;
		}
	}
	return result;
}

QString QOPlotCurveData::toJson()
{
	QString result;
	QTextStream out(&result);
	out << "{" << endl;
	out << QString("\"type\": \"line\",") << endl;
	out << QString("\"x\": [");
	{
		bool first = true;
		foreach (qreal v, x) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << v;
		}
		out << QString("],") << endl;
	}
	out << QString("\"y\": [");
	{
		bool first = true;
		foreach (qreal v, y) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << v;
		}
		out << QString("],") << endl;
	}
	out << QString("\"pattern\": \"%1%2\",")
		   .arg(pointSymbol)
		   .arg(dataType == "scatter" ? "" : "-") << endl;
	out << QString("\"label\": \"%1\",").arg(escapeLatexJson(legendLabel)) << endl;
	out << QString("\"color\": [%1, %2, %3]")
		   .arg(pen.color().hueF())
		   .arg(pen.color().saturationF())
		   .arg(pen.color().valueF()) << endl;
	out << "}" << endl;
	out.flush();
	return result;
}

void QOPlotCurveData::boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax)
{
	if (x.count() > 0) {
		xmin = xmax = x[0];
		ymin = ymax = y[0];
	} else {
		xmin = 0; xmax = 1;
		ymin = 0; ymax = 1;
	}
	for (int i = 0; i < x.count(); i++) {
		if (x[i] < xmin)
			xmin = x[i];
		if (x[i] > xmax)
			xmax = x[i];
		if (y[i] < ymin)
			ymin = y[i];
		if (y[i] > ymax)
			ymax = y[i];
	}
}

QOPlotStemData::QOPlotStemData() : QOPlotCurveData()
{
	dataType = "stem";
	pointSymbol = "o";
}

QOPlotScatterData::QOPlotScatterData() : QOPlotCurveData()
{
	dataType = "scatter";
	pointSymbol = "o";
}

QOPlotScatterData *QOPlotScatterData::fromPoints(QVector<QPointF> points, QString legendLabel, QColor color, QString pointSymbol)
{
	QOPlotScatterData *curve = new QOPlotScatterData;
	for (int i = 0; i < points.count(); i++) {
		curve->x.append(points[i].x());
		curve->y.append(points[i].y());
	}
	curve->legendLabel = legendLabel;
	curve->pen.setColor(color);
	curve->pointSymbol = pointSymbol;
	return curve;
}

QOPlotBoxPlotItem::QOPlotBoxPlotItem()
{
	pen = QPen(Qt::blue, 2.0, Qt::SolidLine);
	pen.setCosmetic(true);
	quartileLow = quartileMed = quartileHigh = whiskerLow = whiskerHigh = 0.0;
}

QOPlotBoxPlotItem::QOPlotBoxPlotItem(qreal wLow, qreal q1, qreal q2, qreal q3, qreal wHigh, QVector<qreal> outliers) :
	quartileLow(q1), quartileMed(q2), quartileHigh(q3), whiskerLow(wLow), whiskerHigh(wHigh), outliers(outliers)
{
	pen = QPen(Qt::blue, 2.0, Qt::SolidLine);
	pen.setCosmetic(true);
}

QOPlotBoxPlotData::QOPlotBoxPlotData() : QOPlotData()
{
	dataType = "boxplot";
	pointSymbol = "x";
	symbolSize = 6.0;
	boxWidth = 0.7;
	itemWidth = 1.0;
	items = QVector<QOPlotBoxPlotItem>();
}

QString QOPlotBoxPlotData::toJson()
{
	QString result;
	QTextStream out(&result);
	out << "{" << endl;
	out << QString("\"type\": \"boxplot\",") << endl;
	out << QString("\"x\": [") << endl;
	for	(int iBox = 0; iBox < items.count(); iBox++) {
		out << QString("[");
		bool first = true;
		foreach (qreal v, items[iBox].data) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << v;
		}
		out << QString("]");
		if (iBox < items.count() - 1) {
			out << ",";
		}
		out << endl;
	}
	out << "]" << endl;
	out << "}" << endl;
	out.flush();
	return result;
}

void QOPlotBoxPlotData::boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax)
{
	if (items.isEmpty()) {
		xmin = 0; xmax = 1;
		ymin = 0; ymax = 1;
	} else {
		xmin = 1.0 - itemWidth/2;
		xmax = 1.0 + items.count() * itemWidth + itemWidth/2;

		for (int i = 0; i < items.count(); i++) {
			if (i == 0) {
				ymin = items[i].whiskerLow;
				ymax = items[i].whiskerHigh;
			}
			ymin = qMin(ymin, items[i].whiskerLow);
			ymax = qMax(ymax, items[i].whiskerHigh);
			for (int j = 0; j < items[i].outliers.count(); j++) {
				ymin = qMin(ymin, items[i].outliers[j]);
				ymax = qMax(ymax, items[i].outliers[j]);
			}
		}
	}
}

QOPlotBandData::QOPlotBandData() : QOPlotData()
{
	dataType = "band";
	centralPen = QPen(Qt::blue, 1.0, Qt::SolidLine);
	centralPen.setCosmetic(true);
	lowerPen = upperPen = Qt::NoPen;
	lowerPen.setCosmetic(true);
	upperPen.setCosmetic(true);
	brush = QBrush(QColor::fromRgbF(0, 0, 1, 0.5));
	brushWhiskers = QBrush(QColor::fromRgbF(0, 0, 1, 0.25));
	x = yLower = yUpper = yCentral = yWhiskerLower = yWhiskerUpper = QVector<qreal>();
}

void QOPlotBandData::boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax)
{
	if (x.count() > 0) {
		xmin = xmax = x[0];
		ymin = ymax = yLower[0];
	} else {
		xmin = 0; xmax = 1;
		ymin = 0; ymax = 1;
	}
	for (int i = 0; i < x.count(); i++) {
		xmin = qMin(xmin, x[i]);
		xmax = qMax(xmax, x[i]);
		ymin = qMin(ymin, yLower[i]);
		ymin = qMin(ymin, yUpper[i]);
		ymax = qMax(ymax, yLower[i]);
		ymax = qMax(ymax, yUpper[i]);
	}
}

QOPlotBandData *QOPlotBandData::fromPoints(QVector<QPointF> points, int binCount, QString legendLabel, QColor medianColor, QColor bandColor, QColor bandWhiskersColor)
{
	QOPlotBandData *band = new QOPlotBandData;

	if (!points.isEmpty()) {
		qreal xmin, xmax;
		xmin = xmax = points[0].x();
		for (int i = 0; i < points.count(); i++) {
			xmin = qMin(xmin, points[i].x());
			xmax = qMax(xmax, points[i].x());
		}

		// bin points
		QList<QList<QPointF> > bins;
		for (int i = 0; i < binCount; i++) {
			bins << QList<QPointF>();
		}
		for (int i = 0; i < points.count(); i++) {
			int iBin = (int)((points[i].x() - xmin) / (xmax - xmin) * binCount);
			iBin = qMin(iBin, binCount-1);
			iBin = qMax(iBin, 0);
			bins[iBin] << points[i];
		}

		// compute min, max, median for each bin
		for (int i = 0; i < binCount; i++) {
			if (bins[i].isEmpty())
				continue;
			QList<qreal> x, y;
			for (int j = 0; j < bins[i].count(); j++) {
				x << bins[i][j].x();
				y << bins[i][j].y();
			}
			qreal xMed, yQ1, yMed, yQ3;
			QOPlot::quartiles(y, yQ1, yMed, yQ3);
			xMed = QOPlot::median(x);

			qreal iqr = yQ3 - yQ1;
			// outlier bounds
			qreal lowerLimit = yQ1 - 1.5 * iqr;
			qreal upperLimit = yQ3 + 1.5 * iqr;
			// whiskers: the lowest datum still within 1.5 IQR of the lower quartile, and the highest datum still within 1.5 IQR of the upper quartile
			qreal yLow, yUp;
			yLow = yQ1;
			yUp = yQ3;
			foreach (qreal v, y) {
				if (lowerLimit <= v && v < yLow)
					yLow = v;
				if (v <= upperLimit && v > yUp)
					yUp = v;
			}

			band->x.append(xMed);
			band->yWhiskerLower.append(yLow);
			band->yLower.append(yQ1);
			band->yCentral.append(yMed);
			band->yUpper.append(yQ3);
			band->yWhiskerUpper.append(yUp);
		}
	}
	band->legendLabel = legendLabel;
	band->centralPen.setColor(medianColor);
	band->brush.setColor(bandColor);
	band->brushWhiskers.setColor(bandWhiskersColor);
	return band;
}

class QGraphicsDummyItem : public QGraphicsItem
{
public:
	explicit QGraphicsDummyItem(QGraphicsItem *parent = 0) : QGraphicsItem(parent) {
		setFlag(QGraphicsItem::ItemHasNoContents);
	}

	QRectF bbox;
	QRectF boundingRect() const
	{
		return bbox;
	}

	void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *)
	{
	}
};

class QGraphicsAATextItem : public QGraphicsTextItem
{
public:
	explicit QGraphicsAATextItem(QGraphicsItem *parent = 0) : QGraphicsTextItem(parent) { }
	explicit QGraphicsAATextItem(const QString &text, QGraphicsItem *parent = 0) : QGraphicsTextItem(text, parent) { }

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPainter::RenderHints hints = painter->renderHints();
		painter->setRenderHint(QPainter::Antialiasing, true);
		QGraphicsTextItem::paint(painter, option, widget);
		painter->setRenderHints(hints, false);
		painter->setRenderHints(hints, true);
	}
};

QOPlotWidget::QOPlotWidget(QWidget *parent, int plotMinWidth, int plotMinHeight,
			QSizePolicy policy) :
	QWidget(parent), plotMinWidth(plotMinWidth), plotMinHeight(plotMinHeight)
{
	setStyle(NULL);
	setStyleSheet("QWidget { border: 0px; }");
	setMinSize(plotMinWidth, plotMinHeight);
	setSizePolicy(policy);
	setFocusPolicy(Qt::StrongFocus);
	scenePlot = new QGraphicsScene(this);
	viewPlot = new QOGraphicsView(scenePlot, this);

	scenePlot->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
//	scenePlot->setItemIndexMethod(QGraphicsScene::NoIndex);
	viewPlot->setGeometry(0, 0, width(), height());
	viewPlot->setStyle(NULL);
	viewPlot->setAlignment(0);
	viewPlot->setRenderHint(QPainter::Antialiasing, false);
	viewPlot->setDragMode(QGraphicsView::NoDrag);
	viewPlot->setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
	viewPlot->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);

	connect(viewPlot, SIGNAL(mouseMoved(QMouseEvent*)), SLOT(mouseMoveEvent(QMouseEvent*)));
	connect(viewPlot, SIGNAL(mousePressed(QMouseEvent*)), SLOT(mousePressEvent(QMouseEvent*)));
	connect(viewPlot, SIGNAL(mouseReleased(QMouseEvent*)), SLOT(mouseReleaseEvent(QMouseEvent*)));
	connect(viewPlot, SIGNAL(wheelMoved(QWheelEvent*)), SLOT(wheelEvent(QWheelEvent*)));

	marginLeft = marginRight = marginTop = marginBottom = 50;
	extraMargin = 15;
	world_x_off = 0;
	world_y_off = 0;
	world_press_x = NAN;
	world_press_y = NAN;
	zoomX = zoomY = 1.0;

	track_x = track_y = 0;
	tracked = false;

	itemPlot = new QGraphicsDummyItem();
	scenePlot->addItem(itemPlot);
	itemPlot->setZValue(1);

	itemLegend = new QGraphicsDummyItem();
	scenePlot->addItem(itemLegend);
	itemLegend->setZValue(1000);

	bg = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	bg->setZValue(-1000);

	axesBgLeft = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgLeft->setZValue(100);
	axesBgRight = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgRight->setZValue(100);
	axesBgTop = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgTop->setZValue(100);
	axesBgBottom = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgBottom->setZValue(100);
	plotAreaBorder = scenePlot->addRect(0, 0, 1, 1, QPen(plot.foregroundColor), QBrush(Qt::NoBrush));
	plotAreaBorder->setZValue(200);

	xLabel = scenePlot->addText("X Axis");
	xLabel->setDefaultTextColor(plot.foregroundColor);
	xLabel->setZValue(300);
	yLabel = scenePlot->addText("Y Axis");
	yLabel->setDefaultTextColor(plot.foregroundColor);
	yLabel->setZValue(300);
	titleLabel = scenePlot->addText("Title");
	titleLabel->setDefaultTextColor(plot.foregroundColor);
	titleLabel->setZValue(300);

	drawPlot();
}

void QOPlotWidget::setPlot(QOPlot plot)
{
	this->plot = plot;
	if (plot.autoAdjusted) {
		autoAdjustAxes();
	} else if (plot.autoAdjustedFixedAspect) {
		autoAdjustAxes(plot.autoAspectRatio);
	} else if (plot.fixedAxes) {
		fixAxes(plot.fixedXmin, plot.fixedXmax, plot.fixedYmin, plot.fixedYmax);
	}
	drawPlot();
}

void QOPlotWidget::setAntiAliasing(bool enable)
{
	if (enable) {
		viewPlot->setRenderHint(QPainter::Antialiasing, true);
		viewPlot->setOptimizationFlags(QGraphicsView::DontSavePainterState);
	} else {
		viewPlot->setRenderHint(QPainter::Antialiasing, false);
		viewPlot->setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
	}
}

void QOPlotWidget::test()
{
	plot.xlabel = "Teh X axis";
	plot.ylabel = "Teh Y axis";
	plot.title = "Awesome example";

	QOPlotCurveData *curve1 = new QOPlotCurveData;
	curve1->x.append(-10);
	curve1->y.append(-10);
	curve1->x.append(0);
	curve1->y.append(0);
	curve1->x.append(10);
	curve1->y.append(10);
	curve1->pen.setColor(Qt::blue);
	curve1->pen.setStyle(Qt::SolidLine);
	curve1->pointSymbol = "*";
	curve1->legendLabel = "Blah";
	plot.data << QSharedPointer<QOPlotData>(curve1);

	QOPlotCurveData *curve2 = new QOPlotCurveData;
	curve2->x.append(-5);
	curve2->y.append(-10);
	curve2->x.append(5);
	curve2->y.append(10);
	curve2->pen.setColor(Qt::green);
	curve2->pen.setStyle(Qt::DotLine);
	curve2->pointSymbol = "o";
	curve2->legendLabel = "Boo";
	plot.data << QSharedPointer<QOPlotData>(curve2);

	QOPlotStemData *stem1 = new QOPlotStemData;
	for (double x = -10; x  <= 10.001; x += 2) {
		stem1->x.append(x);
		stem1->y.append(0.1 + sqrt(fabs(x)));
	}
	stem1->pen.setColor(Qt::red);
	stem1->legendLabel = "Teh stem";
	plot.data << QSharedPointer<QOPlotData>(stem1);

	QOPlotScatterData *scatter1 = new QOPlotScatterData;
	for (double x = -10; x  <= 10.001; x += 2) {
		scatter1->x.append(x);
		scatter1->y.append(0.5 + 3 * sqrt(fabs(x)));
	}
	scatter1->pen.setColor(Qt::gray);
	scatter1->legendLabel = "Teh scatter";
	plot.data << QSharedPointer<QOPlotData>(scatter1);

	drawPlot();
}

void QOPlotWidget::testHuge()
{
	plot.xlabel = "Teh X axis";
	plot.ylabel = "Teh Y axis";
	plot.title = "Awesome example (huge)";

	qreal xmin, xmax, step;
	xmin = 0;
	xmax = 1000;
	step = 0.1;

	QOPlotCurveData *curve1 = new QOPlotCurveData;

	for (double x = xmin;  x <= xmax; x += step) {
		curve1->x.append(x);
		curve1->y.append(sin(x));
	}
	curve1->pen.setColor(Qt::blue);
	curve1->legendLabel = "sin(x)";
	plot.data << QSharedPointer<QOPlotData>(curve1);

	drawPlot();
}

void QOPlotWidget::testBoxplot()
{
	plot.xlabel = "Teh groups";
	plot.ylabel = "Teh values";
	plot.title = "Boxplot example";

	QOPlotBoxPlotData *boxes = new QOPlotBoxPlotData;

	bool showCDF = true;
	bool showHist = true;
	QList<int> throwValues = QList<int>() << 1000 << 10000;
	int totalGraphs = 4 * throwValues.count();
	foreach (int throws, throwValues) {
		QColor color;
		QList<qreal> bino;
		for (int i = 0; i < throws; i++) {
			qreal dice1 = 1 + (rand() / (qreal)RAND_MAX) * 6;
			qreal dice2 = 1 + (rand() / (qreal)RAND_MAX) * 6;
			bino.append(dice1 + dice2);
		}
		color = QColor::fromHsvF(boxes->items.count() / (qreal)totalGraphs, 1, 0.8, 1.0);
		QOPlotBoxPlotItem box1 = QOPlot::makeBoxWhiskers(bino, QString("2 real dice, %1 throws").arg(throws), color, showHist, showCDF);
		boxes->items << box1;

		QList<qreal> uni;
		for (int i = 0; i < throws; i++) {
			qreal dice1 = 1 + (rand() / (qreal)RAND_MAX) * 6;
			uni.append(dice1);
		}
		color = QColor::fromHsvF(boxes->items.count() / (qreal)totalGraphs, 1, 0.8, 1.0);
		QOPlotBoxPlotItem box2 = QOPlot::makeBoxWhiskers(uni, QString("1 real dice, %1 throws").arg(throws), color, showHist, showCDF);
		boxes->items << box2;

		QList<qreal> bino2;
		for (int i = 0; i < throws; i++) {
			qreal dice1 = 1 + qRound((rand() / (qreal)RAND_MAX) * 6);
			qreal dice2 = 1 + qRound((rand() / (qreal)RAND_MAX) * 6);
			bino2.append(dice1 + dice2);
		}
		color = QColor::fromHsvF(boxes->items.count() / (qreal)totalGraphs, 1, 0.8, 1.0);
		QOPlotBoxPlotItem box3 = QOPlot::makeBoxWhiskers(bino2, QString("2 int dice, %1 throws").arg(throws), color, showHist, showCDF);
		boxes->items << box3;

		QList<qreal> uni2;
		for (int i = 0; i < throws; i++) {
			qreal dice1 = 1 + qRound((rand() / (qreal)RAND_MAX) * 6);
			uni2.append(dice1);
		}
		color = QColor::fromHsvF(boxes->items.count() / (qreal)totalGraphs, 1, 0.8, 1.0);
		QOPlotBoxPlotItem box4 = QOPlot::makeBoxWhiskers(uni2, QString("1 int dice, %1 throws").arg(throws), color, showHist, showCDF);
		boxes->items << box4;
	}

	plot.addData(boxes);
	plot.xGridVisible = false;
	plot.yGridVisible = false;

	drawPlot();
}

void drawPlus(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	QGraphicsLineItem *line = new QGraphicsLineItem(size/2.0, 0, size/2.0, 0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	line = new QGraphicsLineItem(0, -size/2.0, 0, size/2.0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

void drawCross(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	QGraphicsLineItem *line = new QGraphicsLineItem(-size/2.0, -size/2.0, size/2.0, size/2.0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	line = new QGraphicsLineItem(-size/2.0, size/2.0, size/2.0, -size/2.0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

void drawStar(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	drawPlus(size, parent, x, y, z, pen);
	drawCross(size, parent, x, y, z, pen);
}

void drawCircle(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	QGraphicsEllipseItem *circle = new QGraphicsEllipseItem(-size/2.0, -size/2.0, size, size, parent);
	circle->setPen(pen);
	circle->setBrush(pen.color());
	circle->setZValue(z);
	circle->setPos(x, y);
	circle->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

QPen changeAlpha(QPen pen, int alpha)
{
	QPen result (pen);
	result.setColor(QColor(pen.color().red(), pen.color().green(), pen.color().blue(), alpha));
	return result;
}

QBrush changeAlpha(QBrush brush, int alpha)
{
	QBrush result (brush);
	result.setColor(QColor(brush.color().red(), brush.color().green(), brush.color().blue(), alpha));
	return result;
}

QColor changeAlpha(QColor c, int alpha)
{
	return QColor(c.red(), c.green(), c.blue(), alpha);
}

void QOPlotWidget::drawPlot()
{
	//qDebug() << "DRAW";

	xLabel->setPlainText(plot.xlabel);
	yLabel->setPlainText(plot.ylabel);
	titleLabel->setPlainText(plot.title);

	bg->setBrush(QBrush(plot.backgroundColor));
	axesBgLeft->setBrush(QBrush(plot.backgroundColor));
	axesBgRight->setBrush(QBrush(plot.backgroundColor));
	axesBgTop->setBrush(QBrush(plot.backgroundColor));
	axesBgBottom->setBrush(QBrush(plot.backgroundColor));
	plotAreaBorder->setPen(QPen(plot.foregroundColor));
	xLabel->setDefaultTextColor(plot.foregroundColor);
	yLabel->setDefaultTextColor(plot.foregroundColor);
	titleLabel->setDefaultTextColor(plot.foregroundColor);

	scenePlot->removeItem(itemPlot);
	delete itemPlot;
	itemPlot = new QGraphicsDummyItem();
	scenePlot->addItem(itemPlot);
	itemPlot->setZValue(1);

	if (itemLegend->scene())
		scenePlot->removeItem(itemLegend);
	delete itemLegend;
	itemLegend = new QGraphicsDummyItem();
	scenePlot->addItem(itemLegend);
	itemLegend->setZValue(1000);

	const qreal legendLineLength = 30.0;
	const int legendLineSymbolsCount = 3;
	const qreal legendPadding = 10.0;
	const qreal legendSpacing = 5.0;
	const qreal legendTextIndent = 40.0; // = x_text - padding; must be larger than legendLineLength & friends
	qreal legendCurrentItemY = legendPadding;
	qreal legendCurrentWidth = 2 * legendPadding;
	bool haveLegend = false;

	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	foreach (QSharedPointer<QOPlotData> dataItem, plot.data) {
		if (dataItem->getDataType() == "line") {
			QSharedPointer<QOPlotCurveData> item = qSharedPointerCast<QOPlotCurveData>(dataItem);
			if (!item)
				continue;
			for (int i = 0; i < item->x.count() - 1; i++) {
				QGraphicsLineItem *line = new QGraphicsLineItem(item->x[i], item->y[i], item->x[i+1], item->y[i+1], itemPlot);
				line->setPen(item->pen);
				line->setZValue(1);
			}
			if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
				for (int i = 0; i < item->x.count(); i++) {
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					}
				}
			}
			// line legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw line
				QGraphicsLineItem *line = new QGraphicsLineItem(0, 0, legendLineLength, 0, itemLegendLabel);
				line->setPen(changeAlpha(item->pen, plot.legendAlpha));
				line->setPos(0, 0);
				line->setZValue(1);

				// draw symbols
				if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
					for (int i = 0; i < legendLineSymbolsCount; i++) {
						double x = item->symbolSize / 2.0 + i / (double)(legendLineSymbolsCount - 1) * (legendLineLength - item->symbolSize);
						if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
							drawPlus(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
							drawCross(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
							drawStar(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
							drawCircle(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						}
					}
				}

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		} else if (dataItem->getDataType() == "stem") {
			QSharedPointer<QOPlotStemData> item = qSharedPointerDynamicCast<QOPlotStemData>(dataItem);
			if (!item)
				continue;
			for (int i = 0; i < item->x.count(); i++) {
				if (item->y[i] == 0)
					continue;
				QGraphicsLineItem *line = new QGraphicsLineItem(item->x[i], 0.0, item->x[i], item->y[i], itemPlot);
				line->setPen(item->pen);
				line->setZValue(1);
			}
			if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
				for (int i = 0; i < item->x.count(); i++) {
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					}
				}
			}
			// stem legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw line
				QGraphicsLineItem *line = new QGraphicsLineItem(0, 0, legendLineLength, 0, itemLegendLabel);
				line->setPen(changeAlpha(item->pen.color(), plot.legendAlpha));
				line->setPos(0, 0);
				line->setZValue(1);

				// draw symbols
				if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
					double x = -item->symbolSize / 2.0 + legendLineLength;
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					}
				}

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		} else if (dataItem->getDataType() == "scatter") {
			QSharedPointer<QOPlotScatterData> item = qSharedPointerDynamicCast<QOPlotScatterData>(dataItem);
			if (!item)
				continue;
			if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
				for (int i = 0; i < item->x.count(); i++) {
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					}
				}
			}
			// scatter legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw symbols
				if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
					double x = -item->symbolSize / 2.0 + legendLineLength;
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					}
				}

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		} else if (dataItem->getDataType() == "boxplot") {
			QSharedPointer<QOPlotBoxPlotData> item = qSharedPointerDynamicCast<QOPlotBoxPlotData>(dataItem);
			if (!item)
				continue;
			for (int i = 0; i < item->items.count(); i++) {
				QOPlotBoxPlotItem &box = item->items[i];
				const qreal itemX = (i + 1) * item->itemWidth;
				const qreal boxLeft = itemX - item->boxWidth/2;
				const qreal boxRight = itemX + item->boxWidth/2;
				const qreal whiskerLeft = itemX - item->boxWidth/4;
				const qreal whiskerRight = itemX + item->boxWidth/4;
				const qreal cdfWidth = item->itemWidth/2.0;
				const qreal histWidth = cdfWidth;
				const qreal cdfAlpha = 0.2;
				const qreal histAlpha = cdfAlpha;

				QGraphicsRectItem *boxRect = new QGraphicsRectItem(boxLeft, box.quartileLow, item->boxWidth, box.quartileHigh - box.quartileLow, itemPlot);
				boxRect->setPen(box.pen);
				boxRect->setBrush(Qt::NoBrush);
				boxRect->setZValue(1);

				QGraphicsLineItem *median = new QGraphicsLineItem(boxLeft, box.quartileMed, boxRight, box.quartileMed, itemPlot);
				QPen medianPen = box.pen;
				medianPen.setColor(medianPen.color().lighter(150));
				median->setPen(medianPen);
				median->setZValue(1);

				if (!qIsNaN(box.whiskerLow)) {
					QGraphicsLineItem *vertLow = new QGraphicsLineItem(itemX, box.quartileLow, itemX, box.whiskerLow, itemPlot);
					vertLow->setPen(box.pen);
					vertLow->setZValue(1);

					QGraphicsLineItem *whiskerLow = new QGraphicsLineItem(whiskerLeft, box.whiskerLow, whiskerRight, box.whiskerLow, itemPlot);
					whiskerLow->setPen(box.pen);
					whiskerLow->setZValue(1);
				}

				if (!qIsNaN(box.whiskerHigh)) {
					QGraphicsLineItem *vertHigh = new QGraphicsLineItem(itemX, box.quartileHigh, itemX, box.whiskerHigh, itemPlot);
					vertHigh->setPen(box.pen);
					vertHigh->setZValue(1);

					QGraphicsLineItem *whiskerHigh = new QGraphicsLineItem(whiskerLeft, box.whiskerHigh, whiskerRight, box.whiskerHigh, itemPlot);
					whiskerHigh->setPen(box.pen);
					whiskerHigh->setZValue(1);
				}

				for (int iOutlier = 0; iOutlier < box.outliers.count(); iOutlier++) {
					qreal x = itemX;
					qreal y = box.outliers[iOutlier];
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, x, y, 1, box.pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, x, y, 1, box.pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, x, y, 1, box.pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, x, y, 1, box.pen);
					}
				}

				// CDF
				if (!box.cdf.isEmpty()) {
					for (int iPoint = 0; iPoint < box.cdf.count() - 1; iPoint++) {
						QGraphicsLineItem *line = new QGraphicsLineItem(itemX - box.cdf[iPoint].y() * cdfWidth, box.cdf[iPoint].x(), itemX - box.cdf[iPoint+1].y() * cdfWidth, box.cdf[iPoint+1].x(), itemPlot);
						QPen pen = box.pen;
						QColor c = pen.color();
						c.setAlphaF(cdfAlpha);
						pen.setColor(c);
						line->setPen(pen);
						line->setZValue(1);
					}
				}

				// histogram
				if (!box.histogram.isEmpty()) {
					for (int iPoint = 0; iPoint < box.histogram.count(); iPoint++) {
						QGraphicsRectItem *rect = new QGraphicsRectItem(QRectF(itemX, box.histogram[iPoint].x() - box.histogramBinWidth*1.0/2.0, box.histogram[iPoint].y() * histWidth, box.histogramBinWidth * 1.0), itemPlot);
						QColor c = box.pen.color();
						c.setAlphaF(histAlpha);
						rect->setPen(Qt::NoPen);
						rect->setBrush(QBrush(c));
						rect->setZValue(1);
					}
				}

				// boxplot legend
				if (item->legendVisible) {
					QGraphicsDummyItem *boxLegendLabel = new QGraphicsDummyItem(itemLegend);
					boxLegendLabel->setZValue(1);

					// draw line
					QGraphicsLineItem *line = new QGraphicsLineItem(0, 0, legendLineLength, 0, boxLegendLabel);
					line->setPen(changeAlpha(box.pen.color(), plot.legendAlpha));
					line->setPos(0, 0);
					line->setZValue(1);

					// draw label
					QString text;
					if (box.legendLabel.isEmpty()) {
						text = QString::number(i);
					} else {
						text = box.legendLabel;
					}
					QGraphicsTextItem *label = new QGraphicsTextItem(text, boxLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(box.pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());

					// set bbox manually
					boxLegendLabel->bbox = boxLegendLabel->childrenBoundingRect();

					// align vertically (everything is centered locally around y=0)
					boxLegendLabel->setPos(legendPadding, legendCurrentItemY + boxLegendLabel->boundingRect().height() / 2.0);

					legendCurrentItemY += boxLegendLabel->boundingRect().height() + legendSpacing;
					haveLegend = true;
				}
			}
		} else if (dataItem->getDataType() == "band") {
			QSharedPointer<QOPlotBandData> item = qSharedPointerCast<QOPlotBandData>(dataItem);
			if (!item)
				continue;
			// lines
			for (int i = 0; i < item->x.count() - 1; i++) {
				QGraphicsLineItem *line;
				line = new QGraphicsLineItem(item->x[i], item->yCentral[i], item->x[i+1], item->yCentral[i+1], itemPlot);
				line->setPen(item->centralPen);
				line->setZValue(1);

				line = new QGraphicsLineItem(item->x[i], item->yLower[i], item->x[i+1], item->yLower[i+1], itemPlot);
				line->setPen(item->lowerPen);
				line->setZValue(1);

				line = new QGraphicsLineItem(item->x[i], item->yUpper[i], item->x[i+1], item->yUpper[i+1], itemPlot);
				line->setPen(item->upperPen);
				line->setZValue(1);
			}
			// fill whiskers
			{
				QVector<QPointF> contour;
				for (int i = 0; i < item->x.count(); i++) {
					contour << QPointF(item->x[i], item->yWhiskerUpper[i]);
				}
				for (int i = item->x.count()-1; i >= 0 ; i--) {
					contour << QPointF(item->x[i], item->yWhiskerLower[i]);
				}
				QGraphicsPolygonItem *poly = new QGraphicsPolygonItem(contour, itemPlot);
				poly->setPen(Qt::NoPen);
				poly->setBrush(item->brushWhiskers);
				poly->setZValue(0.99);
			}
			// fill quartiles
			{
				QVector<QPointF> contour;
				for (int i = 0; i < item->x.count(); i++) {
					contour << QPointF(item->x[i], item->yUpper[i]);
				}
				for (int i = item->x.count()-1; i >= 0 ; i--) {
					contour << QPointF(item->x[i], item->yLower[i]);
				}
				QGraphicsPolygonItem *poly = new QGraphicsPolygonItem(contour, itemPlot);
				poly->setPen(Qt::NoPen);
				poly->setBrush(item->brush);
				poly->setZValue(1);
			}
			// line legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw line
				QGraphicsLineItem *line = new QGraphicsLineItem(0, 0, legendLineLength, 0, itemLegendLabel);
				line->setPen(changeAlpha(item->centralPen, plot.legendAlpha));
				line->setPos(0, 0);
				line->setZValue(1);

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->centralPen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		}
	}

	// legend background
	legendCurrentItemY -= legendSpacing;
	QGraphicsRectItem *legendBg = new QGraphicsRectItem(0, 0, legendCurrentWidth, legendCurrentItemY + legendPadding, itemLegend);
	legendBg->setPen(changeAlpha(plot.foregroundColor, plot.legendAlpha));
	legendBg->setBrush(changeAlpha(plot.legendBackground, plot.legendAlpha));
	legendBg->setZValue(0);
	itemLegend->bbox = itemLegend->childrenBoundingRect();
	if (!haveLegend || !plot.legendVisible) {
		scenePlot->removeItem(itemLegend);
	}

	// update axes and other stuff
	updateGeometry();
}

void QOPlotWidget::saveImage(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as image");
		fileDialog.setFilter("PNG (*.png)");
		fileDialog.setDefaultSuffix("png");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		QImage image(width(), height(), QImage::Format_RGB32);
		{
			QPainter painter(&image);
			painter.setRenderHint(QPainter::Antialiasing);
			painter.fillRect(image.rect(), Qt::white);
			scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		}
		image.save(fileName);
	}
}

void QOPlotWidget::saveSvgImage(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as scalable vectorial image");
		fileDialog.setFilter("SVG (*.svg)");
		fileDialog.setDefaultSuffix("svg");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		QSvgGenerator generator;
		generator.setFileName(fileName);
		generator.setSize(QSize(width(), height()));
		generator.setViewBox(QRect(0, 0, width(), height()));
		generator.setTitle(plot.title);
		generator.setDescription("Created by the LINE emulator.");
		QPainter painter;
		painter.begin(&generator);
		painter.setRenderHint(QPainter::Antialiasing);
		scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		painter.end();
	}
}

void QOPlotWidget::savePdf(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as PDF");
		fileDialog.setFilter("PDF (*.pdf)");
		fileDialog.setDefaultSuffix("pdf");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		// Setup printer
		QPrinter printer(QPrinter::HighResolution);
		printer.setOutputFormat(QPrinter::PdfFormat);
		printer.setOutputFileName(fileName);
		printer.setFullPage(true);
		printer.setPageSize(QPrinter::Custom);
		printer.setPaperSize(QSizeF(width(), height()), QPrinter::Point);
		printer.setPageMargins(0, 0, 0, 0, QPrinter::Point);

		QPainter painter;
		painter.begin(&printer);
		painter.setRenderHint(QPainter::Antialiasing);
		scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		painter.end();
	}
}

void QOPlotWidget::saveEps(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as EPS");
		fileDialog.setFilter("EPS (*.eps)");
		fileDialog.setDefaultSuffix("eps");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		// Setup printer
		QPrinter printer(QPrinter::HighResolution);
		printer.setOutputFormat(QPrinter::PostScriptFormat);
		printer.setOutputFileName(fileName);
		printer.setFullPage(true);
		printer.setPageSize(QPrinter::Custom);
        printer.setPaperSize(QSizeF(width(), height()), QPrinter::Point);
        printer.setPageMargins(0, 0, 0, 0, QPrinter::Point);

		QPainter painter;
		painter.begin(&printer);
        painter.setRenderHint(QPainter::HighQualityAntialiasing);
		scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		painter.end();
	}
}

void QOPlotWidget::saveJson(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as JSON");
		fileDialog.setFilter("JSON (*.json)");
		fileDialog.setDefaultSuffix("json");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}
	if (fileName.isEmpty()) {
		return;
	}
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		return;
	QTextStream out(&file);
	out << "{" << endl;
	out << QString("\"title\": \"%1\",").arg(escapeLatexJson(plot.title)) << endl;
	out << QString("\"xLabel\": \"%1\",").arg(escapeLatexJson(plot.xlabel)) << endl;
	out << QString("\"yLabel\": \"%1\",").arg(escapeLatexJson(plot.ylabel)) << endl;
    out << QString("\"fileName\": \"%1\",").arg(QString(fileName).replace(".json", ".eps")) << endl;
    out << QString("\"fontScale\": 2.5,") << endl;
	out << QString("\"grid\": \"%1\",")
		   .arg((plot.xGridVisible || plot.yGridVisible) ? "major" : "") << endl;
	if (plot.fixedAxes) {
		out << QString("\"xmin\": %1,").arg(plot.fixedXmin) << endl;
		out << QString("\"xmax\": %1,").arg(plot.fixedXmax) << endl;
		out << QString("\"ymin\": %1,").arg(plot.fixedYmin) << endl;
		out << QString("\"ymax\": %1,").arg(plot.fixedYmax) << endl;
	} else {
		qreal deviceW, deviceH, worldW, worldH;
		getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
		out << QString("\"xmin\": %1,").arg(world_x_off) << endl;
		out << QString("\"xmax\": %1,").arg(world_x_off + deviceW / zoomX) << endl;
		out << QString("\"ymin\": %1,").arg(world_y_off) << endl;
		out << QString("\"ymax\": %1,").arg(world_y_off + deviceH / zoomY) << endl;
	}
	out << QString("\"_xFormatStr\": \"%2.1f\",") << endl;
	out << QString("\"_yFormatStr\": \"%2.1f\",") << endl;
	if (!plot.xCustomTicks.isEmpty()) {
		QList<qreal> ticks;
		QList<QString> tickLabels;

		foreach (qreal x, plot.xCustomTicks.uniqueKeys()) {
			ticks << x;
			tickLabels << plot.xCustomTicks[x];
		}

		out << QString("\"xTicks\": [");
		bool first = true;
		foreach (qreal x, ticks) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << x;
		}
		out << QString("],") << endl;
		out << QString("\"xTickLabels\": [");
		first = true;
		foreach (QString s, tickLabels) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << '"' << s << '"';
		}
		out << QString("],") << endl;
	} else {
		out << QString("\"_xTicks\": [],") << endl;
		out << QString("\"_xTickLabels\": [],") << endl;
	}

	if (!plot.yCustomTicks.isEmpty()) {
		QList<qreal> ticks;
		QList<QString> tickLabels;

		foreach (qreal y, plot.yCustomTicks.uniqueKeys()) {
			ticks << y;
			tickLabels << plot.yCustomTicks[y];
		}

		out << QString("\"yTicks\": [");
		bool first = true;
		foreach (qreal y, ticks) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << y;
		}
		out << QString("],") << endl;
		out << QString("\"yTickLabels\": [");
		first = true;
		foreach (QString s, tickLabels) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << '"' << s << '"';
		}
		out << QString("],") << endl;
	} else {
		out << QString("\"_yTicks\": [],") << endl;
		out << QString("\"_yTickLabels\": [],") << endl;
	}

	out << QString("\"minorXTicks\" : 0,") << endl;
	out << QString("\"minorYTicks\" : 0,") << endl;
	out << QString("\"_majorXTicks\" : -1,") << endl;
	out << QString("\"_majorYTicks\" : -1,") << endl;
	out << QString("\"%1noLegend\" : 0,").arg(plot.legendVisible ? "_" : "") << endl;
	out << QString("\"_legendTitle\" : \"Legend\",") << endl;

    //out << QString("\"w\" : %1,").arg(width() / qApp->desktop()->logicalDpiX()) << endl;
    //out << QString("\"h\" : %1,").arg(height() / qApp->desktop()->logicalDpiY()) << endl;
    out << QString("\"w\" : 30,") << endl;
    out << QString("\"h\" : 8,") << endl;

	out << QString("\"data\": [") << endl;

	for (int i = 0; i < plot.data.count(); i++) {
		out << plot.data[i]->toJson();
		if (i < plot.data.count() - 1) {
			out << ",";
		}
		out << endl;
	}

	out << QString("]") << endl;
	out << "}" << endl;
}

void QOPlotWidget::resizeEvent(QResizeEvent*e)
{
	if (width() == 0 || height() == 0)
		return;
	if (e->size() == e->oldSize())
		return;
	if (plot.autoAdjusted) {
		autoAdjustAxes();
	} else if (plot.autoAdjustedFixedAspect) {
		autoAdjustAxes(plot.autoAspectRatio);
	} else if (plot.fixedAxes) {
		fixAxes(plot.fixedXmin, plot.fixedXmax, plot.fixedYmin, plot.fixedYmax);
	} else {
		updateGeometry();
	}
}

void QOPlotWidget::setVisible(bool visible)
{
	QWidget::setVisible(visible);
	if (visible) {
		QResizeEvent e(size(), QSize(0,0));
		resizeEvent(&e);
	}
}

void QOPlotWidget::updateGeometry()
{
	if (!isVisible())
		return;
	qreal totalMarginBottom;
	totalMarginBottom = marginBottom;

	if (plot.legendPosition == QOPlot::BottomOutside) {
		setMinSize(plotMinWidth, plotMinHeight + itemLegend->childrenBoundingRect().height() + extraMargin);
		totalMarginBottom = marginBottom + itemLegend->childrenBoundingRect().height() + extraMargin;
	} else {
		setMinSize(plotMinWidth, plotMinHeight);
	}

	bg->setRect(0, 0, width(), height());
	axesBgLeft->setRect(0, 0, marginLeft, height());
	axesBgRight->setRect(width() - marginRight, 0, marginRight, height());
	axesBgTop->setRect(0, 0, width(), marginTop);
	axesBgBottom->setRect(0, height() - totalMarginBottom, width(), totalMarginBottom);

	viewPlot->setGeometry(0, 0, width(), height());
	viewPlot->setSceneRect(0, 0, width(), height());

	qreal plotWidth, plotHeight, world_w, world_h;
	getPlotAreaGeometry(plotWidth, plotHeight, world_w, world_h);

	plotAreaBorder->setRect(marginLeft, marginTop, plotWidth, plotHeight);

	xLabel->setPos(width() / 2.0 - xLabel->boundingRect().width() / 2.0, marginTop + plotHeight + marginBottom * 2.0 / 3.0 - xLabel->boundingRect().height() / 2.0);
	yLabel->setPos(marginLeft / 3.0 - yLabel->boundingRect().height() / 2.0, marginTop + plotHeight / 2.0 + yLabel->boundingRect().width() / 2.0);
	yLabel->setRotation(-90);
	titleLabel->setPos(width() / 2.0 - titleLabel->boundingRect().width() / 2.0, marginTop / 2.0 - titleLabel->boundingRect().height() / 2.0);

	itemPlot->resetTransform();
	itemPlot->translate(marginLeft + plotWidth * (-world_x_off/world_w), marginBottom + plotHeight * (1 + world_y_off/world_h));
	itemPlot->scale(plotWidth/world_w, -plotHeight/world_h);

	// ticks
	foreach (QGraphicsItem* item, tickItems) {
		delete item;
	}
	tickItems.clear();

	qreal x_tick_start, x_tick_size;
	const int x_tick_count = width() > 1400 ? 20 : 10;
	const qreal x_tick_length = 5; // line length
	nice_loose_label(world_x_off, world_x_off + world_w, x_tick_count, x_tick_start, x_tick_size);
	// X tick marks & grid
    QColor gridColor = Qt::gray;
	for (int i = 0; i <= x_tick_count; i++) {
		qreal x_tick_world = x_tick_start + i * x_tick_size;
		if (world_x_off <= x_tick_world && x_tick_world <= world_x_off + world_w) {
			qreal x_tick_device = marginLeft + (x_tick_world - world_x_off)/world_w*plotWidth;
			QGraphicsTextItem *tick = scenePlot->addText(plot.xCustomTicks.contains(x_tick_world) ?
															 plot.xCustomTicks[x_tick_world] :
															 plot.xSISuffix ? withMultiplierSuffix(x_tick_world) :
																			  withCommasStr(x_tick_world));
			tick->setPos(x_tick_device - tick->boundingRect().width() / 2.0, marginTop + plotHeight);
			tick->setDefaultTextColor(plot.foregroundColor);
			tick->setZValue(200);
			tickItems << tick;
			QGraphicsLineItem *line = scenePlot->addLine(x_tick_device, marginTop + plotHeight, x_tick_device, marginTop + plotHeight - x_tick_length, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			line = scenePlot->addLine(x_tick_device, marginTop, x_tick_device, marginTop + x_tick_length, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			if (plot.xGridVisible) {
                line = scenePlot->addLine(x_tick_device, marginTop + plotHeight, x_tick_device, marginTop + plotHeight - plotHeight, QPen(gridColor, 1.0, Qt::DotLine));
				line->setZValue(0.4);
				tickItems << line;
			}
		}
	}
	// Y axis
	if (world_x_off <= 0 && 0 <= world_x_off + world_w) {
		qreal x_tick_device = marginLeft + (0.0 - world_x_off)/world_w*plotWidth;
		QGraphicsLineItem *line = scenePlot->addLine(x_tick_device, marginTop + plotHeight, x_tick_device, marginTop + plotHeight - plotHeight, QPen(plot.foregroundColor, 1.0, Qt::SolidLine));
		line->setZValue(0.4);
		tickItems << line;
	}

	qreal y_tick_start, y_tick_size;
	const int y_tick_count = 10;
	const qreal y_tick_length = 5; // line length
	nice_loose_label(world_y_off, world_y_off + world_h, y_tick_count, y_tick_start, y_tick_size);
	double y_tick_max_width = -1;
	// Y tick marks & grid
	for (int i = 0; i <= y_tick_count; i++) {
		qreal y_tick_world = y_tick_start + i * y_tick_size;
		if (world_y_off <= y_tick_world && y_tick_world <= world_y_off + world_h) {
			qreal y_tick_device = marginTop + (1 - (y_tick_world - world_y_off)/world_h)*plotHeight;
			QGraphicsTextItem *tick = scenePlot->addText(plot.yCustomTicks.contains(y_tick_world) ?
															 plot.yCustomTicks[y_tick_world] :
															 plot.ySISuffix ? withMultiplierSuffix(y_tick_world) :
																			  withCommasStr(y_tick_world));
			tick->setDefaultTextColor(plot.foregroundColor);
			tick->setPos(marginLeft - tick->boundingRect().width(), y_tick_device - tick->boundingRect().height() / 2.0);
			tick->setZValue(200);
			tickItems << tick;
			y_tick_max_width = qMax(y_tick_max_width, tick->boundingRect().width());
			QGraphicsLineItem *line = scenePlot->addLine(marginLeft, y_tick_device, marginLeft + y_tick_length, y_tick_device, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			line = scenePlot->addLine(marginLeft + plotWidth, y_tick_device, marginLeft + plotWidth - y_tick_length, y_tick_device, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			if (plot.yGridVisible) {
                line = scenePlot->addLine(marginLeft, y_tick_device, marginLeft + plotWidth, y_tick_device, QPen(gridColor, 1.0, Qt::DotLine));
				line->setZValue(0.4);
				tickItems << line;
			}
		}
	}
	// X axis
	if (world_y_off <= 0 && 0 <= world_y_off + world_h) {
		qreal y_tick_device = marginTop + (1 - (0.0 - world_y_off)/world_h)*plotHeight;
		QGraphicsLineItem *line = scenePlot->addLine(marginLeft, y_tick_device, marginLeft + plotWidth, y_tick_device, QPen(plot.foregroundColor, 1.0, Qt::SolidLine));
		line->setZValue(0.4);
		tickItems << line;
	}

	// track items
	foreach (QGraphicsItem* item, trackItems) {
		delete item;
	}
	trackItems.clear();
	if (tracked) {
		// vertical line and label
		qreal x_track_device = marginLeft + (track_x - world_x_off)/world_w*plotWidth;
		QGraphicsTextItem *track_xlabel = scenePlot->addText(QString::number(track_x));
		track_xlabel->setDefaultTextColor(plot.foregroundColor);
		track_xlabel->setPos(x_track_device - track_xlabel->boundingRect().width() / 2.0, marginTop + plotHeight);
		track_xlabel->setZValue(201);
		trackItems << track_xlabel;
		QGraphicsRectItem *xlabelBg = scenePlot->addRect(track_xlabel->boundingRect().translated(track_xlabel->pos()).translated(0, 1), QPen(Qt::NoPen), plot.backgroundColor);
		xlabelBg->setZValue(200.5);
		trackItems << xlabelBg;
		QGraphicsLineItem *vline = scenePlot->addLine(x_track_device, marginTop + plotHeight, x_track_device, marginTop + plotHeight - plotHeight, QPen(plot.foregroundColor.lighter(), 1.0, Qt::DashLine));
		vline->setZValue(0.45);
		trackItems << vline;
		// horizontal line and label
		qreal y_track_device = marginTop + (1 - (track_y - world_y_off)/world_h)*plotHeight;
		QGraphicsTextItem *track_ylabel = scenePlot->addText(QString::number(track_y));
		track_ylabel->setDefaultTextColor(plot.foregroundColor);
		track_ylabel->setPos(marginLeft - track_ylabel->boundingRect().width(), y_track_device - track_ylabel->boundingRect().height() / 2.0);
		track_ylabel->setZValue(201);
		trackItems << track_ylabel;
		QGraphicsRectItem *ylabelBg = scenePlot->addRect(track_ylabel->boundingRect().translated(track_ylabel->pos()).translated(-1, 0), QPen(Qt::NoPen), plot.backgroundColor);
		ylabelBg->setZValue(200.5);
		trackItems << ylabelBg;
		y_tick_max_width = qMax(y_tick_max_width, track_ylabel->boundingRect().width());
		QGraphicsLineItem *hline = scenePlot->addLine(marginLeft, y_track_device, marginLeft + plotWidth, y_track_device, QPen(plot.foregroundColor.lighter(), 1.0, Qt::DashLine));
		hline->setZValue(0.45);
		trackItems << hline;
	}

	// adjust marginLeft if it is too small
	if (y_tick_max_width > marginLeft * 2.0 / 3.0 - yLabel->boundingRect().height() / 2.0) {
		marginLeft = (y_tick_max_width + yLabel->boundingRect().height() / 2.0) * 3.0 / 2.0 * 1.1;
		updateGeometry();
	}

	// legend
	if (plot.legendPosition == QOPlot::TopLeft) {
		itemLegend->setPos(marginLeft, marginTop);
	} else if (plot.legendPosition == QOPlot::TopRight) {
		itemLegend->setPos(marginLeft + plotWidth - itemLegend->childrenBoundingRect().width(), marginTop);
	} else if (plot.legendPosition == QOPlot::BottomLeft) {
		itemLegend->setPos(marginLeft, marginTop + plotHeight - itemLegend->childrenBoundingRect().height());
	} else if (plot.legendPosition == QOPlot::BottomRight) {
		itemLegend->setPos(marginLeft + plotWidth - itemLegend->childrenBoundingRect().width(), marginTop + plotHeight - itemLegend->childrenBoundingRect().height());
	} else if (plot.legendPosition == QOPlot::BottomOutside) {
		itemLegend->setPos(marginLeft, marginTop + plotHeight + marginBottom);
	}

	QWidget::updateGeometry();
}

void QOPlotWidget::getPlotAreaGeometry(qreal &deviceW, qreal &deviceH, qreal &worldW, qreal &worldH)
{
	deviceW = qMax(width() - marginLeft - marginRight, 1.0);
	deviceH = qMax(height() - marginTop - marginBottom, 1.0);
	if (plot.legendPosition == QOPlot::BottomOutside)
		deviceH = qMax(deviceH - itemLegend->childrenBoundingRect().height() - extraMargin, 1.0);

	worldW = deviceW / zoomX;
	worldH = deviceH / zoomY;
}

void QOPlotWidget::mouseMoveEvent(QMouseEvent* event)
{
	// world coords
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		//		qDebug() << "move:" << "screen" << event->x() << event->y() << "world" << world_x << world_y;
		bool mustUpdate = false;
		if (plot.drag_x_enabled && event->buttons() & Qt::LeftButton && event->modifiers() == Qt::NoModifier && !isnan(world_press_x)) {
			qreal delta_x = world_x - world_press_x;
			world_x_off -= delta_x;
			mustUpdate = true;
		}

		if (plot.drag_y_enabled && event->buttons() & Qt::LeftButton && event->modifiers() == Qt::NoModifier && !isnan(world_press_y)) {
			qreal delta_y = world_y - world_press_y;
			world_y_off -= delta_y;
			mustUpdate = true;
		}

		if (mustUpdate) {
			plot.autoAdjusted = false;
			plot.autoAdjustedFixedAspect = false;
			plot.fixedAxes = false;
			updateGeometry();
			emitViewportChanged();
		} else if (event->buttons() & Qt::RightButton) {
			track_x = world_x;
			track_y = world_y;
			tracked = true;
			updateGeometry();
		}
	} else {
		if (event->buttons() & Qt::RightButton) {
			if (tracked) {
				tracked = false;
				updateGeometry();
			}
		}
	}
}

void QOPlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
	// world coords
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		//		qDebug() << "release:" << "screen" << event->x() << event->y() << "world" << world_x << world_y;
	}
}

void QOPlotWidget::mousePressEvent(QMouseEvent* event)
{
	// world coords
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		//		qDebug() << "press:" << "screen" << event->x() << event->y() << "world" << world_x << world_y;
		if (event->modifiers() & Qt::ControlModifier) {
			QPointF scenePos = viewPlot->mapToScene(event->x(), event->y());
			QPointF plotPos = itemPlot->mapFromScene(scenePos);
			qreal wdx, wdy;
			widgetDeltaToWorldDelta(50, 50, wdx, wdy);
			QOPlotHit hits = plot.getHits(plotPos.x(), plotPos.y(), zoomY/zoomX, wdx);
			if (!hits.hits.isEmpty()) {
				emit mouseHit(hits);
			}
		} else if (event->modifiers() == Qt::NoModifier) {
			if (event->buttons() & Qt::LeftButton) {
				world_press_x = world_x;
				world_press_y = world_y;
			} else if (event->buttons() & Qt::RightButton) {
				track_x = world_x;
				track_y = world_y;
				tracked = true;
				updateGeometry();
			}
		}
	} else {
		if (event->buttons() & Qt::RightButton) {
			if (tracked) {
				tracked = false;
				updateGeometry();
			}
		}
	}

	if (event->buttons() & Qt::MidButton) {
		autoAdjustAxes();
	}
}

void QOPlotWidget::wheelEvent(QWheelEvent *event)
{
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		float steps = event->delta()/8.0/15.0;

		//		qDebug() << "wheel:" << "screen" << event->x() << event->y() << "delta" << event->delta() << "world" << world_x << world_y;

		if (plot.zoom_x_enabled) {
			qreal zoomX_old = zoomX;
			zoomX *= pow(1.25, steps);
			world_x_off = (world_x_off - world_x * (1.0 - zoomX / zoomX_old)) * zoomX_old / zoomX;
		}

		if (plot.zoom_y_enabled) {
			qreal zoomY_old = zoomY;
			zoomY *= pow(1.25, steps);
			world_y_off = (world_y_off - world_y * (1.0 - zoomY / zoomY_old)) * zoomY_old / zoomY;
		}

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		drawPlot();
		emitViewportChanged();
		event->accept();
	}
}

void QOPlotWidget::keyPressEvent(QKeyEvent *event)
{
	event->ignore();
	if (event->key() == Qt::Key_Left && plot.drag_x_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dx = -1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dx = -50;
		} else {
			widget_dx = -10;
		}
		widget_dy = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_x_off += world_dx;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		emitViewportChanged();
		event->accept();
	} else if (event->key() == Qt::Key_Right && plot.drag_x_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dx = 1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dx = 50;
		} else {
			widget_dx = 10;
		}
		widget_dy = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_x_off += world_dx;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		emitViewportChanged();
		event->accept();
	} else if (event->key() == Qt::Key_Up && plot.drag_y_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dy = -1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dy = -50;
		} else {
			widget_dy = -10;
		}
		widget_dx = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_y_off += world_dy;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		emitViewportChanged();
		event->accept();
	} else if (event->key() == Qt::Key_Down && plot.drag_y_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dy = 1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dy = 50;
		} else {
			widget_dy = 10;
		}
		widget_dx = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_y_off += world_dy;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		emitViewportChanged();
		event->accept();
	} else if (event->key() == Qt::Key_Minus) {
		float steps;
		if (event->modifiers() & Qt::ControlModifier) {
			steps = -0.1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			steps = -5;
		} else {
			steps = -1;
		}

		qreal deviceW, deviceH, worldW, worldH;
		getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
		qreal world_x = world_x_off + worldW * 0.5;
		qreal world_y = world_y_off + worldH * 0.5;

		if (plot.zoom_x_enabled) {
			qreal zoomX_old = zoomX;
			zoomX *= pow(1.25, steps);
			world_x_off = (world_x_off - world_x * (1.0 - zoomX / zoomX_old)) * zoomX_old / zoomX;
		}

		if (plot.zoom_y_enabled) {
			qreal zoomY_old = zoomY;
			zoomY *= pow(1.25, steps);
			world_y_off = (world_y_off - world_y * (1.0 - zoomY / zoomY_old)) * zoomY_old / zoomY;
		}

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		emitViewportChanged();
		event->accept();
	} else if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
		float steps;
		if (event->modifiers() & Qt::ControlModifier) {
			steps = 0.1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			steps = 5;
		} else {
			steps = 1;
		}

		qreal deviceW, deviceH, worldW, worldH;
		getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
		qreal world_x = world_x_off + worldW * 0.5;
		qreal world_y = world_y_off + worldH * 0.5;

		if (plot.zoom_x_enabled) {
			qreal zoomX_old = zoomX;
			zoomX *= pow(1.25, steps);
			world_x_off = (world_x_off - world_x * (1.0 - zoomX / zoomX_old)) * zoomX_old / zoomX;
		}

		if (plot.zoom_y_enabled) {
			qreal zoomY_old = zoomY;
			zoomY *= pow(1.25, steps);
			world_y_off = (world_y_off - world_y * (1.0 - zoomY / zoomY_old)) * zoomY_old / zoomY;
		}

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		emitViewportChanged();
		event->accept();
	} else if (event->key() == Qt::Key_A) {
		autoAdjustAxes();
		emitViewportChanged();
	} else if (event->key() == Qt::Key_S) {
		saveImage(plotFileName.isEmpty() ? "" : plotFileName + ".png");
	} else if (event->key() == Qt::Key_V) {
		saveSvgImage(plotFileName.isEmpty() ? "" : plotFileName + ".svg");
	} else if (event->key() == Qt::Key_P) {
		savePdf(plotFileName.isEmpty() ? "" : plotFileName + ".pdf");
	} else if (event->key() == Qt::Key_E) {
		saveEps(plotFileName.isEmpty() ? "" : plotFileName + ".eps");
	} else if (event->key() == Qt::Key_J) {
		saveJson(plotFileName.isEmpty() ? "" : plotFileName + ".json");
	} else if (event->key() == Qt::Key_Q) {
		if (viewPlot->renderHints() & QPainter::Antialiasing) {
			viewPlot->setRenderHint(QPainter::Antialiasing, false);
			viewPlot->setRenderHint(QPainter::HighQualityAntialiasing, false);
		} else {
			viewPlot->setRenderHint(QPainter::Antialiasing, true);
			viewPlot->setRenderHint(QPainter::HighQualityAntialiasing, true);
		}
	} else if (event->key() == Qt::Key_I) {
		invertLigthness(plot.foregroundColor);
		invertLigthness(plot.backgroundColor);
		invertLigthness(plot.legendBackground);
		drawPlot();
	}
}

void QOPlotWidget::focusInEvent(QFocusEvent *event)
{
	event->accept();
}

void QOPlotWidget::focusOutEvent(QFocusEvent *event)
{
	event->accept();
}

void QOPlotWidget::enterEvent(QEvent *event)
{
	event->accept();
	setFocus(Qt::MouseFocusReason);
}

void QOPlotWidget::autoAdjustAxes()
{
	// compute bounding box
	qreal xmin, xmax, ymin, ymax;
	dataBoundingBox(xmin, xmax, ymin, ymax);

	if (plot.includeOriginX) {
		if (xmin > 0)
			xmin = 0;
		if (xmax < 0)
			xmax = 0;
	}
	if (plot.includeOriginY) {
		if (ymin > 0)
			ymin = 0;
		if (ymax < 0)
			ymax = 0;
	}
	xmax += (xmax - xmin) * 0.1;
	ymax += (ymax - ymin) * 0.1;

	// compute zoom/scroll
	recalcViewport(xmin, xmax, ymin, ymax);

	plot.autoAdjusted = true;
	plot.autoAdjustedFixedAspect = false;
	plot.fixedAxes = false;

	updateGeometry();
}

void QOPlotWidget::autoAdjustAxes(qreal aspectRatio)
{
	plot.autoAspectRatio = aspectRatio;

	// compute bounding box
	qreal xmin, xmax, ymin, ymax;
	dataBoundingBox(xmin, xmax, ymin, ymax);

	if (plot.includeOriginX) {
		if (xmin > 0)
			xmin = 0;
		if (xmax < 0)
			xmax = 0;
	}
	if (plot.includeOriginY) {
		if (ymin > 0)
			ymin = 0;
		if (ymax < 0)
			ymax = 0;
	}
	xmax += (xmax - xmin) * 0.1;
	ymax += (ymax - ymin) * 0.1;

	// compute zoom/scroll
	recalcViewport(xmin, xmax, ymin, ymax, aspectRatio);

	plot.autoAdjusted = false;
	plot.autoAdjustedFixedAspect = true;
	plot.fixedAxes = false;

	updateGeometry();
}

void QOPlotWidget::fixAxes(qreal xmin, qreal xmax, qreal ymin, qreal ymax)
{
	plot.fixedXmin = xmin;
	plot.fixedXmax = xmax;
	plot.fixedYmin = ymin;
	plot.fixedYmax = ymax;

	recalcViewport(xmin, xmax, ymin, ymax);

	plot.autoAdjusted = false;
	plot.autoAdjustedFixedAspect = false;
	plot.fixedAxes = true;

	updateGeometry();
}

void QOPlotWidget::recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax)
{
	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	world_x_off = xmin;
	world_y_off = ymin;
	zoomX = (xmax - xmin == 0) ? 1.0 : deviceW / (xmax - xmin);
	zoomY = (ymax - ymin == 0) ? 1.0 : deviceH / (ymax - ymin);
}

void QOPlotWidget::recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax, qreal aspectRatio)
{
	// Solve:
	// zoomX / zoomY = 1/aspectRatio
	// zoomX <= zoomXMin
	// zoomY <= zoomYMin
	// 1. Compute zoomXMin, zoomYMin
	recalcViewport(xmin, xmax, ymin, ymax);
	// 2. Enforce ratio
	if (aspectRatio > 1) {
		zoomX = zoomY / aspectRatio;
	} else {
		zoomY = zoomX * aspectRatio;
	}
}

void QOPlotWidget::dataBoundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax)
{
	if (!plot.data.isEmpty()) {
		plot.data[0]->boundingBox(xmin, xmax, ymin, ymax);
	} else {
		xmin = 0; xmax = 1;
		ymin = 0; ymax = 1;
	}
	foreach (QSharedPointer<QOPlotData> item, plot.data) {
		qreal dxmin, dxmax, dymin, dymax;
		item->boundingBox(dxmin, dxmax, dymin, dymax);
		if (dxmin < xmin)
			xmin = dxmin;
		if (dxmax > xmax)
			xmax = dxmax;
		if (dymin < ymin)
			ymin = dymin;
		if (dymax > ymax)
			ymax = dymax;
	}
}

bool QOPlotWidget::widgetCoordsToWorldCoords(qreal widget_x, qreal widget_y, qreal &world_x, qreal &world_y)
{
	widget_x -= marginLeft;
	widget_y -= marginTop;

	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	if (widget_x < 0 || widget_x >= deviceW)
		return false;
	if (widget_y < 0 || widget_y >= deviceH)
		return false;

	widget_y = deviceH - widget_y;
	world_x = world_x_off + widget_x / deviceW * worldW;
	world_y = world_y_off + widget_y / deviceH * worldH;

	// success
	return true;
}

void QOPlotWidget::widgetDeltaToWorldDelta(qreal widget_dx, qreal widget_dy, qreal &world_dx, qreal &world_dy)
{
	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	widget_dy = -widget_dy;
	world_dx = widget_dx / deviceW * worldW;
	world_dy = widget_dy / deviceH * worldH;
}

void QOPlotWidget::emitViewportChanged()
{
	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	emit viewportChanged(world_x_off, world_y_off, world_x_off + worldW, world_y_off + worldH, this);
}



void QOPlotGroup::addPlot(QOPlotWidget *plot)
{
	plots.insert(plot);
	QObject::connect(plot, SIGNAL(viewportChanged(qreal,qreal,qreal,qreal,QOPlotWidget*)), this, SLOT(viewportChanged(qreal,qreal,qreal,qreal,QOPlotWidget*)));
}

void QOPlotGroup::clear()
{
	plots.clear();
}

void QOPlotGroup::viewportChanged(qreal xmin, qreal ymin, qreal xmax, qreal ymax, QOPlotWidget *sender)
{
	foreach (QOPlotWidget *plot, plots) {
		if (plot != sender) {
			if (plot->plot.zoom_x_enabled && plot->plot.zoom_y_enabled) {
				plot->recalcViewport(xmin, xmax, ymin, ymax);
			} else if (plot->plot.zoom_x_enabled) {
				qreal deviceW, deviceH, worldW, worldH;
				plot->getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
				plot->recalcViewport(xmin, xmax, plot->world_y_off, plot->world_y_off + worldH);
			} else if (plot->plot.zoom_y_enabled) {
				qreal deviceW, deviceH, worldW, worldH;
				plot->getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
				plot->recalcViewport(plot->world_x_off, plot->world_x_off + worldW, ymin, ymax);
			}
			plot->updateGeometry();
		}
	}
}
