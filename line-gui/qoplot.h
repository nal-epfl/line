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

#ifndef QOPLOT_H
#define QOPLOT_H

#include <QtCore>
#include <QtGui>

class QOPlotData;
class QOPlotCurveData;
class QOPlotBoxPlotItem;

class QOPlotHit;
class QOPlotDataHit;

// Holds the plot configuration (no GUI elements)
class QOPlot
{
public:
	enum LegendPosition {
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,
		BottomOutside
	};

	explicit QOPlot();
	~QOPlot();

	// the plot title
	QString title;
	// axis labels
	QString xlabel;
	bool xSISuffix;
	QMap<qreal, QString> xCustomTicks;
	QString ylabel;
	bool ySISuffix;
	QMap<qreal, QString> yCustomTicks;

	// the data to plot
	QList<QSharedPointer<QOPlotData> > data;

	// Adds d to the plot and takes ownership (so do not delete d somewhere else)
	void addData(QOPlotData *d) {
		data << QSharedPointer<QOPlotData>(d);
	}

	void differentColors();

	// bg color: used for the background...
	QColor backgroundColor;
	// fg color: used for axes, text
	QColor foregroundColor;

	// legend config
	bool legendVisible; // if true, show the legend
	LegendPosition legendPosition;
	QColor legendBackground; // RGB, A does not matter
	int legendAlpha; // opacity

	// show axis grid
	bool xGridVisible;
	bool yGridVisible;

	// There are 3 different ways of setting a predefined viewport.
	// In any case, the viewport is changed on user scroll/zoom.
	// 1. Fixed viewport: specify axes limits (xmin xmax ymin ymax)
	bool fixedAxes;
	qreal fixedXmin, fixedXmax, fixedYmin, fixedYmax;
	// 2. Auto adjusted: the plot is automatically resized to show all data
	// If includeOriginX is set, the viewport is changed to include X=0
	// If includeOriginY is set, the viewport is changed to include Y=0
	bool autoAdjusted;
	bool includeOriginX;
	bool includeOriginY;
	// 3. Auto adjusted with custom aspect ratio: the plot is automatically resized to show all data,
	// but a specific aspect ratio is preserved
	// If includeOriginX is set, the viewport is changed to include X=0
	// If includeOriginY is set, the viewport is changed to include Y=0
	bool autoAdjustedFixedAspect;
	qreal autoAspectRatio;

	// set this to enable/disable mouse actions
	bool drag_x_enabled;
	bool drag_y_enabled;
	bool zoom_x_enabled;
	bool zoom_y_enabled;

	QOPlotHit getHits(qreal x, qreal y, qreal aspectRatio, qreal distanceThreshold);

	static QVector<QPointF> makePoints(QList<qreal> v);

	static QVector<QPointF> makeCDF(QList<qreal> v);
	static QVector<QPointF> makeCDF(QList<qreal> v, qreal min);
	static QVector<QPointF> makeCDF(QList<qreal> v, qreal min, qreal max);

	static QVector<QPointF> makeCDFSampled(QList<qreal> v, int binCount = -1);
	static QVector<QPointF> makeCDFSampled(QList<qreal> v, qreal min, int binCount = -1);
	static QVector<QPointF> makeCDFSampled(QList<qreal> v, qreal min, qreal max, int binCount = -1);

	static QVector<QPointF> makeHistogram(QList<qreal> v, qreal &binWidth, int binCount = -1);
	static QOPlotBoxPlotItem makeBoxWhiskers(QList<qreal> v, QString title, QColor color, bool withHist, bool withCDF);

	static qreal median(QList<qreal> v);
	static qreal medianSorted(QList<qreal> v);
	static void quartiles(QList<qreal> v, qreal &q1, qreal &q2, qreal &q3);

	static QOPlotCurveData *makeLine(qreal x1, qreal y1, qreal x2, qreal y2, QColor c = Qt::black);
};

// Holds a data set to be plotted (e.g. a vector of points). Subclasses should specialize this.
class QOPlotData
{
public:
	explicit QOPlotData();
	virtual ~QOPlotData() {}
	virtual QString getDataType() { return dataType; }
	bool legendVisible;  // show a legend item for this line
	QString legendLabel; // text label for legend
	QColor legendLabelColor; // text color

	// allows the user to set an integer attribute to each data point
	// if empty, a hit will return the point index
	// if not empty, a hit will return pointUserData[point index[
	QVector<int> pointUserData;
	virtual QOPlotDataHit getHits(qreal x, qreal y, qreal aspectRatio);
	virtual QString toJson() { return "{}"; }

	virtual void boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax) { xmin = 0; xmax = 1; ymin = 0; ymax = 1;}
	virtual QRectF boundingBox() {
		qreal xmin, xmax, ymin, ymax;
		boundingBox(xmin, xmax, ymin, ymax);
		return QRectF(xmin, xmax-xmin, ymin, ymax-ymin).normalized();
	}

