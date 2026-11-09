#pragma once

#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;
class QTimer;
class QUdpSocket;
class QJsonObject;

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
    bool leave = false; // asks to leave the class, not to unblock a site
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
 * restricted. While in a class, the usual ways around its blocks are closed too
 * (see blocks()): Tor windows, the site inside other pages, behind view-source,
 * a translation proxy or an archive, its other addresses, and web proxies. A
 * student leaves when the teacher lets them (asked from Settings), closes the
 * class or makes a new code, or once the teacher's computer has been out of reach
 * for 14 days. It is a classroom tool, not device management: a browser installed
 * elsewhere is out of its reach.
 *
 * The teacher's browser serves a small JSON API over HTTP on port 47800 (or the
 * next free one) and answers discovery on UDP port 47801:
 *   POST /v2/class      {id, name}                   the list, this student's allowed sites and answers
 *   POST /v2/request    {id, name, site, note, kind}
 *
 * The class code is the shared secret, and it never crosses the network. Both
 * sides stretch it into a key (classKey(): PBKDF2, so a recorded exchange cannot
 * be tried against every code quickly) and sign with HMAC-SHA256:
 *   - discovery: "SPCLASS2?<nonce>?<mac>" proves the student knows the code; the
 *     teacher's "SPCLASS2!<port>!<mac>" proves it is the teacher;
 *   - each request carries X-SP-Time, X-SP-Nonce and X-SP-Mac over the method,
 *     path, time, nonce and body; the teacher refuses stale times and repeated
 *     nonces;
 *   - each answer carries X-SP-Mac over the request's nonce, its status and body,
 *     so a student takes blocks and answers from the teacher alone.
 * Anyone without the code can neither join nor pose as the teacher. Students
 * share the code, so one student could pose as another; it is a classroom tool
 * for a school network, and the README calls it a beta.
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
    /** In a class as a student: its blocks apply, and ways around them are closed. */
    bool locked() const;
    /** The teacher's computer has been out of reach so long that the student may leave alone. */
    bool canLeaveFreely() const;
    /** Asks the teacher to let this student leave. Answers with askFinished. */
    void askToLeave();
    QString joinedClassName() const;
    QString studentName() const;
    /** Joins the class: @p code, or code@address[:port]. Answers with joinFinished. */
    void join(const QString &code, const QString &name);
    void leave();
    /** Whether the teacher's computer answered lately. */
    bool teacherReachable() const { return m_reachable; }
    int joinedSiteCount() const;
    /**
     * Whether the class blocks @p url for this student; @p site and @p reason say why.
     * Covers the site's other addresses, the page wrapped by view-source:, a
     * translation proxy or an archive, and web proxy and mirror sites.
     */
    bool blocks(const QUrl &url, QString *site = nullptr, QString *reason = nullptr) const;
    /** Asks the teacher to unblock @p site. Answers with askFinished. */
    void ask(const QString &site, const QString &note);

    // ---- Shared, and exposed for testing ----------------------------------------
    /** Six letters and digits, without look-alikes (no 0/O, 1/I/L). */
    /** Ten letters and digits, without look-alikes (0/O, 1/I/L). */
    static QString makeCode();
    /** "ABCDE-FGH23", as people read and type it. */
    static QString displayCode(const QString &code);
    /** "ABCDE-FGH23", "abcdefgh23@10.0.0.5" or "…@10.0.0.5:47802" → code, host and port. */
    static bool parseJoinCode(const QString &text, QString *code, QString *host, quint16 *port);
    /** The page another address shows: view-source:, Google Translate and its .translate.goog
     *  proxy, the Wayback Machine, Google's cache. Invalid when @p url wraps nothing. */
    static QUrl unwrap(const QUrl &url);
    /** Other addresses of the same site: youtu.be for youtube.com, twitter.com for x.com. */
    static QStringList aliases(const QString &site);
    /** Web proxies, mirrors and paywall skippers, which get around any block. */
    static bool isBypassSite(const QUrl &url);
    /** The site of @p sites that covers @p url, if any, and not in @p allowed. */
    static const BlockedSite *match(const QUrl &url, const QList<BlockedSite> &sites,
                                    const QStringList &allowed = {});
    /** The page shown instead of a blocked site, with its reason and the Ask form. */
    static QString blockedPage(const QUrl &url, const QString &site, const QString &reason,
                               const QString &className);
    static QString askedPage(const QString &site, bool sent);
    /** One HTTP request read from @p data: complete, or more is needed (or it is bad). */
    enum class Parse { Complete, Incomplete, Bad };
    static Parse parseHttp(const QByteArray &data, QByteArray *method, QByteArray *target, QByteArray *body,
                           QHash<QByteArray, QByteArray> *headers = nullptr);

    /** The key both sides derive from a class code. Exposed for testing. */
    static QByteArray classKey(const QString &code);
    /** HMAC-SHA256 of @p message with @p key, in hex. */
    static QByteArray sign(const QByteArray &key, const QByteArray &message);

signals:
    void changed();
    /** Teacher: a student asked to unblock a site. */
    void requestReceived(const spacepenguin::UnblockRequest &request);
    /** Student: the teacher answered. */
    void answerReceived(const QString &site, bool allowed);
    void joinFinished(bool ok, const QString &message);
    void askFinished(bool ok);
    /** Student: out of the class, because the teacher let them go or ended it. */
    void released(const QString &message);

private:
    explicit Classroom(QObject *parent = nullptr);

    void refresh();
    void serve();
    void stopServing();
    void handleConnection();
    QByteArray respond(const QByteArray &method, const QByteArray &target, const QByteArray &body,
                       const QHash<QByteArray, QByteArray> &headers, int *status, QByteArray *mac);
    QByteArray respondSigned(const QByteArray &path, const QJsonObject &request, int *status);
    bool verifyRequest(const QByteArray &method, const QByteArray &path, const QByteArray &body,
                       const QHash<QByteArray, QByteArray> &headers, QByteArray *nonce);
    /** Student: a signed POST to the teacher; @p nonce is what the answer is signed over. */
    QNetworkReply *post(const QUrl &url, const QJsonObject &body, const QString &code, QByteArray *nonce);
    /** Student: whether @p reply is the teacher's own answer, and its body. */
    static bool verifiedAnswer(QNetworkReply *reply, const QString &code, const QByteArray &nonce,
                               QByteArray *body);
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
    QSet<QString> m_released; // students the teacher let leave, told at their next visit
    QHash<QByteArray, qint64> m_seenNonces; // requests already answered, against replays
    void postRequest(const QJsonObject &fields);
    void forceLeave(const QString &message);

    // Student.
    QTimer *m_poll = nullptr;
    bool m_reachable = false;
    int m_failures = 0;
    bool m_polling = false;
    QNetworkAccessManager *m_network = nullptr;
};

}

Q_DECLARE_METATYPE(spacepenguin::UnblockRequest)
