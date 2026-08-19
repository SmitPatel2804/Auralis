#include <auralis/bluetooth/AgentCapability.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/PairingRequest.h>

#include "FakeBlueZClient.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BlueZAgent;
using auralis::bluetooth::AgentCapability;
using auralis::bluetooth::PairingRequest;
using auralis::bluetooth::PairingRequestType;
using auralis::bluetooth::parseAgentCapability;
using auralis::bluetooth::toBlueZCapability;
using auralis::test::FakeBlueZClient;

class TstBlueZAgent : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QDBusMessage>("QDBusMessage");
    }

    void authorizationRequestAcceptCompletes()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QSignalSpy pendingSpy(&agent, &BlueZAgent::pendingRequestChanged);

        QDBusMessage call;

        agent.handleRequestAuthorization(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        QCOMPARE(pendingSpy.count(), 1);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);
        QVERIFY(request->requestType() == PairingRequestType::AuthorizePairing);

        agent.acceptPairingRequest(request->requestId());
        QVERIFY(agent.pendingRequest() == nullptr);
    }

    void capabilityMappingCoversAllValues()
    {
        QCOMPARE(toBlueZCapability(AgentCapability::NoInputNoOutput), u"NoInputNoOutput");
        QCOMPARE(toBlueZCapability(AgentCapability::DisplayOnly), u"DisplayOnly");
        QCOMPARE(toBlueZCapability(AgentCapability::DisplayYesNo), u"DisplayYesNo");
        QCOMPARE(toBlueZCapability(AgentCapability::KeyboardOnly), u"KeyboardOnly");
        QCOMPARE(toBlueZCapability(AgentCapability::KeyboardDisplay), u"KeyboardDisplay");

        QCOMPARE(parseAgentCapability(QStringLiteral("NoInputNoOutput")), AgentCapability::NoInputNoOutput);
        QCOMPARE(parseAgentCapability(QStringLiteral("DisplayOnly")), AgentCapability::DisplayOnly);
        QCOMPARE(parseAgentCapability(QStringLiteral("DisplayYesNo")), AgentCapability::DisplayYesNo);
        QCOMPARE(parseAgentCapability(QStringLiteral("KeyboardOnly")), AgentCapability::KeyboardOnly);
        QCOMPARE(parseAgentCapability(QStringLiteral("KeyboardDisplay")), AgentCapability::KeyboardDisplay);
        QCOMPARE(parseAgentCapability(QStringLiteral("invalid-value")), AgentCapability::KeyboardDisplay);
    }

    void initializeRegistersConfiguredCapability()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client, AgentCapability::DisplayYesNo);

        if (!agent.initialize()) {
            QSKIP("System D-Bus Agent1 export is unavailable in this test environment");
        }
        QCOMPARE(client.registerAgentRequests(), 1);
        QCOMPARE(client.requestDefaultAgentRequests(), 1);
        QCOMPARE(client.lastRegisterAgentCapability(), QStringLiteral("DisplayYesNo"));
        QCOMPARE(agent.capability(), AgentCapability::DisplayYesNo);
    }

    void rejectClearsPendingRequest()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call;

        agent.handleRequestConfirmation(QStringLiteral("/org/bluez/hci0/dev_AA"), 123456, call);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);
        agent.rejectPairingRequest(request->requestId());
        QVERIFY(agent.pendingRequest() == nullptr);
    }

    void submitPinOnlyForEnterPin()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call;

        agent.handleRequestPinCode(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);
        agent.acceptPairingRequest(request->requestId());
        QVERIFY(agent.pendingRequest() != nullptr);
        agent.submitPinCode(request->requestId(), QStringLiteral("1234"));
        QVERIFY(agent.pendingRequest() == nullptr);
    }

    void invalidPinKeepsRequestPending()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call;

        agent.handleRequestPinCode(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);

        agent.submitPinCode(request->requestId(), QString());
        QVERIFY(agent.pendingRequest() != nullptr);
    }

    void invalidPasskeyKeepsRequestPending()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call;

        agent.handleRequestPasskey(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);

        agent.submitPasskey(request->requestId(), 1000000U);
        QVERIFY(agent.pendingRequest() != nullptr);
    }

    void duplicateResponseIsIgnored()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call;

        agent.handleRequestPinCode(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);

        agent.submitPinCode(request->requestId(), QStringLiteral("1234"));
        QVERIFY(agent.pendingRequest() == nullptr);

        agent.submitPinCode(request->requestId(), QStringLiteral("5678"));
        QVERIFY(agent.pendingRequest() == nullptr);
    }

    void expiredRequestIsIgnored()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        agent.acceptPairingRequest(QStringLiteral("missing-id"));
        agent.rejectPairingRequest(QStringLiteral("missing-id"));
        agent.submitPinCode(QStringLiteral("missing-id"), QStringLiteral("0000"));
    }

    void cancelInvalidatesPending()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call;

        agent.handleDisplayPasskey(QStringLiteral("/org/bluez/hci0/dev_AA"), 654321, 0, call);
        QVERIFY(agent.pendingRequest() != nullptr);
        agent.handleCancel();
        QVERIFY(agent.pendingRequest() == nullptr);
    }
};

QTEST_GUILESS_MAIN(TstBlueZAgent)
#include "tst_BlueZAgent.moc"