protected:
	QString dataType; // can be: "line", "stem"
};

// Holds a data set to be plotted as a zig-zag line.
class QOPlotCurveData : public QOPlotData
{
public:
	explicit QOPlotCurveData();
	QVector<qreal> x; // x coords of the data points
	QVector<qreal> y; // y coords of the data points, x and y MUST have the same length
	QPen pen; // color, line style, width etc. Set cosmetic to true!
	QString pointSymbol; // use "+" etc, or "" for no symbol
	qreal symbolSize; // symbol size in pixels

	QOPlotDataHit getHits(qreal x, qreal y, qreal aspectRatio);
	virtual QString toJson();
	void boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax);

	static QOPlotCurveData *fromPoints(QVector<QPointF> points, QString legendLabel = QString(), QColor color = Qt::blue, QString pointSymbol = QString());
};

// Holds a data set to be plotted as a scatter plot.
class QOPlotScatterData : public QOPlotCurveData
{
public:
	explicit QOPlotScatterData();

	static QOPlotScatterData *fromPoints(QVector<QPointF> points, QString legendLabel = QString(), QColor color = Qt::blue, QString pointSymbol = "o");
};

class QOPlotStemData : public QOPlotCurveData
{
public:
	explicit QOPlotStemData();
};

class QOPlotBoxPlotItem
{
public:
	explicit QOPlotBoxPlotItem();
	explicit QOPlotBoxPlotItem(qreal wLow, qreal q1, qreal q2, qreal q3, qreal wHigh, QVector<qreal> outliers = QVector<qreal>());
	qreal quartileLow;
	qreal quartileMed;
	qreal quartileHigh;
	qreal whiskerLow;
	qreal whiskerHigh;
	QVector<qreal> outliers;
	// QString plotLabel;   // if empty it is set to the index
	QString legendLabel; // if empty it is set to the index
	QPen pen;
	QVector<QPointF> cdf;       // y between 0 and 1
	QVector<QPointF> histogram; // y between 0 and 1; x is bin center
	qreal histogramBinWidth;
	QVector<qreal> data;
};

class QOPlotBoxPlotData : public QOPlotData
{
public:
	explicit QOPlotBoxPlotData();
	QVector<QOPlotBoxPlotItem> items;
	virtual QString toJson();
	qreal boxWidth;
	qreal itemWidth;
	QString pointSymbol; // for outliers, use "+" etc
	qreal symbolSize; // symbol size in pixels

	void boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax);

private:
	QString legendLabel;
};

class QOPlotBandData : public QOPlotData
{
public:
	explicit QOPlotBandData();
	// these vectors need to have the same size, even if the pen is Qt::NoPen
	QVector<qreal> x;
	QVector<qreal> yCentral;
	QVector<qreal> yLower;
	QVector<qreal> yUpper;
	QVector<qreal> yWhiskerLower;
	QVector<qreal> yWhiskerUpper;
	// the legend is also drawn with centralPen
	QPen centralPen; // color, line style, width etc. Set cosmetic to true!
	QPen lowerPen; // color, line style, width etc. Set cosmetic to true!
	QPen upperPen; // color, line style, width etc. Set cosmetic to true!
	QBrush brush;
	QBrush brushWhiskers;

	void boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax);

	static QOPlotBandData *fromPoints(QVector<QPointF> points, int binCount = 10, QString legendLabel = QString(), QColor medianColor = Qt::blue, QColor bandColor = QColor::fromRgbF(0, 0, 1, 0.5), QColor bandWhiskersColor = QColor::fromRgbF(0, 0, 1, 0.25));
};

class QGraphicsDummyItem;

class QOGraphicsView : public QGraphicsView
{
	Q_OBJECT
public:
	QOGraphicsView(QWidget *parent = 0) : QGraphicsView(parent) {}
	QOGraphicsView(QGraphicsScene *scene, QWidget *parent = 0) : QGraphicsView(scene, parent) {}

signals:
	void mousePressed(QMouseEvent *event);
	void mouseMoved(QMouseEvent *event);
	void mouseReleased(QMouseEvent *event);
	void wheelMoved(QWheelEvent *event);

protected:
	void mousePressEvent(QMouseEvent *event) { emit mousePressed(event); }
	void mouseMoveEvent(QMouseEvent *event) { emit mouseMoved(event); }
	void mouseReleaseEvent(QMouseEvent *event) { emit mouseReleased(event); }
	void wheelEvent(QWheelEvent *event) { emit wheelMoved(event); }
};

class QOPlotDataHit
{
public:
	QOPlotDataHit();
	int dataIndex;
	int pointIndex;
	int userData;
	qreal distance;
};

class QOPlotHit
{
public:
	QOPlot *plot;
	QList<QOPlotDataHit> hits;
	QOPlotDataHit bestHit;
};

