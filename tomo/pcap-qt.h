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

#ifndef PCAP_H
#define PCAP_H

#include <QtCore>

// Pcap format described in http://wiki.wireshark.org/Development/LibpcapFileFormat

typedef struct {
		quint32 magic_number;   // magic number
		quint16 version_major;  // major version number
		quint16 version_minor;  // minor version number
		qint32  thiszone;       // GMT to local correction
		quint32 sigfigs;        // accuracy of timestamps
		quint32 snaplen;        // ax length of captured packets, in octets
		quint32 network;        // data link type, one of the LINKTYPE_* constants defined below
} PcapHeader;

typedef struct {
		quint32 ts_sec;         // timestamp seconds
		quint32 ts_usec;        // timestamp microseconds
		quint32 incl_len;       // number of octets of packet saved in file
		quint32 orig_len;       // actual length of packet
} PcapPacketHeader;

// Link type constants described on http://www.tcpdump.org/linktypes.html

#define LINKTYPE_NULL 0
// DLT_NULL BSD loopback encapsulation; the link layer header is a 4-byte field, in host byte order, containing a PF_ value from socket.h for the network-layer protocol of the packet.
// Note that ``host byte order'' is the byte order of the machine on which the packets are captured, and the PF_ values are for the OS of the machine on which the packets are captured; if a live capture is being done, ``host byte order'' is the byte order of the machine capturing the packets, and the PF_ values are those of the OS of the machine capturing the packets, but if a ``savefile'' is being read, the byte order and PF_ values are not necessarily those of the machine reading the capture file.

#define LINKTYPE_ETHERNET 1
// DLT_EN10MB IEEE 802.3 Ethernet (10Mb, 100Mb, 1000Mb, and up); the 10MB in the
// DLT_ name is historical.

#define LINKTYPE_AX25 3
// DLT_AX25 AX.25 packet, with nothing preceding it.

#define LINKTYPE_IEEE802_5 6
// DLT_IEEE802 IEEE 802.5 Token Ring; the IEEE802, without _5, in the
// DLT_ name is historical.

#define LINKTYPE_ARCNET_BSD 7
// DLT_ARCNET ARCNET Data Packets, as described by the ARCNET Trade Association standard ATA 878.1-1999, but without the Starting Delimiter, Information Length, or Frame Check Sequence fields, and with only the first ISU of the Destination Identifier. For most packet types, ARCNET Trade Association draft standard ATA 878.2 is also used. See also RFC 1051 and RFC 1201; for RFC 1051 frames, ATA 878.2 is not used.

#define LINKTYPE_SLIP 8
// DLT_SLIP SLIP, encapsulated with a LINKTYPE_SLIP header.

#define LINKTYPE_PPP 9
// DLT_PPP PPP, as per RFC 1661 and RFC 1662; if the first 2 bytes are 0xff and 0x03, it's PPP in HDLC-like framing, with the PPP header following those two bytes, otherwise it's PPP without framing, and the packet begins with the PPP header.

#define LINKTYPE_FDDI 10
// DLT_FDDI FDDI, as specified by ANSI INCITS 239-1994.

#define LINKTYPE_PPP_HDLC 50
// DLT_PPP_SERIAL PPP in HDLC-like framing, as per RFC 1662, or Cisco PPP with HDLC framing, as per section 4.3.1 of RFC 1547; the first byte will be 0xFF for PPP in HDLC-like framing, and will be 0x0F or 0x8F for Cisco PPP with HDLC framing.

#define LINKTYPE_PPP_ETHER 51
// DLT_PPP_ETHER PPPoE; the packet begins with a PPPoE header, as per RFC 2516.

#define LINKTYPE_ATM_RFC1483 100
// DLT_ATM_RFC1483 RFC 1483 LLC/SNAP-encapsulated ATM; the packet begins with an IEEE 802.2 LLC header.

#define LINKTYPE_RAW 101
// DLT_RAW Raw IP; the packet begins with an IPv4 or IPv6 header, with the "version" field of the header indicating whether it's an IPv4 or IPv6 header.

