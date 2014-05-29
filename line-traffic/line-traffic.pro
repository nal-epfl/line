#-------------------------------------------------
#
# Project created by QtCreator 2011-11-23T02:55:43
#
#-------------------------------------------------

exists( ../line.pro ) {
  system(../line-traffic/make-remote.sh)
	TEMPLATE = subdirs
}

!exists( ../line.pro ) {
	QT += core xml

	QT -= gui

  TARGET = line-traffic
	CONFIG   += console
	CONFIG   -= app_bundle

	TEMPLATE = app

	INCLUDEPATH += /usr/include/libev
	INCLUDEPATH += ../util
	INCLUDEPATH += ../line-gui
	LIBS += -lev

  QMAKE_CXXFLAGS += -std=c++0x

  DEFINES += LINE_CLIENT

  QMAKE_CFLAGS += -Wno-strict-aliasing -Wno-unused-local-typedefs
  QMAKE_CXXFLAGS += -Wno-strict-aliasing -Wno-unused-local-typedefs

  system(pkg-config libtcmalloc_minimal) : {
    CONFIG += link_pkgconfig
    PKGCONFIG += libtcmalloc_minimal
    DEFINES += USE_TC_MALLOC
    message("Linking with Google's tcmalloc.")
  } else {
    warning("Linking with system malloc. You should instead install tcmalloc for significatly reduced latency.")
  }

	SOURCES += main.cpp \
		../line-gui/netgraphpath.cpp \
		../line-gui/netgraphnode.cpp \
		../line-gui/netgraphedge.cpp \
		../line-gui/netgraphconnection.cpp \
		../line-gui/netgraphas.cpp \
		../line-gui/netgraph.cpp \
		../util/util.cpp \
		../line-gui/route.cpp \
		../line-gui/gephilayout.cpp \
		../line-gui/convexhull.cpp \
		../line-gui/bgp.cpp \
		evtcp.cpp \
		tcpsink.cpp \
		evudp.cpp \
		udpsink.cpp \
		udpcbr.cpp \
		udpvbr.cpp \
		../util/chronometer.cpp \
		tcpparetosource.cpp \
		readerwriter.cpp \
		tcpsource.cpp \
		udpsource.cpp

	HEADERS += \
		../line-gui/netgraphpath.h \
		../line-gui/netgraphnode.h \
		../line-gui/netgraphedge.h \
		../line-gui/netgraphconnection.h \
		../line-gui/netgraphas.h \
		../line-gui/netgraph.h \
		../util/util.h \
		../util/debug.h \
		../line-gui/route.h \
		../line-gui/gephilayout.h \
		../line-gui/convexhull.h \
		../line-gui/bgp.h \
		evtcp.h \
		tcpsink.h \
		evudp.h \
		udpsink.h \
		udpcbr.h \
		udpvbr.h \
		../util/chronometer.h \
		tcpparetosource.h \
		readerwriter.h \
		tcpsource.h \
		udpsource.h \
		../line-gui/qrgb-line.h

	OTHER_FILES += \
		make-remote.sh

	bundle.path = /usr/bin
	bundle.files = $$TARGET
	INSTALLS += bundle
}

HEADERS += \
    tcpdashsource.h

SOURCES += \
    tcpdashsource.cpp
