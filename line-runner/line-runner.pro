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

QMAKE_CXXFLAGS += -std=c++0x -Wno-unused-local-typedefs

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
    ../util/bitarray.cpp

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
    ../util/bitarray.h

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
