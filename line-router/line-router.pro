#-------------------------------------------------
#
# Project created by QtCreator 2011-06-07T14:43:49
#
#-------------------------------------------------

exists( ../line.pro ) {
	system(../line-router/make-remote.sh)
	TEMPLATE = subdirs
}

!exists( ../line.pro ) {
	QT       += core xml

	QT       -= gui

	TARGET   = line-router
	CONFIG   += console
	CONFIG   -= app_bundle

	TEMPLATE = app

	DEFINES += LINE_EMULATOR

	bundle.path = /usr/bin
	bundle.files = $$TARGET deploycore.pl
	INSTALLS += bundle

	INCLUDEPATH += ../util/
	INCLUDEPATH += ../line-gui/

	PF_RING_DIR = ../PF_RING-5.6.1
	#INCLUDEPATH += $$PF_RING_DIR/userland/c++ $$PF_RING_DIR/kernel $$PF_RING_DIR/kernel/plugins $$PF_RING_DIR/userland/libpcap-1.1.1-ring $$PF_RING_DIR/userland/lib
	#QMAKE_LIBS += $$PF_RING_DIR/userland/c++/libpfring_cpp.a $$PF_RING_DIR/userland/lib/libpfring.a $$PF_RING_DIR/userland/libpcap-1.1.1-ring/libpcap.a
	QMAKE_LIBS += /usr/local/lib/libpfring.a /usr/local/lib/libpcap.a

  QMAKE_CXXFLAGS += -std=c++0x -Wno-unused-local-typedefs

	system(pkg-config libnl-1) : {
		CONFIG += link_pkgconfig
		PKGCONFIG += libnl-1
	} else {
		error("You need to install nl (apt-get install libnl-dev (ubuntu), yum install libnl-devel (redhat), or zypper install libnl-devel (suse)).")
	}

	system(pkg-config libtcmalloc_minimal) : {
		CONFIG += link_pkgconfig
		PKGCONFIG += libtcmalloc_minimal
		DEFINES += USE_TC_MALLOC
		message("Linking with Google's tcmalloc.")
	} else {
    warning("Linking with system malloc. You should instead install tcmalloc for significatly reduced latency.")
	}

	SOURCES += main.cpp \
		pfcount.cpp \
		qpairingheap.cpp \
		pconsumer.cpp \
		pscheduler.cpp \
		psender.cpp \
		../util/bitarray.cpp \
		../line-gui/netgraphpath.cpp \
    ../line-gui/netgraphnode.cpp \
		../line-gui/netgraphedge.cpp \
		../line-gui/netgraphconnection.cpp \
		../line-gui/netgraphas.cpp \
		../line-gui/netgraph.cpp \
		../util/util.cpp \
		../malloc_profile/malloc_profile_wrapper.c \
    ../util/qbarrier.cpp \
		../line-gui/route.cpp \
		../tomo/tomodata.cpp \
		../util/chronometer.cpp \
		../line-gui/line-record.cpp \
		../line-gui/intervalmeasurements.cpp \
		../line-gui/flowevent.cpp

	OTHER_FILES += \
		make-remote.sh \
		deploycore-template.pl \
		../remote-config.sh

	HEADERS += \
		qpairingheap.h \
		pscheduler.h \
		pconsumer.h \
		psender.h \
		../util/bitarray.h \
		../line-gui/netgraphpath.h \
		../line-gui/netgraphnode.h \
		../line-gui/netgraphedge.h \
		../line-gui/netgraphconnection.h \
		../line-gui/netgraphas.h \
		../line-gui/netgraph.h \
		../util/util.h \
    ../util/qbarrier.h \
		../util/debug.h \
		../malloc_profile/malloc_profile_wrapper.h \
		../util/ovector.h \
    ../util/spinlockedqueue.h \
    ../util/waitfreequeuemoody.h \
    ../util/waitfreequeuedvyukov.h \
    ../util/readerwriterqueue.h \
    ../util/spinlockedqueue.h \
    ../util/producerconsumerqueue.h \
		../line-gui/route.h \
		../tomo/tomodata.h \
		../util/chronometer.h \
		../line-gui/line-record.h \
		../line-gui/intervalmeasurements.h \
		../line-gui/queuing-decisions.h \
		../line-gui/qrgb-line.h \
		../line-gui/flowevent.h

	QMAKE_CFLAGS_DEBUG += -g -fno-omit-frame-pointer -rdynamic
	QMAKE_CXXFLAGS_DEBUG += -g -fno-omit-frame-pointer -rdynamic
	QMAKE_LFLAGS_DEBUG += -g -fno-omit-frame-pointer -rdynamic
}

HEADERS += \
    ../util/readerwriterqueue.h \
    ../util/waitfreequeuemoody.h \
    ../util/waitfreequeuedvyukov.h
