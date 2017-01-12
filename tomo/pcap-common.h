#ifndef PCAPCOMMON_H
#define PCAPCOMMON_H

#include <QtCore>

// Pcap format described in http://wiki.wireshark.org/Development/LibpcapFileFormat

typedef struct {
	quint32 magic_number;  // magic number
	quint16 version_major; // major version number
	quint16 version_minor; // minor version number
	qint32 thiszone;       // GMT to local correction
	quint32 sigfigs;       // accuracy of timestamps
	quint32 snaplen;       // ax length of captured packets, in octets
	quint32 network;       // data link type, one of the LINKTYPE_* constants defined below
} PcapHeader;

typedef struct {
	quint32 ts_sec;   // timestamp seconds
	quint32 ts_nsec;  // timestamp nanoseconds
	quint32 incl_len; // number of octets of packet saved in file
	quint32 orig_len; // actual length of packet
} PcapPacketHeader;

#endif // PCAPCOMMON_H
