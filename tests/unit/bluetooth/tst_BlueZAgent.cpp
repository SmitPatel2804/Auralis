#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/PairingRequest.h>

#include "FakeBlueZClient.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BlueZAgent;
using auralis::bluetooth::PairingRequest;
using auralis::bluetooth::PairingRequestType;
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

        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.bluez"),
            QStringLiteral("/auralis/agent"),
            QStringLiteral("org.bluez.Agent1"),
            QStringLiteral("RequestAuthorization"));
        call.setDelayedReply(true);

        QVERIFY(call.isDelayedReply());

        agent.handleRequestAuthorization(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        QCOMPARE(pendingSpy.count(), 1);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);
        QVERIFY(request->requestType() == PairingRequestType::AuthorizePairing);

        agent.acceptPairingRequest(request->requestId());
        QVERIFY(agent.pendingRequest() == nullptr);
    }

    void rejectClearsPendingRequest()
    {
        FakeBlueZClient client;
        BlueZAgent agent(&client);
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.bluez"),
            QStringLiteral("/auralis/agent"),
            QStringLiteral("org.bluez.Agent1"),
            QStringLiteral("RequestConfirmation"));
        call.setDelayedReply(true);

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
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.bluez"),
            QStringLiteral("/auralis/agent"),
            QStringLiteral("org.bluez.Agent1"),
            QStringLiteral("RequestPinCode"));
        call.setDelayedReply(true);

        agent.handleRequestPinCode(QStringLiteral("/org/bluez/hci0/dev_AA"), call);
        PairingRequest* request = agent.pendingRequest();
        QVERIFY(request != nullptr);
        agent.acceptPairingRequest(request->requestId());
        QVERIFY(agent.pendingRequest() != nullptr);
        agent.submitPinCode(request->requestId(), QStringLiteral("1234"));
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
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.bluez"),
            QStringLiteral("/auralis/agent"),
            QStringLiteral("org.bluez.Agent1"),
            QStringLiteral("DisplayPasskey"));
        call.setDelayedReply(true);

        agent.handleDisplayPasskey(QStringLiteral("/org/bluez/hci0/dev_AA"), 654321, 0, call);
        QVERIFY(agent.pendingRequest() != nullptr);
        agent.handleCancel();
        QVERIFY(agent.pendingRequest() == nullptr);
    }
};

QTEST_GUILESS_MAIN(TstBlueZAgent)
#include "tst_BlueZAgent.moc"
