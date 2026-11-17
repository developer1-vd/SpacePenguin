#include <QtTest>

#include "mousegestures.h"

using namespace spacepenguin;
using namespace spacepenguin::MouseGestures;

class TestMouseGestures : public QObject
{
    Q_OBJECT

private slots:
    void straightStrokes();
    void twoPartStrokes();
    void smallMovesAreNoGesture();
    void wobblesAreIgnored();
    void everyGestureIsDistinct();
};

static QList<QPoint> path(std::initializer_list<QPoint> corners, int step = 5)
{
    // The pointer events a real stroke through these corners would give.
    QList<QPoint> points;
    const QList<QPoint> list(corners);
    for (int i = 0; i + 1 < list.size(); ++i) {
        const QPoint from = list[i], to = list[i + 1];
        const int steps = qMax(1, int(QLineF(from, to).length()) / step);
        for (int s = 0; s < steps; ++s)
            points << from + (to - from) * s / steps;
    }
    points << list.last();
    return points;
}

void TestMouseGestures::straightStrokes()
{
    QCOMPARE(directions(path({{200, 200}, {100, 205}})), QStringLiteral("L"));
    QCOMPARE(directions(path({{200, 200}, {320, 190}})), QStringLiteral("R"));
    QCOMPARE(directions(path({{200, 200}, {205, 90}})), QStringLiteral("U"));
    QCOMPARE(directions(path({{200, 200}, {195, 300}})), QStringLiteral("D"));
    QCOMPARE(actionFor(QStringLiteral("L")), Action::Back);
    QCOMPARE(actionFor(QStringLiteral("R")), Action::Forward);
}

void TestMouseGestures::twoPartStrokes()
{
    QCOMPARE(directions(path({{200, 200}, {200, 100}, {200, 200}})), QStringLiteral("UD"));
    QCOMPARE(directions(path({{200, 200}, {200, 300}, {320, 300}})), QStringLiteral("DR"));
    QCOMPARE(actionFor(QStringLiteral("UD")), Action::Reload);
    QCOMPARE(actionFor(QStringLiteral("DR")), Action::CloseTab);
    QCOMPARE(actionFor(QStringLiteral("LRLR")), Action::None);
}

void TestMouseGestures::smallMovesAreNoGesture()
{
    QVERIFY(directions(path({{200, 200}, {210, 205}})).isEmpty());
    QVERIFY(directions({QPoint(1, 1)}).isEmpty());
    QCOMPARE(actionFor(QString()), Action::None);
}

void TestMouseGestures::wobblesAreIgnored()
{
    // A long stroke left that drifts a few pixels up and down is still just left.
    QCOMPARE(directions(path({{400, 200}, {350, 210}, {300, 195}, {250, 208}, {200, 200}})), QStringLiteral("L"));
}

void TestMouseGestures::everyGestureIsDistinct()
{
    QSet<QString> seen;
    for (const Entry &entry : all()) {
        QVERIFY(!seen.contains(entry.directions));
        seen.insert(entry.directions);
        QCOMPARE(actionFor(entry.directions), entry.action);
        QVERIFY(!label(entry.action).isEmpty());
        QCOMPARE(arrows(entry.directions).size(), entry.directions.size());
    }
}

QTEST_GUILESS_MAIN(TestMouseGestures)
#include "tst_mousegestures.moc"