#define LINKTYPE_C_HDLC 104
// DLT_C_HDLC Cisco PPP with HDLC framing, as per section 4.3.1 of RFC 1547.

#define LINKTYPE_IEEE802_11 105
// DLT_IEEE802_11 IEEE 802.11 wireless LAN.

#define LINKTYPE_FRELAY 107
// DLT_FRELAY Frame Relay

#define LINKTYPE_LOOP 108
// DLT_LOOP OpenBSD loopback encapsulation; the link-layer header is a 4-byte field, in network byte order, containing a PF_ value from OpenBSD's socket.h for the network-layer protocol of the packet.

#define Note that, if a ``savefile'' is being read, those PF_ values are not necessarily those of the machine reading the capture file.

#define LINKTYPE_LINUX_SLL 113
// DLT_LINUX_SLL Linux "cooked" capture encapsulation.

#define LINKTYPE_LTALK 114
// DLT_LTALK Apple LocalTalk; the packet begins with an AppleTalk LocalTalk Link Access Protocol header, as described in chapter 1 of Inside AppleTalk, Second Edition.

#define LINKTYPE_PFLOG 117
// DLT_PFLOG OpenBSD pflog; the link-layer header contains a "struct pfloghdr" structure, as defined by the host on which the file was saved. (This differs from operating system to operating system and release to release; there is nothing in the file to indicate what the layout of that structure is.)

#define LINKTYPE_IEEE802_11_PRISM 119
// DLT_PRISM_HEADER Prism monitor mode information followed by an 802.11 header.

#define LINKTYPE_IP_OVER_FC 122
// DLT_IP_OVER_FC RFC 2625 IP-over-Fibre Channel, with the link-layer header being the Network_Header as described in that RFC.

#define LINKTYPE_SUNATM 123
// DLT_SUNATM ATM traffic, encapsulated as per the scheme used by SunATM devices.

#define LINKTYPE_IEEE802_11_RADIOTAP 127
// DLT_IEEE802_11_RADIO Radiotap link-layer information followed by an 802.11 header.

#define LINKTYPE_ARCNET_LINUX 129
// DLT_ARCNET_LINUX ARCNET Data Packets, as described by the ARCNET Trade Association standard ATA 878.1-1999, but without the Starting Delimiter, Information Length, or Frame Check Sequence fields, with only the first ISU of the Destination Identifier, and with an extra two-ISU "offset" field following the Destination Identifier. For most packet types, ARCNET Trade Association draft standard ATA 878.2 is also used; however, no exception frames are supplied, and reassembled frames, rather than fragments, are supplied. See also RFC 1051 and RFC 1201; for RFC 1051 frames, ATA 878.2 is not used.

#define LINKTYPE_APPLE_IP_OVER_IEEE1394 138
// DLT_APPLE_IP_OVER_IEEE1394 Apple IP-over-IEEE 1394 cooked header.

#define LINKTYPE_MTP2_WITH_PHDR 139
// DLT_MTP2_WITH_PHDR Signaling System 7 Message Transfer Part Level 2, as specified by ITU-T Recommendation Q.703, preceded by a pseudo-header.

#define LINKTYPE_MTP2 140
// DLT_MTP2 Signaling System 7 Message Transfer Part Level 2, as specified by ITU-T Recommendation Q.703.

#define LINKTYPE_MTP3 141
// DLT_MTP3 Signaling System 7 Message Transfer Part Level 3, as specified by ITU-T Recommendation Q.704, with no MTP2 header preceding the MTP3 packet.

#define LINKTYPE_SCCP 142
// DLT_SCCP Signaling System 7 Signalling Connection Control Part, as specified by ITU-T Recommendation Q.711, ITU-T Recommendation Q.712, ITU-T Recommendation Q.713, and ITU-T Recommendation Q.714, with no MTP3 or MTP2 headers preceding the SCCP packet.

