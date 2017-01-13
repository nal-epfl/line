#include "udpparetosource.h"

#include "util.h"
#include "chronometer.h"

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0

UDPParetoSource::UDPParetoSource(int fd, ParetoSourceArg pareto, UDPCBRSourceArg cbr)
	: UDPClient(fd),
	  cbr(cbr)
{
	transferSize = pareto.generateSize();
	qDebugT() << "Pareto transfer size (bytes): " << transferSize;
}

UDPClient *UDPParetoSource::makeUDPParetoSource(int fd, void *arg)
{
	Q_ASSERT(arg);
	CBRParetoSourceArg *params = (CBRParetoSourceArg*)arg;
	return new UDPParetoSource(fd, params->pareto, params->cbr);
}

void UDPParetoSource::onWrite()
{
	UDPClient::onWrite();

	if (getTotalBytesWritten() >= transferSize) {
		qDebugT() << "UDPParetoSource finished";
		udp_deferred_close_client(m_fd);
		return;
	}

	if (cbr.shouldSend(totalWritten, (getCurrentTimeNanosec() - tConnect) * 1.0e-9)) {
		QByteArray b;
		quint64 left = transferSize - getTotalBytesWritten();
		b.fill('z', qMin(left, (quint64)cbr.frameSize));
		write(b);
	}
}

void UDPParetoSource::onStop()
{
	UDPClient::onStop();
	printUploadStats();
}
