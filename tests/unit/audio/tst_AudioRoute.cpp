#include <auralis/audio/AudioRoute.h>

#include <QtTest>

using auralis::audio::AudioRoute;
using auralis::audio::RouteError;
using auralis::audio::RouteState;

class TstAudioRoute : public QObject {
    Q_OBJECT

private slots:
    void defaultStateIsInactive()
    {
        AudioRoute route;
        QVERIFY(route.id.isEmpty());
        QVERIFY(route.state == RouteState::Inactive);
        QVERIFY(!route.enabled);
        QVERIFY(!route.error.hasError());
        QCOMPARE(route.volume, 1.0);
        QVERIFY(route.ownerType == auralis::audio::RouteOwnerType::Manual);
    }

    void uniqueIdsAreDistinct()
    {
        AudioRoute a;
        a.id = QStringLiteral("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
        AudioRoute b;
        b.id = QStringLiteral("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");
        QVERIFY(a.id != b.id);
    }

    void emptyDestinationListIsInvalidForActivation()
    {
        AudioRoute route;
        route.id = QStringLiteral("r1");
        route.sourceId = QStringLiteral("src:1:Stream/Output/Audio");
        QVERIFY(route.destinationIds.isEmpty());
    }

    void duplicateDestinationsCanBeNormalized()
    {
        QStringList dests{QStringLiteral("pw:a"), QStringLiteral("pw:a"), QStringLiteral("pw:b")};
        dests.removeDuplicates();
        QCOMPARE(dests.size(), 2);
    }

    void structuredErrorCarriesCategoryAndDetail()
    {
        AudioRoute route;
        route.error = {RouteError::NoCompatiblePorts, QStringLiteral("no overlap")};
        QVERIFY(route.error.hasError());
        QCOMPARE(toString(route.error.category), QStringLiteral("NoCompatiblePorts"));
        QCOMPARE(route.error.detail, QStringLiteral("no overlap"));
    }

    void toStringCoversStates()
    {
        QCOMPARE(toString(RouteState::Activating), QStringLiteral("Activating"));
        QCOMPARE(toString(RouteState::Failed), QStringLiteral("Failed"));
        QCOMPARE(toString(RouteError::SourceNotFound), QStringLiteral("SourceNotFound"));
    }
};

QTEST_GUILESS_MAIN(TstAudioRoute)
#include "tst_AudioRoute.moc"