class QOPlotGroup;

class QOPlotWidget : public QWidget
{
	Q_OBJECT
public:
	explicit QOPlotWidget(QWidget *parent = 0,
				 int plotMinWidth = 0, int plotMinHeight = 0,
				 QSizePolicy policy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));

	QOPlot plot;

	// the size of the area between the widget border and the plot (with labels etc)
	qreal marginLeft;
	qreal marginRight;
	qreal marginTop;
	qreal marginBottom;
	// if the legend is outside, add this extra margin (should be ~1 line of text)
	qreal extraMargin;

	// used internally for keeping a zoom factor
	qreal zoomX, zoomY;

	QString plotFileName;

	// sets the data and redraws
	void setPlot(QOPlot plot);

	// sets the viewport such that the entire range of the data is visible
	// this is called automatically on middle click
	void autoAdjustAxes();

	// same as the other one, but forces an aspect ratio
	void autoAdjustAxes(qreal aspectRatio);

	// sets a fixed viewport
	void fixAxes(qreal xmin, qreal xmax, qreal ymin, qreal ymax);

	void setPlotMinSize(int w, int h) {
		minWidth = w;
		minHeight = h;
	}

	void setAntiAliasing(bool enable);
signals:
	void mouseHit(QOPlotHit hits);
	void viewportChanged(qreal xmin, qreal ymin, qreal xmax, qreal ymax, QOPlotWidget *plot);

public slots:
	// repopulates the scene (i.e. redraws everything); call this after changing data, labels, colors etc.
	void drawPlot();

	void saveImage(QString fileName = QString());
	void saveSvgImage(QString fileName = QString());
	void savePdf(QString fileName = QString());
	void saveEps(QString fileName = QString());
	void saveJson(QString fileName = QString());

	void test();
	void testHuge();
	void testBoxplot();

protected:
	QGraphicsScene *scenePlot;
	QOGraphicsView *viewPlot;
	QGraphicsDummyItem *itemPlot;
	QGraphicsDummyItem *itemLegend;
	QGraphicsRectItem *bg;
	QGraphicsRectItem *axesBgLeft;
	QGraphicsRectItem *axesBgRight;
	QGraphicsRectItem *axesBgTop;
	QGraphicsRectItem *axesBgBottom;
	QGraphicsRectItem *plotAreaBorder;
	QGraphicsTextItem *xLabel;
	QGraphicsTextItem *yLabel;
	QGraphicsTextItem *titleLabel;
	QList<QGraphicsItem*> tickItems;
	QList<QGraphicsItem*> trackItems;

	// used internally for keeping the axes origin offset
	// call updateGeometry() after changing this
	qreal world_x_off;
	qreal world_y_off;

	// the last mouse press coords
	qreal world_press_x;
	qreal world_press_y;

	// coords of a tracked point (we show the XY value for this)
	bool tracked;
	qreal track_x;
	qreal track_y;

	// compute the bbox of the data
	void dataBoundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax);

	// compute zoom and offset to obtain a viewport
	void recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax);
	void recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax, qreal aspectRatio);

	// returns the plot area in device and world (plot) coords
	void getPlotAreaGeometry(qreal &deviceW, qreal &deviceH, qreal &worldW, qreal &worldH);

	// resizes and repositions everything
	// call after changing the widget size, or plot viewport
	void updateGeometry();

	void resizeEvent(QResizeEvent*e);
	void setVisible(bool visible);

	// Tries to convert widget coords to world coords, and returns true in case of success.
	// Failure occurs when the position is outside the plot area.
	bool widgetCoordsToWorldCoords(qreal widget_x, qreal widget_y, qreal &world_x, qreal &world_y);
	void widgetDeltaToWorldDelta(qreal widget_dx, qreal widget_dy, qreal &world_dx, qreal &world_dy);

	// this allows us to place the widget into Qt layouts
	int minWidth;
	int minHeight;
	int plotMinWidth;
	int plotMinHeight;
	QSize minimumSizeHint() const {return QSize(minWidth, minHeight);}
	QSize sizeHint() const {return QSize(minWidth, minHeight);}

	void setMinSize(int w, int h) {
		minWidth = w;
		minHeight = h;
	}

	void emitViewportChanged();

protected slots:
	// process mouse events for drag and zoom
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void focusInEvent(QFocusEvent *event);
	void focusOutEvent(QFocusEvent *event);
	void enterEvent(QEvent *event);

private:
	friend class QOPlotGroup;
};

class QOPlotGroup : public QObject {
	Q_OBJECT

public:
	void addPlot(QOPlotWidget *plot);
	void clear();

protected:
	QSet<QOPlotWidget*> plots;

public slots:
	void viewportChanged(qreal xmin, qreal ymin, qreal xmax, qreal ymax, QOPlotWidget *sender);
};

#endif // QOPLOT_H

