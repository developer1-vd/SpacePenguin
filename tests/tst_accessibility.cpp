#include <QtTest>

#include <QAction>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

#include "accessibility.h"

using namespace spacepenguin;

// Screen readers need a name for every control; icon-only buttons have none of
// their own, so they take their tooltip's.
class TestAccessibility : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { AccessibleNames::install(); }

    void iconButtonTakesItsTooltip()
    {
        QToolButton button;
        button.setToolTip(QStringLiteral("Close tab"));
        button.show();
        QVERIFY(QTest::qWaitForWindowExposed(&button));
        QCOMPARE(button.accessibleName(), QStringLiteral("Close tab"));
        // And follows it.
        button.setToolTip(QStringLiteral("Close <b>this</b> tab"));
        QCOMPARE(button.accessibleName(), QStringLiteral("Close this tab"));
    }

    void actionButtonTakesItsAction()
    {
        QAction back(QStringLiteral("&Back"));
        QToolButton button;
        button.setDefaultAction(&back);
        button.setToolButtonStyle(Qt::ToolButtonIconOnly);
        button.show();
        QCOMPARE(button.accessibleName(), QStringLiteral("Back"));
        // Reload turning into Stop: the button follows its new action.
        QAction stop(QStringLiteral("Stop"));
        button.setDefaultAction(&stop);
        QCOMPARE(button.accessibleName(), QStringLiteral("Stop"));
    }

    void namesInTheCodeStay()
    {
        QToolButton button;
        button.setAccessibleName(QStringLiteral("Main menu"));
        button.setToolTip(QStringLiteral("Menu"));
        button.show();
        QCOMPARE(button.accessibleName(), QStringLiteral("Main menu"));
    }

    void wordsNeedNoName()
    {
        QPushButton button(QStringLiteral("Save"));
        button.setToolTip(QStringLiteral("Save the file"));
        button.show();
        QVERIFY(button.accessibleName().isEmpty()); // read out as "Save"
        QCOMPARE(AccessibleNames::fallbackName(&button), QString());
    }

    void fieldsTakeTheirTooltip()
    {
        QLineEdit field;
        field.setToolTip(QStringLiteral("Search history"));
        field.show();
        QCOMPARE(field.accessibleName(), QStringLiteral("Search history"));
    }

    void fieldsTakeTheirHint()
    {
        QLineEdit field;
        field.setPlaceholderText(QStringLiteral("Find in page"));
        field.setClearButtonEnabled(true);
        field.setText(QStringLiteral("penguin"));
        field.show();
        QCOMPARE(field.accessibleName(), QStringLiteral("Find in page"));
        for (QWidget *child : field.findChildren<QWidget *>()) {
            if (child->inherits("QLineEditIconButton") && child->isVisible())
                QCOMPARE(child->accessibleName(), QStringLiteral("Clear text"));
        }
    }
};

QTEST_MAIN(TestAccessibility)
#include "tst_accessibility.moc"
