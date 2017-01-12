#-------------------------------------------------
#
# Project created by QtCreator 2011-11-23T02:55:43
#
#-------------------------------------------------

exists( ../line.pro ) {
  system(../line-traffic/make-remote.sh)
	TEMPLATE = subdirs
}

DEFINES += \'SRCDIR=\"$$_PRO_FILE_PWD_\"\'

!exists( ../line.pro ) {
	QT += core xml network

	QT -= gui

  TARGET = line-traffic
	CONFIG   += console
	CONFIG   -= app_bundle

	TEMPLATE = app

	INCLUDEPATH += /usr/include/libev
	INCLUDEPATH += ../util
	INCLUDEPATH += ../line-gui
  INCLUDEPATH += ../tomo
  LIBS += -lev -lunwind -lpcap

  QMAKE_CXXFLAGS += -std=c++0x

  DEFINES += LINE_CLIENT

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

#  QMAKE_CFLAGS += -fsanitize=address -fno-omit-frame-pointer
#  QMAKE_CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
#  QMAKE_LFLAGS += -fsanitize=address -fno-omit-frame-pointer

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
    tcpdashsource.h \
    ../tomo/pcap-qt.h \
    ../util/embed-file.h \
    ../util/gitinfo.h \
    ../util/json.h \
    ../tomo/readpacket.h \
    ../util/compresseddevice.h \
    evpid.h \
    ../tomo/fastpcap.h \
    ../tomo/pcap-common.h \
    ../line-gui/traffictrace.h

SOURCES += \
    tcpdashsource.cpp \
    ../util/gitinfo.cpp \
    ../util/json.cpp \
    ../util/compresseddevice.cpp \
    evpid.cpp \
    ../tomo/fastpcap.cpp \
    ../tomo/pcap-qt.cpp \
    ../tomo/readpacket.cpp \
    ../line-gui/traffictrace.cpp

OTHER_FILES += \
    virtual-interfaces-howto.txt

DISTFILES += \
    config-file-howto.txt \
    iperf.txt
