#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <QProcess>

namespace spacepenguin {

/**
 * Read Aloud: text to speech with the voices the system already has.
 *
 * macOS speaks with `say`, Windows with its own speech engine (through
 * PowerShell's System.Speech), and Linux and the BSDs with speech-dispatcher
 * (`spd-say`, what desktop screen readers use) or eSpeak. Nothing is sent
 * anywhere: the text goes to a program on this computer on its standard input
 * (never on its command line, which other users can see), never through a shell.
 */
class ReadAloud : public QObject
{
    Q_OBJECT

public:
    enum class Engine { None, MacSay, WindowsSpeech, SpeechDispatcher, ESpeak };

    /** A speech program to run: what it is and its arguments. */
    struct Command {
        QString program;
        QStringList arguments;
        /** Given on standard input rather than as the last argument. */
        bool textOnStdin = false;
    };

    static ReadAloud *instance();

    /** The engine this system offers, None when it has none. */
    static Engine engine();
    static bool isAvailable() { return engine() != Engine::None; }
    /** "speech-dispatcher", "macOS speech"…, for the settings. */
    static QString engineName(Engine engine);

    /**
     * Pieces of at most @p limit characters, split at paragraph and then sentence
     * ends, so speech can stop between them and each fits on a command line.
     */
    static QStringList chunks(const QString &text, int limit = 2000);
    /**
     * How to speak with @p engine. @p rate is -5 (slow) to 5 (fast), 0 the voice's
     * own pace; @p language a BCP 47 tag ("en", "pt-BR") or empty for the default.
     */
    static Command command(Engine engine, int rate, const QString &language);

    /** Reads @p text aloud, stopping anything being read now. */
    void speak(const QString &text, const QString &language = QString());
    void stop();
    /** The pace for what is read next (see command()). */
    void setRate(int rate) { m_rate = rate; }
    bool isSpeaking() const { return !m_queue.isEmpty() || m_process; }

signals:
    void speakingChanged(bool speaking);

private:
    explicit ReadAloud(QObject *parent = nullptr);
    void speakNext();

    QStringList m_queue;
    QString m_language;
    int m_rate = 0;
    QPointer<QProcess> m_process;
};

}
