#include "readaloud.h"

#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace spacepenguin {

ReadAloud *ReadAloud::instance()
{
    static ReadAloud *readAloud = new ReadAloud(qApp);
    return readAloud;
}

ReadAloud::ReadAloud(QObject *parent)
    : QObject(parent)
{
}

ReadAloud::Engine ReadAloud::engine()
{
#if defined(Q_OS_MACOS)
    return QStandardPaths::findExecutable(QStringLiteral("say")).isEmpty() ? Engine::None : Engine::MacSay;
#elif defined(Q_OS_WIN)
    return QStandardPaths::findExecutable(QStringLiteral("powershell")).isEmpty() ? Engine::None
                                                                                    : Engine::WindowsSpeech;
#elif defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return Engine::None;
#else
    if (!QStandardPaths::findExecutable(QStringLiteral("spd-say")).isEmpty())
        return Engine::SpeechDispatcher;
    if (!QStandardPaths::findExecutable(QStringLiteral("espeak-ng")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("espeak")).isEmpty())
        return Engine::ESpeak;
    return Engine::None;
#endif
}

QString ReadAloud::engineName(Engine engine)
{
    switch (engine) {
    case Engine::None: return QString();
    case Engine::MacSay: return QCoreApplication::translate("ReadAloud", "macOS speech");
    case Engine::WindowsSpeech: return QCoreApplication::translate("ReadAloud", "Windows speech");
    case Engine::SpeechDispatcher: return QStringLiteral("speech-dispatcher");
    case Engine::ESpeak: return QStringLiteral("eSpeak");
    }
    return QString();
}

QStringList ReadAloud::chunks(const QString &text, int limit)
{
    QStringList pieces;
    const auto add = [&pieces](QString piece) {
        piece = piece.simplified();
        if (!piece.isEmpty())
            pieces << piece;
    };
    static const QRegularExpression paragraphBreak(QStringLiteral("\\n\\s*\\n|\\r\\n\\s*\\r\\n"));
    static const QRegularExpression sentenceEnd(QStringLiteral("(?<=[.!?。！？])\\s+"));
    for (const QString &paragraph : text.split(paragraphBreak, Qt::SkipEmptyParts)) {
        if (paragraph.size() <= limit) {
            add(paragraph);
            continue;
        }
        QString current;
        for (const QString &sentence : paragraph.split(sentenceEnd, Qt::SkipEmptyParts)) {
            if (!current.isEmpty() && current.size() + 1 + sentence.size() > limit) {
                add(current);
                current.clear();
            }
            // One sentence longer than the limit is cut where it must be.
            QString rest = sentence;
            while (rest.size() > limit) {
                int cut = rest.lastIndexOf(QLatin1Char(' '), limit);
                if (cut <= 0)
                    cut = limit;
                add(rest.left(cut));
                rest = rest.mid(cut);
            }
            current += (current.isEmpty() ? QString() : QStringLiteral(" ")) + rest;
        }
        add(current);
    }
    return pieces;
}

ReadAloud::Command ReadAloud::command(Engine engine, int rate, const QString &language)
{
    rate = qBound(-5, rate, 5);
    // Only letters, digits and hyphens: the tag ends up inside a PowerShell script.
    QString tag = language;
    tag.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9-]")));
    tag = tag.left(16);
    Command command;
    switch (engine) {
    case Engine::None:
        break;
    case Engine::MacSay:
        command.program = QStringLiteral("say");
        command.arguments = {QStringLiteral("-r"), QString::number(175 + rate * 25), QStringLiteral("-f"),
                             QStringLiteral("-")};
        command.textOnStdin = true;
        break;
    case Engine::WindowsSpeech: {
        QString script = QStringLiteral(
            "[Console]::InputEncoding = [Text.Encoding]::UTF8; Add-Type -AssemblyName System.Speech; "
            "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Rate = %1; ")
                             .arg(rate * 2);
        if (!tag.isEmpty())
            script += QStringLiteral("$v = $s.GetInstalledVoices() | Where-Object { $_.Enabled -and "
                                     "$_.VoiceInfo.Culture.Name -like '%1*' } | Select-Object -First 1; "
                                     "if ($v) { $s.SelectVoice($v.VoiceInfo.Name) }; ")
                          .arg(tag);
        script += QStringLiteral("$s.Speak([Console]::In.ReadToEnd())");
        command.program = QStringLiteral("powershell");
        command.arguments = {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                             QStringLiteral("-Command"), script};
        command.textOnStdin = true;
        break;
    }
    case Engine::SpeechDispatcher:
        command.program = QStringLiteral("spd-say");
        // -w: return when done, so the next piece follows this one. -e: the text
        // on standard input, as command lines are visible to every user (ps).
        command.arguments = {QStringLiteral("-w"), QStringLiteral("-e"), QStringLiteral("-r"), QString::number(rate * 20)};
        if (!tag.isEmpty())
            command.arguments << QStringLiteral("-l") << tag.section(QLatin1Char('-'), 0, 0).toLower();
        command.textOnStdin = true;
        break;
    case Engine::ESpeak:
        command.program = QStandardPaths::findExecutable(QStringLiteral("espeak-ng")).isEmpty()
            ? QStringLiteral("espeak")
            : QStringLiteral("espeak-ng");
        command.arguments = {QStringLiteral("-s"), QString::number(175 + rate * 25)};
        if (!tag.isEmpty())
            command.arguments << QStringLiteral("-v") << tag.toLower();
        command.arguments << QStringLiteral("--stdin");
        command.textOnStdin = true;
        break;
    }
    return command;
}

void ReadAloud::speak(const QString &text, const QString &language)
{
    stop();
    m_queue = chunks(text);
    m_language = language;
    if (m_queue.isEmpty())
        return;
    emit speakingChanged(true);
    speakNext();
}

void ReadAloud::stop()
{
    const bool was = isSpeaking();
    m_queue.clear();
    if (QProcess *process = m_process) {
        m_process = nullptr;
        process->disconnect(this);
        process->kill();
        process->waitForFinished(1000);
        process->deleteLater();
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)
        // spd-say only hands the text to the speech-dispatcher service, which
        // keeps talking; this tells it to stop.
        if (engine() == Engine::SpeechDispatcher)
            QProcess::startDetached(QStringLiteral("spd-say"), {QStringLiteral("-C")});
#endif
    }
    if (was)
        emit speakingChanged(false);
}

void ReadAloud::speakNext()
{
    if (m_queue.isEmpty()) {
        emit speakingChanged(false);
        return;
    }
    const QString piece = m_queue.takeFirst();
    const Command command = ReadAloud::command(engine(), m_rate, m_language);
    if (command.program.isEmpty()) {
        m_queue.clear();
        emit speakingChanged(false);
        return;
    }
    auto *process = new QProcess(this);
    m_process = process;
    connect(process, &QProcess::finished, this, [this, process] {
        if (process != m_process)
            return;
        m_process = nullptr;
        process->deleteLater();
        speakNext();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || process != m_process)
            return;
        m_process = nullptr;
        process->deleteLater();
        m_queue.clear();
        emit speakingChanged(false);
    });
    process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process->setStandardOutputFile(QProcess::nullDevice());
    QStringList arguments = command.arguments;
    if (!command.textOnStdin)
        arguments << piece;
    process->start(command.program, arguments);
    if (command.textOnStdin) {
        process->write(piece.toUtf8());
        process->closeWriteChannel();
    }
}

}
