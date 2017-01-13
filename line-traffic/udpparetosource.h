#ifndef UDPPARETOSOURCE_H
#define UDPPARETOSOURCE_H

#include "evudp.h"
#include "udpcbr.h"
#include "pareto.h"

class CBRParetoSourceArg {
public:
	ParetoSourceArg pareto;
	UDPCBRSourceArg cbr;
};

class UDPParetoSource : public UDPClient {
public:
	UDPParetoSource(int fd, ParetoSourceArg pareto, UDPCBRSourceArg cbr);
	static UDPClient* makeUDPParetoSource(int fd, void *arg);
	virtual void onWrite();
	virtual void onStop();

	UDPCBRSourceArg cbr;
	quint64 transferSize;
};

#endif // UDPPARETOSOURCE_H
