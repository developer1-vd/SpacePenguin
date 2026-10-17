#include <QtTest>

#include "shortcuts.h"

using namespace spacepenguin;

class TestShortcuts : public QObject
{
    Q_OBJECT

private slots:
    void everyCommandHasABinding();
    void noTwoCommandsShareABinding();
    void avoidsDesktopBindings();
};

static QList<Shortcuts::Command> allCommands()
{
    QList<Shortcuts::Command> commands;
    for (int command = Shortcuts::NewTab; command <= Shortcuts::KeyboardShortcuts; ++command)
        commands.append(static_cast<Shortcuts::Command>(command));
    return commands;
}

void TestShortcuts::everyCommandHasABinding()
{
    for (const Shortcuts::Command command : allCommands()) {
        QVERIFY2(!Shortcuts::keys(command).isEmpty(),
                 qPrintable(QStringLiteral("command %1 has no binding").arg(command)));
        QVERIFY(!Shortcuts::text(command).isEmpty());
    }
}

void TestShortcuts::noTwoCommandsShareABinding()
{
    // An ambiguous shortcut fires neither action, so a clash silently breaks both.
    QHash<QString, int> owner;
    for (const Shortcuts::Command command : allCommands()) {
        for (const QKeySequence &sequence : Shortcuts::keys(command)) {
            const QString key = sequence.toString(QKeySequence::PortableText);
            QVERIFY2(!owner.contains(key) || owner.value(key) == command,
                     qPrintable(QStringLiteral("%1 is bound to commands %2 and %3")
                                    .arg(key).arg(owner.value(key)).arg(command)));
            owner.insert(key, command);
        }
    }
}

void TestShortcuts::avoidsDesktopBindings()
{
    // Bindings the desktop itself owns would never reach the browser.
    QList<QKeySequence> reserved;
#ifdef Q_OS_MACOS
    reserved = {QKeySequence(Qt::CTRL | Qt::Key_H), QKeySequence(Qt::CTRL | Qt::Key_Tab),
                QKeySequence(Qt::CTRL | Qt::Key_M), QKeySequence(Qt::Key_F11)};
#else
    reserved = {QKeySequence(Qt::META | Qt::Key_Tab), QKeySequence(Qt::ALT | Qt::Key_Tab),
                QKeySequence(Qt::ALT | Qt::Key_F4)};
#endif
    for (const Shortcuts::Command command : allCommands()) {
        for (const QKeySequence &sequence : Shortcuts::keys(command))
            QVERIFY2(!reserved.contains(sequence),
                     qPrintable(sequence.toString(QKeySequence::PortableText)));
    }
}

QTEST_MAIN(TestShortcuts)
#include "tst_shortcuts.moc"
