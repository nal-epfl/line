#-------------------------------------------------
#
# Project created by QtCreator 2011-03-28T15:57:20
#
#-------------------------------------------------

!exists(../remote_config.h) {
	system(cd $$PWD/..; /bin/sh remote-config.sh)
}
system(cd $$PWD/..; [ ! -f remote_config.h ] || test remote-config.sh -nt remote_config.h):system(cd $$PWD/..; /bin/sh remote-config.sh ; touch line-gui/line-gui.pro)

QT += core gui xml svg opengl network testlib

TARGET = line-gui
TEMPLATE = app

LIBS += -lglpk -llzma

QMAKE_CXXFLAGS += -std=c++0x -Wno-unused-local-typedefs

exists( /usr/include/glpk ) {
	INCLUDEPATH += /usr/include/glpk
}

extralib.target = extra
extralib.commands =	cd $$PWD/..; \
				test remote-config.sh -nt remote_config.h && \
				/bin/sh remote-config.sh || /bin/true
extralib.depends =
QMAKE_EXTRA_TARGETS += extralib
PRE_TARGETDEPS = extra

SOURCES += main.cpp\
        mainwindow.cpp \
    netgraph.cpp \
    netgraphnode.cpp \
    netgraphedge.cpp \
    netgraphscene.cpp \
    netgraphscenenode.cpp \
    netgraphsceneedge.cpp \
    ../util/util.cpp \
    netgraphpath.cpp \
    briteimporter.cpp \
    netgraphconnection.cpp \
    bgp.cpp \
    gephilayout.cpp \
    convexhull.cpp \
    netgraphas.cpp \
    netgraphsceneas.cpp \
    remoteprocessssh.cpp \
    netgraphsceneconnection.cpp \
    qdisclosure.cpp \
    qaccordion.cpp \
    qoplot.cpp \
    nicelabel.cpp \
    route.cpp \
    mainwindow_scalability.cpp \
    mainwindow_batch.cpp \
    mainwindow_deployment.cpp \
    mainwindow_BRITE.cpp \
    mainwindow_remote.cpp \
    mainwindow_results.cpp \
    ../tomo/tomodata.cpp \
    qgraphicstooltip.cpp \
    flowlayout.cpp \
    qcoloredtabwidget.cpp \
    ../util/profiling.cpp \
    ../util/graphviz.cpp \
    ../util/chronometer.cpp \
    ../util/qswiftprogressdialog.cpp \
    ../line-router/qpairingheap.cpp \
    ../util/qbinaryheap.cpp \
    mainwindow_graphml.cpp \
    graphmlimporter.cpp \
    netgrapheditor.cpp \
    netgraphphasing.cpp \
    ../util/unionfind.cpp \
    ../util/colortable.cpp \
    qgraphicsviewgood.cpp \
    writelog.cpp \
    gephilayoutnetgraph.cpp \
    line-record_processor.cpp \
    line-record.cpp \
    mainwindow_importtraces.cpp \
    intervalmeasurements.cpp \
    mainwindow_capture.cpp \
    ../tomo/readpacket.cpp \
    qwraplabel.cpp \
    mainwindow_fake_emulation.cpp \
    ../line-runner/run_experiment_params.cpp \
    flowevent.cpp \
    ../util/ovector.cpp \
    ../util/bitarray.cpp

HEADERS  += mainwindow.h \
    netgraph.h \
    netgraphnode.h \
    netgraphedge.h \
    netgraphscene.h \
    netgraphscenenode.h \
    netgraphsceneedge.h \
    ../util/util.h \
    ../util/debug.h \
    netgraphpath.h \
    briteimporter.h \
    netgraphconnection.h \
    bgp.h \
    gephilayout.h \
    convexhull.h \
    netgraphas.h \
    netgraphsceneas.h \
    remoteprocessssh.h \
    netgraphsceneconnection.h \
    qdisclosure.h \
    qaccordion.h \
    qoplot.h \
    nicelabel.h \
    route.h \
    ../tomo/tomodata.h \
    qgraphicstooltip.h \
    flowlayout.h \
    qcoloredtabwidget.h \
    ../util/profiling.h \
    ../util/graphviz.h \
    ../util/chronometer.h \
    ../util/qswiftprogressdialog.h \
    ../line-router/qpairingheap.h \
    ../util/qbinaryheap.h \
    topogen.h \
    graphmlimporter.h \
    netgrapheditor.h \
    netgraphphasing.h \
    ../util/unionfind.h \
    ../util/colortable.h \
    qgraphicsviewgood.h \
    netgraphcommon.h \
    writelog.h \
    line-record.h \
    line-record_processor.h \
    intervalmeasurements.h \
    ../tomo/pcap-qt.h \
    ../tomo/readpacket.h \
    queuing-decisions.h \
    qwraplabel.h \
    qrgb-line.h \
    ../line-runner/run_experiment_params.h \
    flowevent.h \
    ../util/ovector.h \
    ../util/bitarray.h

FORMS    += mainwindow.ui \
    ../util/qswiftprogressdialog.ui \
    netgrapheditor.ui \
    formcoverage.ui

exists(customcontrols.h) {
    HEADERS += customcontrols.h
    SOURCES += customcontrols.cpp customcontrols_*.cpp
    FORMS += customcontrols.ui
    DEFINES += HAVE_CUSTOM_CONTROLS
}

INCLUDEPATH += ../util/
INCLUDEPATH += ../line-runner/
INCLUDEPATH += /usr/include/glpk

OTHER_FILES += \
    ../remote-config.sh \
    style.css \
    ../credits.txt \
    clearall.pl \
    zzz.txt \
    junk.txt \
    ../benchmark.txt \
    ../faq.txt \
    ../line-topologies/www/util.js \
    ../line-topologies/www/index.html \
    ../line-topologies/www/timeline.html

RESOURCES += \
    resources.qrc

#QMAKE_LFLAGS += -Wl,-rpath -Wl,/usr/local/lib


