#define LINKTYPE_DOCSIS 143
// DLT_DOCSIS DOCSIS MAC frames, as described by the DOCSIS 3.0 MAC and Upper Layer Protocols Interface Specification.

#define LINKTYPE_LINUX_IRDA 144
// DLT_LINUX_IRDA Linux-IrDA packets, with a LINKTYPE_LINUX_IRDA header, with the payload for IrDA frames beginning with by the IrLAP header as defined by IrDA Data Specifications, including the IrDA Link Access Protocol specification.

#define LINKTYPE_USER0 147
#define LINKTYPE_USER1 148
#define LINKTYPE_USER2 149
#define LINKTYPE_USER3 150
#define LINKTYPE_USER4 151
#define LINKTYPE_USER5 152
#define LINKTYPE_USER6 153
#define LINKTYPE_USER7 154
#define LINKTYPE_USER8 155
#define LINKTYPE_USER9 156
#define LINKTYPE_USER10 157
#define LINKTYPE_USER11 158
#define LINKTYPE_USER12 159
#define LINKTYPE_USER13 160
#define LINKTYPE_USER14 161
#define LINKTYPE_USER15 162
// DLT_USER0-DLT_USER15 Reserved for private use; see above.

#define LINKTYPE_IEEE802_11_AVS 163
// DLT_IEEE802_11_RADIO_AVS AVS monitor mode information followed by an 802.11 header.

#define LINKTYPE_BACNET_MS_TP 165
// DLT_BACNET_MS_TP BACnet MS/TP frames, as specified by section 9.3 MS/TP Frame Format of ANSI/ASHRAE Standard 135, BACnet® - A Data Communication Protocol for Building Automation and Control Networks, including the preamble and, if present, the Data CRC.

#define LINKTYPE_PPP_PPPD 166
// DLT_PPP_PPPD PPP in HDLC-like encapsulation, but with the 0xff address byte replaced by a direction indication - 0x00 for incoming and 0x01 for outgoing.

#define LINKTYPE_GPRS_LLC 169
// DLT_GPRS_LLC General Packet Radio Service Logical Link Control, as defined by 3GPP TS 04.64.

#define LINKTYPE_LINUX_LAPD 177
// DLT_LINUX_LAPD Link Access Procedures on the D Channel (LAPD) frames, as specified by ITU-T Recommendation Q.920 and ITU-T Recommendation Q.921, captured via vISDN, with a LINKTYPE_LINUX_LAPD header, followed by the Q.921 frame, starting with the address field.

#define LINKTYPE_BLUETOOTH_HCI_H4 187
// DLT_BLUETOOTH_HCI_H4 Bluetooth HCI UART transport layer; the frame contains an HCI packet indicator byte, as specified by Volume 4, part A of the Core Version 4.0 of the Bluetooth specifications, followed by an HCI packet of the specified packet type, as specified by Volume 2, Part E of the same document.

#define LINKTYPE_USB_LINUX 189
// DLT_USB_LINUX USB packets, beginning with a Linux USB header, as specified by the struct usbmon_packet in the Documentation/usb/usbmon.txt file in the Linux source tree. Only the first 48 bytes of that header are present. All fields in the header are in the host byte order for the pcap file, as specified by the file's magic number, or for the section of the pcap-ng file, as specified by the Section Header Block.

#define LINKTYPE_PPI 192
// DLT_PPI Per-Packet Information information, as specified by the Per-Packet Information Header Specification, followed by a packet with the LINKTYPE_ value specified by the pph_dlt field of that header.

#define LINKTYPE_IEEE802_15_4 195
// DLT_IEEE802_15_4 IEEE 802.15.4 wireless Personal Area Network, with each packet having the FCS at the end of the frame.

#define LINKTYPE_SITA 196
// DLT_SITA Various link-layer types, with a pseudo-header, for SITA.

#define LINKTYPE_ERF 197
// DLT_ERF Various link-layer types, with a pseudo-header, for Endace DAG cards; encapsulates Endace ERF records.

