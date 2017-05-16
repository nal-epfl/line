#-------------------------------------------------
#
# Project created by QtCreator 2011-03-28T15:57:20
#
#-------------------------------------------------

!exists(../remote_config.h) {
	system(cd $$PWD/..; /bin/sh remote-config.sh)
}
system(cd $$PWD/..; [ ! -f remote_config.h ] || test remote-config.sh -nt remote_config.h):system(cd $$PWD/..; /bin/sh remote-config.sh ; touch line-gui/line-gui.pro)

DEFINES += \'SRCDIR=\"$$_PRO_FILE_PWD_\"\'

QT += core gui xml svg opengl network testlib

TARGET = line-gui
TEMPLATE = app

LIBS += -lglpk -llzma -lunwind -lX11 -lpcap

QMAKE_CFLAGS += -std=gnu99 -fno-strict-overflow -fno-strict-aliasing -Wno-unused-local-typedefs -gdwarf-2
QMAKE_CXXFLAGS += -std=c++11 -fno-strict-overflow -fno-strict-aliasing -Wno-unused-local-typedefs -gdwarf-2
QMAKE_LFLAGS += -flto -fno-strict-overflow -fno-strict-aliasing

QMAKE_CFLAGS += -fstack-protector-all --param ssp-buffer-size=4
QMAKE_CXXFLAGS += -fstack-protector-all --param ssp-buffer-size=4
QMAKE_LFLAGS += -fstack-protector-all --param ssp-buffer-size=4

QMAKE_CFLAGS += -fPIE -pie -rdynamic
QMAKE_CXXFLAGS += -fPIE -pie -rdynamic
QMAKE_LFLAGS += -fPIE -pie -rdynamic

QMAKE_CFLAGS += -Wl,-z,relro,-z,now
QMAKE_CXXFLAGS += -Wl,-z,relro,-z,now
QMAKE_LFLAGS += -Wl,-z,relro,-z,now

#QMAKE_CFLAGS += -O0 -g3 -fsanitize=address -fno-common -fno-omit-frame-pointer
#QMAKE_CXXFLAGS += -O0 -g3 -fsanitize=address -fno-common -fno-omit-frame-pointer
#QMAKE_LFLAGS += -O0 -g3 -fsanitize=address -fno-common -fno-omit-frame-pointer

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

gitinfobundle.target = gitinfo
gitinfobundle.commands = cd .. ; \
                         git status 2>&1 1> git-status.txt ; \
                         git log --pretty=format:\'%h %an %ae %s (%ci) %d%\' -n 1  2>&1 1> git-log.txt ; \
                         git diff 2>&1 1> git-diff.txt

gitinfobundle.depends =
QMAKE_EXTRA_TARGETS += gitinfobundle
PRE_TARGETDEPS = gitinfo

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
    ../util/bitarray.cpp \
    traffictrace.cpp \
    ../tomo/pcap-qt.cpp \
    ../util/tinyhistogram.cpp \
    customcontrols.cpp \
    customcontrols_qos.cpp \
    ../util/json.cpp \
    ../line-gui/end_to_end_measurements.cpp \
    ../util/compresseddevice.cpp \
    ../tomo/fastpcap.cpp \
    erroranalysisdata.cpp

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
    ../util/bitarray.h \
    traffictrace.h \
    ../util/tinyhistogram.h \
    customcontrols.h \
    ../util/json.h \
    ../line-gui/end_to_end_measurements.h \
    ../util/compresseddevice.h \
    ../tomo/fastpcap.h \
    ../tomo/pcap-common.h \
    erroranalysisdata.h

FORMS    += mainwindow.ui \
    ../util/qswiftprogressdialog.ui \
    netgrapheditor.ui \
    formcoverage.ui \
    customcontrols.ui

INCLUDEPATH += ../util/
INCLUDEPATH += ../line-runner/
INCLUDEPATH += ../tomo/
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


























