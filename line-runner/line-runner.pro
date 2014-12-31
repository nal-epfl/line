#-------------------------------------------------
#
# Project created by QtCreator 2014-01-02T15:56:46
#
#-------------------------------------------------

!exists(../remote_config.h) {
	system(cd $$PWD/..; /bin/sh remote-config.sh)
}
system(cd $$PWD/..; test remote-config.sh -nt remote_config.h):system(cd $$PWD/..; /bin/sh remote-config.sh ; touch line-gui/line-gui.pro)

QT += core xml network
QT -= gui

TARGET = line-runner
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

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

LIBS += -lunwind

gitinfobundle.target = gitinfo
gitinfobundle.commands = cd .. ; \
                         git status 2>&1 1> git-status.txt ; \
                         git log --pretty=format:\'%h %an %ae %s (%ci) %d%\' -n 1  2>&1 1> git-log.txt ; \
                         git diff 2>&1 1> git-diff.txt

gitinfobundle.depends =
QMAKE_EXTRA_TARGETS += gitinfobundle
PRE_TARGETDEPS = gitinfo

SOURCES += main.cpp \
    ../line-gui/route.cpp \
    ../line-gui/remoteprocessssh.cpp \
    ../line-gui/netgraphpath.cpp \
    ../line-gui/netgraphnode.cpp \
    ../line-gui/netgraphedge.cpp \
    ../line-gui/netgraphconnection.cpp \
    ../line-gui/netgraphas.cpp \
    ../line-gui/netgraph.cpp \
    ../line-gui/line-record_processor.cpp \
    ../line-gui/line-record.cpp \
    ../line-gui/intervalmeasurements.cpp \
    ../line-gui/convexhull.cpp \
    ../line-gui/bgp.cpp \
    ../util/util.cpp \
    ../util/unionfind.cpp \
    ../util/qbinaryheap.cpp \
    ../util/chronometer.cpp \
    ../line-gui/gephilayoutnetgraph.cpp \
    ../line-gui/gephilayout.cpp \
    ../tomo/readpacket.cpp \
    run_experiment.cpp \
    run_experiment_params.cpp \
    deploy.cpp \
    ../tomo/tomodata.cpp \
    simulate_experiment.cpp \
    export_matlab.cpp \
    ../util/bitarray.cpp \
    ../line-gui/traffictrace.cpp \
    ../tomo/pcap-qt.cpp \
    ../util/tinyhistogram.cpp \
    ../util/gitinfo.cpp

HEADERS += \
    ../line-gui/netgraph.h \
    ../line-gui/route.h \
    ../line-gui/remoteprocessssh.h \
    ../line-gui/queuing-decisions.h \
    ../line-gui/netgraphpath.h \
    ../line-gui/netgraphnode.h \
    ../line-gui/netgraphedge.h \
    ../line-gui/netgraphconnection.h \
    ../line-gui/netgraphcommon.h \
    ../line-gui/netgraphas.h \
    ../line-gui/line-record_processor.h \
    ../line-gui/line-record.h \
    ../line-gui/intervalmeasurements.h \
    ../line-gui/convexhull.h \
    ../line-gui/bgp.h \
    ../util/util.h \
    ../util/unionfind.h \
    ../util/qbinaryheap.h \
    ../util/debug.h \
    ../util/chronometer.h \
    ../line-gui/qrgb-line.h \
    ../line-gui/gephilayoutnetgraph.h \
    ../line-gui/gephilayout.h \
    ../tomo/readpacket.h \
    run_experiment.h \
    run_experiment_params.h \
    deploy.h \
    ../tomo/tomodata.h \
    simulate_experiment.h \
    export_matlab.h \
    result_processing.h \
    ../util/bitarray.h \
    ../line-gui/traffictrace.h \
    ../tomo/pcap-qt.h \
    ../util/tinyhistogram.h \
    ../util/embed-file.h \
    ../util/gitinfo.h

exists(result_processing.cpp) {
    SOURCES += result_processing.cpp
    DEFINES += HAVE_RESULT_PROCESSING
}

INCLUDEPATH += ../util/
INCLUDEPATH += ../line-gui/
INCLUDEPATH += ../tomo/

RESOURCES += \
    ../line-gui/resources.qrc

installfiles.files += $$TARGET
installfiles.path = /usr/bin
INSTALLS += installfiles