#define LINKTYPE_BLUETOOTH_HCI_H4_WITH_PHDR 201
// DLT_BLUETOOTH_HCI_H4_WITH_PHDR Bluetooth HCI UART transport layer; the frame contains a 4-byte direction field, in network byte order (big-endian), the low-order bit of which is set if the frame was sent from the host to the controller and clear if the frame was received by the host from the controller, followed by an HCI packet indicator byte, as specified by Volume 4, part A of the Core Version 4.0 of the Bluetooth specifications, followed by an HCI packet of the specified packet type, as specified by Volume 2, Part E of the same document.

#define LINKTYPE_AX25_KISS 202
// DLT_AX25_KISS AX.25 packet, with a 1-byte KISS header containing a type indicator.

#define LINKTYPE_LAPD 203
// DLT_LAPD Link Access Procedures on the D Channel (LAPD) frames, as specified by ITU-T Recommendation Q.920 and ITU-T Recommendation Q.921, starting with the address field, with no pseudo-header.

#define LINKTYPE_PPP_WITH_DIR 204
// DLT_PPP_WITH_DIR PPP, as per RFC 1661 and RFC 1662, preceded with a one-byte pseudo-header with a zero value meaning "received by this host" and a non-zero value meaning "sent by this host".

#define LINKTYPE_C_HDLC_WITH_DIR 205
// DLT_C_HDLC_WITH_DIR Cisco PPP with HDLC framing, as per section 4.3.1 of RFC 1547, preceded with a one-byte pseudo-header with a zero value meaning "received by this host" and a non-zero value meaning "sent by this host".

#define LINKTYPE_FRELAY_WITH_DIR 206
// DLT_FRELAY_WITH_DIR Frame Relay, preceded with a one-byte pseudo-header with a zero value meaning "received by this host" and a non-zero value meaning "sent by this host".

#define LINKTYPE_IPMB_LINUX 209
// DLT_IPMB_LINUX IPMB over an I2C circuit, with a Linux-specific pseudo-header.

#define LINKTYPE_IEEE802_15_4_NONASK_PHY 215
// DLT_IEEE802_15_4_NONASK_PHY IEEE 802.15.4 wireless Personal Area Network, with each packet having the FCS at the end of the frame, and with the PHY-level data for non-ASK PHYs (4 octets of 0 as preamble, one octet of SFD, one octet of frame length + reserved bit) preceding the MAC-layer data (starting with the frame control field).

#define LINKTYPE_USB_LINUX_MMAPPED 220
// DLT_USB_LINUX_MMAPPED USB packets, beginning with a Linux USB header, as specified by the struct usbmon_packet in the Documentation/usb/usbmon.txt file in the Linux source tree. All 64 bytes of the header are present. All fields in the header are in the host byte order for the pcap file, as specified by the file's magic number, or for the section of the pcap-ng file, as specified by the Section Header Block. For isochronous transfers, the ndesc field specifies the number of isochronous descriptors that follow.

#define LINKTYPE_FC_2 224
// DLT_FC_2 Fibre Channel FC-2 frames, beginning with a Frame_Header.

#define LINKTYPE_FC_2_WITH_FRAME_DELIMS 225
// DLT_FC_2_WITH_FRAME_DELIMS Fibre Channel FC-2 frames, beginning an encoding of the SOF, followed by a Frame_Header, and ending with an encoding of the SOF.

#define The encodings represent the frame delimiters as 4-byte sequences representing the corresponding ordered sets, with K28.5 represented as 0xBC, and the D symbols as the corresponding byte values; for example, SOFi2, which is K28.5 - D21.5 - D1.2 - D21.2, is represented as 0xBC 0xB5 0x55 0x55.

#define LINKTYPE_IPNET 226
// DLT_IPNET Solaris ipnet pseudo-header, followed by an IPv4 or IPv6 datagram.

#define LINKTYPE_CAN_SOCKETCAN 227
// DLT_CAN_SOCKETCAN CAN (Controller Area Network) frames, with a pseudo-header as supplied by Linux SocketCAN.

