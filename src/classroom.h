#pragma once

#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QTcpServer;
class QTimer;
class QUdpSocket;

namespace spacepenguin {

struct BlockedSite {
    QString site;   // "youtube.com": the site and everything under it
    QString reason; // the teacher's words, shown to students on the blocked page
};

struct UnblockRequest {
    QString id;
    QString studentId;
    QString student;
    QString site;
    QString note;
    QDateTime when;
};

struct ClassStudent {
    QString id;
    QString name;
    QDateTime lastSeen;
};

/**
 * Teacher mode and student mode, on the school's network.
 *
 * A teacher opens a class: their SpacePenguin answers on the local network to a
 * short class code, with the sites blocked for the class (up to 100, each with
 * the teacher's reason). A student joins with the code and their name; their
 * SpacePenguin finds the teacher's on the network (or at code@address, where the
 * network keeps devices from finding each other), keeps the list up to date
 * every 15 seconds and keeps using the last one while the teacher is away.
 * Blocked pages show the teacher's reason and let the student ask to unblock the
 * site, with a note; the teacher allows it for that student or for everyone, or
 * declines, and the student hears back.
 *
 * Nothing goes through the internet, and the teacher's own browsing is not
 * restricted. It is a classroom tool, not device management: a student can leave
 * the class, and the teacher sees who is in it.
 *
 * The teacher's browser serves a small JSON API over HTTP on port 47800 (or the
 * next free one) and answers discovery on UDP port 47801:
 *   GET  /v1/class?code=…&id=…&name=…   the list, this student's allowed sites and answers
 *   POST /v1/request                     {code, id, name, site, note}
 */
class Classroom : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxSites = 100;
    static constexpr quint16 kHttpPort = 47800;
    static constexpr quint16 kDiscoveryPort = 47801;

    static Classroom *instance();

    /** Starts serving (teacher) or following (student) as the settings say; call once. */
    void start();

    // ---- Teacher ----------------------------------------------------------------
    bool classOpen() const;
    void setClassOpen(bool open);
    QString className() const;
    void setClassName(const QString &name);
    QString code() const;
    void newCode();
    QList<BlockedSite> blockedSites() const;
    /** Keeps the first kMaxSites of @p sites, cleaned up; returns how many were kept. */
    int setBlockedSites(const QList<BlockedSite> &sites);
    QList<UnblockRequest> requests() const { return m_requests; }
    /** Answers a request: allowed for that student, for everyone (unblocked), or declined. */
    enum class Answer { AllowStudent, AllowEveryone, Decline };
    void answer(const QString &requestId, Answer answer);
    QList<ClassStudent> students() const;
    /** Whether the class is being served right now, and where. */
    bool serving() const;
    quint16 port() const { return m_port; }
    /** This computer's addresses on the local network, for code@address. */
    static QStringList localAddresses();

    // ---- Student ----------------------------------------------------------------
    bool inClass() const;
    QString joinedClassName() const;
    QString studentName() const;
    /** Joins the class: @p code, or code@address[:port]. Answers with joinFinished. */
    void join(const QString &code, const QString &name);
    void leave();
    /** Whether the teacher's computer answered lately. */
    bool teacherReachable() const { return m_reachable; }
    int joinedSiteCount() const;
    /** Whether the class blocks @p url for this student; @p site and @p reason say why. */
    bool blocks(const QUrl &url, QString *site = nullptr, QString *reason = nullptr) const;
    /** Asks the teacher to unblock @p site. Answers with askFinished. */
    void ask(const QString &site, const QString &note);

    // ---- Shared, and exposed for testing ----------------------------------------
    /** Six letters and digits, without look-alikes (no 0/O, 1/I/L). */
    static QString makeCode();
    /** "ABC234", "abc234@10.0.0.5" or "ABC234@10.0.0.5:47802" → code, host and port. */
    static bool parseJoinCode(const QString &text, QString *code, QString *host, quint16 *port);
    /** The site of @p sites that covers @p url, if any, and not in @p allowed. */
    static const BlockedSite *match(const QUrl &url, const QList<BlockedSite> &sites,
                                    const QStringList &allowed = {});
    /** The page shown instead of a blocked site, with its reason and the Ask form. */
    static QString blockedPage(const QUrl &url, const QString &site, const QString &reason,
                               const QString &className);
    static QString askedPage(const QString &site, bool sent);
    /** One HTTP request read from @p data: complete, or more is needed (or it is bad). */
    enum class Parse { Complete, Incomplete, Bad };
    static Parse parseHttp(const QByteArray &data, QByteArray *method, QByteArray *target, QByteArray *body);

signals:
    void changed();
    /** Teacher: a student asked to unblock a site. */
    void requestReceived(const spacepenguin::UnblockRequest &request);
    /** Student: the teacher answered. */
    void answerReceived(const QString &site, bool allowed);
    void joinFinished(bool ok, const QString &message);
    void askFinished(bool ok);

private:
    explicit Classroom(QObject *parent = nullptr);

    void refresh();
    void serve();
    void stopServing();
    void handleConnection();
    QByteArray respond(const QByteArray &method, const QByteArray &target, const QByteArray &body,
                       const QHostAddress &peer, int *status);
    void handleDiscovery();

    void discover(const QString &code, std::function<void(QHostAddress, quint16)> found);
    void poll();
    void applyClass(const QByteArray &json);
    QUrl teacherUrl(const QString &path) const;

    QNetworkAccessManager *network();

    // Teacher.
    QTcpServer *m_server = nullptr;
    QUdpSocket *m_discovery = nullptr;
    quint16 m_port = 0;
    QList<UnblockRequest> m_requests;
    QHash<QString, ClassStudent> m_students;
    QHash<QString, QList<std::pair<QString, bool>>> m_answers; // per student, not yet delivered

    // Student.
    QTimer *m_poll = nullptr;
    bool m_reachable = false;
    int m_failures = 0;
    bool m_polling = false;
    QNetworkAccessManager *m_network = nullptr;
};

}

Q_DECLARE_METATYPE(spacepenguin::UnblockRequest)
