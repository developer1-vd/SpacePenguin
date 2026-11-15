#include <QtTest>

#include "settingsversion.h"

using namespace spacepenguin;

// Profiles move to the settings layout of each new release, step by step, and
// a newer profile is never touched by an older build.
class TestSettingsVersion : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("settings-%1.ini").arg(++m_count));
    }

    void freshProfileIsStamped()
    {
        QSettings settings(m_path, QSettings::IniFormat);
        QCOMPARE(SettingsVersion::upgrade(settings), SettingsVersion::Outcome::Upgraded);
        QCOMPARE(settings.value(SettingsVersion::kKey).toInt(), SettingsVersion::kCurrent);
        QCOMPARE(SettingsVersion::upgrade(settings), SettingsVersion::Outcome::UpToDate);
        QCOMPARE(SettingsVersion::steps().size(), SettingsVersion::kCurrent);
    }

    void stepsRunInOrderFromWhereTheProfileIs()
    {
        QStringList ran;
        const QList<SettingsVersion::Step> steps = {
            [&](QSettings &) { ran << QStringLiteral("0->1"); },
            [&](QSettings &s) {
                ran << QStringLiteral("1->2");
                s.setValue(QStringLiteral("new/key"), s.value(QStringLiteral("old/key")));
                s.remove(QStringLiteral("old/key"));
            },
            [&](QSettings &) { ran << QStringLiteral("2->3"); },
        };
        QSettings settings(m_path, QSettings::IniFormat);
        settings.setValue(SettingsVersion::kKey, 1);
        settings.setValue(QStringLiteral("old/key"), 42);
        QCOMPARE(SettingsVersion::upgrade(settings, steps, 3), SettingsVersion::Outcome::Upgraded);
        QCOMPARE(ran, (QStringList{QStringLiteral("1->2"), QStringLiteral("2->3")}));
        QCOMPARE(settings.value(QStringLiteral("new/key")).toInt(), 42);
        QVERIFY(!settings.contains(QStringLiteral("old/key")));
        QCOMPARE(settings.value(SettingsVersion::kKey).toInt(), 3);
    }

    void newerProfileIsLeftAlone()
    {
        bool ran = false;
        const QList<SettingsVersion::Step> steps = {[&](QSettings &) { ran = true; }};
        QSettings settings(m_path, QSettings::IniFormat);
        settings.setValue(SettingsVersion::kKey, 7);
        settings.setValue(QStringLiteral("future/key"), QStringLiteral("kept"));
        QCOMPARE(SettingsVersion::upgrade(settings, steps, 1), SettingsVersion::Outcome::Newer);
        QVERIFY(!ran);
        QCOMPARE(settings.value(SettingsVersion::kKey).toInt(), 7);
        QCOMPARE(settings.value(QStringLiteral("future/key")).toString(), QStringLiteral("kept"));
    }

    void garbageCountsAsUnversioned()
    {
        QSettings settings(m_path, QSettings::IniFormat);
        settings.setValue(SettingsVersion::kKey, QStringLiteral("banana"));
        QCOMPARE(SettingsVersion::upgrade(settings), SettingsVersion::Outcome::Upgraded);
        QCOMPARE(settings.value(SettingsVersion::kKey).toInt(), SettingsVersion::kCurrent);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_count = 0;
};

QTEST_MAIN(TestSettingsVersion)
#include "tst_settingsversion.moc"
