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

#define TEST_OVECTOR

#include <QtGui/QApplication>
#include "mainwindow.h"

#include "bgp.h"
#include "convexhull.h"
#include "util.h"
#include "qoplot.h"
#include "../util/qbinaryheap.h"
#include "ovector.h"

#include <QtXml>

//#define TEST_IGP
//#define TEST_BGP

class MyProxyStyle : public QProxyStyle
{
public:
	int styleHint(StyleHint hint, const QStyleOption *option = 0,
				  const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
	{
		if (hint == QStyle::SH_DialogButtonLayout) {
			return QDialogButtonBox::WinLayout;
		}
		return QProxyStyle::styleHint(hint, option, widget, returnData);
	}
};

int main(int argc, char *argv[])
{
	//testOVector();
	//test_bits2DataSizeWithUnit();
	//testReadWriteNumberCompressed();
	unsigned int seed = clock() ^ time(NULL) ^ getpid();
	qDebug() << "seed:" << seed;
	srand(seed);

#if 0
	std::mt19937 randomGen(seed);  // mt19937 is a standard mersenne_twister_engine
	const qreal target = 0.998;
	const int numPackets = 10000;
	qreal minf = 2.0;
	qreal maxf = -1.0;
	for (int r = 0; r < 100000; r++) {
		qreal y, n;
		y = 0;
		n = 0;
		for (int i = 0; i < numPackets; i++) {
			if (frandex2mt(randomGen) > target) {
				n = n + 1;
			} else {
				y = y + 1;
			}
		}
		qreal f = y / (y + n);
		minf = qMin(minf, f);
		maxf = qMax(maxf, f);
	}
	qDebug() << minf << maxf;
	exit(0);
#endif

	QApplication::setGraphicsSystem("raster");

	qDebug() << "This machine has" << getNumCoresLinux() << "CPUs";

	QOPlot::makeCDFSampled(QList<qreal>() << 88.0, 0.0);

	QApplication a(argc, argv);

	a.setStyle(new MyProxyStyle);

	QFile cssFile(":/css/style.css");
	cssFile.open(QIODevice::ReadOnly);
	QTextStream in(&cssFile);
	QString css = in.readAll();
	cssFile.close();
	a.setStyleSheet(css);

	srand(time(NULL));

#if defined(TEST_IGP)
	Bgp::testIGP();
#elif defined(TEST_BGP)
	Bgp::testBGP();
#else
	MainWindow w;
    w.showMaximized();
    //w.showFullScreen();

	return a.exec();
#endif

	return 0;
}
