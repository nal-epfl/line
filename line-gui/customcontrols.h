#ifndef CUSTOMCONTROLS_H
#define CUSTOMCONTROLS_H

#include <QWidget>

#include "mainwindow.h"
#include "util.h"

typedef QQuad<QString, qreal, QString, QString> QStringRealStringStringTuple;
typedef QQuad<qreal, QString, qreal, QString> QRealStringRealStringTuple;
typedef QQuint<QString, qreal, qreal, qreal, qreal> QStringRealRealRealRealTuple;


namespace Ui {
class CustomControls;
}

class MainWindow;

class CustomControls : public QWidget
{
    Q_OBJECT

public:
    explicit CustomControls(QWidget *parent = 0);
    ~CustomControls();

protected slots:
    void autoPolicingSetupDefaults(bool shaping);
    void autoPolicingGenerateSuffix();
    void on_btnAutoPolicingPlot1_clicked();
    void on_btnAutoPolicingPlot1b_clicked();
    void on_btnAutoPolicingPlot1c_clicked();
    void on_btnAutoPolicingPlot7_clicked();
    void on_btnAutoPolicingPlot7b_clicked();
    void on_btnAutoPolicingPlot2_clicked();
    void on_btnAutoPolicingPlot2b_clicked();
    void autoPolicingPlot2common();
    void on_btnAuto2All_clicked();
    void on_btnAutoPolicingPlot3_clicked();
    void on_btnAutoPolicingPlot4_clicked();
private slots:
    void autoReset(int durationSec);
    void setNeutral();
    void setPolicing(qreal weight1, qreal weight2);
    void setShaping(qreal weight1, qreal weight2);
    void setRtt(qint32 rtt1, qint32 rtt2);
    void setTcp(QString tcp1, QString tcp2);
    void setBuffer(QString bufferSize, qreal bufferScaling, QString bufferQoSScaling);
    void setQDisc(QString qdisc);
    void setTransferSize(qreal size1, qreal size2, QString congestion1, QString congestion2, QString buffers, qreal bufferScaling);
    QString autoGenerateSuffix(QString hidden,
                               QString bufferSize, qreal bufferScaling, QString bufferQoSScaling,
                               QString qdisc,
                               qint32 rtt1, qint32 rtt2,
                               QString tcp1, QString tcp2,
                               qreal size1, qreal size2,
                               QString congestion1, QString congestion2);
    void autoGenerate(QList<QStringPair> tcpValues,
                      QList<QStringRealStringStringTuple> buffersScalingQosScalingQdiscValues,
                      QList<QInt32Pair> rttValues,
                      QList<QRealStringRealStringTuple> sizeCongestionValues,
                      QList<QStringRealRealRealRealTuple> qosValues,
                      QString plotName,
                      QString xAxis,
                      int durationSec = 600);
    void autoABase(QList<QStringRealRealRealRealTuple> &qosValues,
                   QList<QStringPair> &tcpValues,
                   QList<QStringRealStringStringTuple> &buffersScalingQosScalingQdiscValues);
    void autoBBase(QList<QStringRealRealRealRealTuple> &qosValues,
                   QList<QStringPair> &tcpValues,
                   QList<QStringRealStringStringTuple> &buffersScalingQosScalingQdiscValues);
    void autoCBase(QList<QStringRealRealRealRealTuple> &qosValues,
                   QList<QStringPair> &tcpValues,
                   QList<QStringRealStringStringTuple> &buffersScalingQosScalingQdiscValues);

    void on_btnAutoAaNeutralSizeGap_clicked();
    void autoBCaQosSize(bool shaping);
    void on_btnAutoBaPolicingSize_clicked();
    void on_btnAutoCaShapingSize_clicked();

    void on_btnAutoAbNeutralRttGap_clicked();
    void autoBCbQosRtt(bool shaping);
    void on_btnAutoBbPolicingRtt_clicked();
    void on_btnAutoCbShapingRtt_clicked();

    void on_btnAutoAcNeutralCongestion_clicked();
    void on_btnAutoAcNeutralCongestionGap_clicked();
    void autoBCcQosCongestion(bool shaping);
    void on_btnAutoBcPolicingCongestion_clicked();
    void on_btnAutoCcShapingCongestion_clicked();

    void on_btnAutoAdNeutralTcp_clicked();
    void autoBCdQoSQoS(bool shaping);
    void on_btnAutoBdPolicingQoS_clicked();
    void on_btnAutoCdShapingQoS_clicked();

    void on_btnAutoCalibrateNone_clicked();

    void createConnection(NetGraph *g, qint32 n1, qint32 n2, QString traffic, int trafficClass);
    void autoD(QString traffic1,
               QString traffic2,
               QString background1,
               QString background2,
               QInt32List nonNeutralLinks);
    void autoDGenerate(QList<QStringQuad> trafficValues,
                       QList<QInt32List> nonNeutralLinks,
                       QString plotSuffix);

    void on_btnAutoDaFile_clicked();
    void on_btnAutoDbWeb_clicked();
    void on_btnAutoDcWebFile_clicked();
    void on_btnAutoDdWebVideo_clicked();

	void on_btnSigcomm7a_clicked();
	void loadGraph(QString name);

	void on_btnSigcomm7b_clicked();

	void on_btnSigcomm7c_clicked();

	void on_btnSigcomm7d_clicked();

	void on_btnSigcomm7e_clicked();

	void on_btnSigcomm7f_clicked();

	void on_btnSigcomm8ab_clicked();

private:
    Ui::CustomControls *ui;
    MainWindow *mainWindow;

    friend class MainWindow;
};

#endif // CUSTOMCONTROLS_H