#define LINKTYPE_IPV4 228
// DLT_IPV4 Raw IPv4; the packet begins with an IPv4 header.

#define LINKTYPE_IPV6 229
// DLT_IPV6 Raw IPv6; the packet begins with an IPv6 header.

#define LINKTYPE_IEEE802_15_4_NOFCS 230
// DLT_IEEE802_15_4_NOFCS IEEE 802.15.4 wireless Personal Area Network, without the FCS at the end of the frame.

#define LINKTYPE_DBUS 231
// DLT_DBUS Raw D-Bus messages, starting with the endianness flag, followed by the message type, etc., but without the authentication handshake before the message sequence.

#define LINKTYPE_DVB_CI 235
// DLT_DVB_CI DVB-CI (DVB Common Interface for communication between a PC Card module and a DVB receiver), with the message format specified by the PCAP format for DVB-CI specification.

#define LINKTYPE_MUX27010 236
// DLT_MUX27010 Variant of 3GPP TS 27.010 multiplexing protocol (similar to, but not the same as, 27.010).

#define LINKTYPE_STANAG_5066_D_PDU 237
// DLT_STANAG_5066_D_PDU D_PDUs as described by NATO standard STANAG 5066, starting with the synchronization sequence, and including both header and data CRCs. The current version of STANAG 5066 is backwards-compatible with the 1.0.2 version, although newer versions are classified.

#define LINKTYPE_NFLOG 239
// DLT_NFLOG Linux netlink NETLINK NFLOG socket log messages.

#define LINKTYPE_NETANALYZER 240
// DLT_NETANALYZER Pseudo-header for Hilscher Gesellschaft für Systemautomation mbH netANALYZER devices, followed by an Ethernet frame, beginning with the MAC header and ending with the FCS.

#define LINKTYPE_NETANALYZER_TRANSPARENT 241
// DLT_NETANALYZER_TRANSPARENT Pseudo-header for Hilscher Gesellschaft für Systemautomation mbH netANALYZER devices, followed by an Ethernet frame, beginning with the preamble, SFD, and MAC header, and ending with the FCS.

#define LINKTYPE_IPOIB 242
// DLT_IPOIB IP-over-InfiniBand, as specified by RFC 4391 section 6.

#define LINKTYPE_MPEG_2_TS 243
// DLT_MPEG_2_TS MPEG-2 Transport Stream transport packets, as specified by ISO 13818-1/ITU-T Recommendation H.222.0 (see table 2-2 of section 2.4.3.2 "Transport Stream packet layer").

#define LINKTYPE_NG40 244
// DLT_NG40 Pseudo-header for ng4T GmbH's UMTS Iub/Iur-over-ATM and Iub/Iur-over-IP format as used by their ng40 protocol tester, followed by frames for the Frame Protocol as specified by 3GPP TS 25.427 for dedicated channels and 3GPP TS 25.435 for common/shared channels in the case of ATM AAL2 or UDP traffic, by SSCOP packets as specified by ITU-T Recommendation Q.2110 for ATM AAL5 traffic, and by NBAP packets for SCTP traffic.

#define LINKTYPE_NFC_LLCP 245
// DLT_NFC_LLCP Pseudo-header for NFC LLCP packet captures, followed by frame data for the LLCP Protocol as specified by NFCForum-TS-LLCP_1.1.

#define LINKTYPE_INFINIBAND 247
// DLT_INFINIBAND Raw InfiniBand frames, starting with the Local Routing Header, as specified in Chapter 5 "Data packet format" of InfiniBand™ Architectural Specification Release 1.2.1 Volume 1 - General Specifications.

#define LINKTYPE_SCTP 248
// DLT_SCTP SCTP packets, as defined by RFC 4960, with no lower-level protocols such as IPv4 or IPv6.

#define LINKTYPE_USBPCAP 249
// DLT_USBPCAP USB packets, beginning with a USBPcap header.

#endif // PCAP_H
