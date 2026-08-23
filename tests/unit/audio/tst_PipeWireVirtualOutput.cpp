#include "AudioTestFixtures.h"

#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireVirtualOutput.h>

#include <QFile>
#include <QtTest>

using auralis::audio::PipeWireObjectStore;
using auralis::audio::inspectPipeWireVirtualOutput;
using auralis::audio::pipeWireDefaultNodeName;
using auralis::audio::pipeWireVirtualOutputModuleArguments;
using auralis::test::makeNode;
using auralis::test::makePort;

class TstPipeWireVirtualOutput : public QObject {
    Q_OBJECT

private slots:
    void parsesDefaultSinkMetadata()
    {
        QCOMPARE(
            pipeWireDefaultNodeName(QStringLiteral(R"({"name":"auralis_virtual_output"})")),
            QStringLiteral("auralis_virtual_output"));
        QVERIFY(pipeWireDefaultNodeName(QStringLiteral("not-json")).isEmpty());
        QVERIFY(pipeWireDefaultNodeName(QStringLiteral(R"({"id":42})")).isEmpty());
    }

    void completePackageGraphIsReadyAndSelected()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(
            20,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("sink")},
             {QStringLiteral("auralis.virtual.persistence"), QStringLiteral("package")}}));
        store.upsert(makePort(200, 20, QStringLiteral("in")));
        store.upsert(makeNode(
            21,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output.source")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("source")},
             {QStringLiteral("auralis.virtual.persistence"), QStringLiteral("package")}}));
        store.upsert(makePort(210, 21, QStringLiteral("out")));

        const auto state = inspectPipeWireVirtualOutput(store, QStringLiteral("auralis_virtual_output"));
        QVERIFY(state.ready());
        QVERIFY(state.packageManaged);
        QVERIFY(!state.runtimeManaged);
        QVERIFY(state.selectedAsDefault);
        QCOMPARE(state.sinkNodeId, static_cast<quint32>(20));
        QCOMPARE(state.sourceNodeId, static_cast<quint32>(21));
    }

    void nodeWithoutUsablePortsIsPartial()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(
            30,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output")}}));
        const auto state = inspectPipeWireVirtualOutput(store);
        QVERIFY(state.partial());
        QVERIFY(!state.ready());
    }

    void runtimeAndPackagedDefinitionsShareStableIdentity()
    {
        const QByteArray runtime = pipeWireVirtualOutputModuleArguments();
        QVERIFY(runtime.contains("auralis_virtual_output"));
        QVERIFY(runtime.contains("auralis_virtual_output.source"));
        QVERIFY(runtime.contains("auralis.virtual.role = \"sink\""));
        QVERIFY(runtime.contains("auralis.virtual.role = \"source\""));
        QVERIFY(runtime.contains("auralis.virtual.persistence = \"runtime\""));
        QVERIFY(runtime.contains("node.autoconnect = false"));
        QVERIFY(runtime.contains("priority.session = 100"));
        QVERIFY(runtime.contains("session.suspend-timeout-seconds = 0"));
        QVERIFY(runtime.contains("node.virtual = false"));

        QFile packaged(QStringLiteral(AURALIS_PIPEWIRE_CONFIG_PATH));
        QVERIFY2(packaged.open(QIODevice::ReadOnly), qPrintable(packaged.errorString()));
        const QByteArray config = packaged.readAll();
        QVERIFY(config.contains("auralis_virtual_output"));
        QVERIFY(config.contains("auralis_virtual_output.source"));
        QVERIFY(config.contains("auralis.virtual.persistence = \"package\""));
        QVERIFY(config.contains("flags = [ ifexists nofail ]"));
        QVERIFY(config.contains("priority.session = 100"));
    }

    void sessionFanoutMatchesExactSinksAndCompensatesLatency()
    {
        const QByteArray args = auralis::audio::pipeWireSessionFanoutModuleArguments(
            {QStringLiteral("bluez_output.one"), QStringLiteral("bluez_output.two")});
        QVERIFY(args.contains("combine.latency-compensate = true"));
        QVERIFY(args.contains("auralis_session_fanout"));
        QVERIFY(args.contains("node.name = \"bluez_output.one\""));
        QVERIFY(args.contains("node.name = \"bluez_output.two\""));
        QVERIFY(args.contains("auralis.session.fanout = true"));
    }

    void delayBridgeUsesLoopbackTargetDelay()
    {
        const QByteArray args = auralis::audio::pipeWireDelayBridgeModuleArguments(
            QStringLiteral("dest-a"), QStringLiteral("bluez_output.one"), 0.08);
        QVERIFY(args.contains("target.delay.sec = 0.0800"));
        QVERIFY(args.contains("auralis.delay.bridge = true"));
        QVERIFY(args.contains("target.object = \"bluez_output.one\""));
        QCOMPARE(auralis::audio::auralisDelayBridgeCaptureNodeName(QStringLiteral("dest-a")),
                 QStringLiteral("auralis_delay_dest_a"));
    }
};

QTEST_GUILESS_MAIN(TstPipeWireVirtualOutput)
#include "tst_PipeWireVirtualOutput.moc"
