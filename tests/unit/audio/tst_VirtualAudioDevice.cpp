#include <auralis/audio/VirtualAudioDevice.h>

#include <QTest>

using namespace auralis::audio;

class TstVirtualAudioDevice final : public QObject {
    Q_OBJECT

private slots:
    void recognizesExpectedEndpointNames()
    {
        QVERIFY(isAuralisVirtualRender(QStringLiteral("Auralis Virtual Output")));
        QVERIFY(isAuralisVirtualRender(QStringLiteral("Speakers (Auralis Virtual Speaker)")));
        QVERIFY(isAuralisVirtualMonitor(QStringLiteral("Auralis Virtual Monitor")));
        QVERIFY(isAuralisVirtualMonitor(QStringLiteral("Microphone (Auralis Virtual Capture)")));
        QVERIFY(!isAuralisVirtualRender(QStringLiteral("Smokin' Buds")));
        QVERIFY(!isAuralisVirtualMonitor(QStringLiteral("Built-in Microphone")));
    }

    void renderEndpointIsSufficientAndTracksWindowsDefault()
    {
        const VirtualAudioDeviceState ready = inspectVirtualAudioDevices(
            {QStringLiteral("Built-in Speakers"), QStringLiteral("Auralis Virtual Output")},
            {QStringLiteral("Auralis Virtual Monitor")},
            QStringLiteral("Auralis Virtual Output"));
        QVERIFY(ready.ready());
        QVERIFY(ready.selectedAsDefault);

        const VirtualAudioDeviceState incomplete = inspectVirtualAudioDevices(
            {QStringLiteral("Auralis Virtual Output")},
            {QStringLiteral("Built-in Microphone")},
            QStringLiteral("Built-in Speakers"));
        QVERIFY(incomplete.renderAvailable);
        QVERIFY(!incomplete.monitorAvailable);
        QVERIFY(incomplete.ready());
        QVERIFY(!incomplete.selectedAsDefault);
    }
};

QTEST_GUILESS_MAIN(TstVirtualAudioDevice)
#include "tst_VirtualAudioDevice.moc"
