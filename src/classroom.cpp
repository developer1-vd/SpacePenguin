#include "classroom.h"

#include "preferences.h"
#include "roles.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QUrlQuery>
#include <QUuid>

#include <algorithm>
#include <memory>

namespace spacepenguin {
namespace {

const QString kTeacher = QStringLiteral("classroom/teacher/");
const QString kStudent = QStringLiteral("classroom/student/");
constexpr int kPollMs = 15000;
constexpr int kMaxRequestBytes = 32 * 1024;
constexpr int kMaxPendingPerStudent = 10;
constexpr int kMaxNote = 300;
constexpr int kMaxName = 40;

bool hasRole(Roles::Role role)
{
    return Preferences::instance()->hasRole(Roles::key(role));
}

QStringList encodeSites(const QList<BlockedSite> &sites)
{
    QStringList entries;
    for (const BlockedSite &site : sites)
        entries.append(site.site + QLatin1Char('\t') + site.reason);
    return entries;
}

QList<BlockedSite> decodeSites(const QStringList &entries)
{
    QList<BlockedSite> sites;
    for (const QString &entry : entries) {
        const QString site = entry.section(QLatin1Char('\t'), 0, 0);
        if (!site.isEmpty())
            sites.append({site, entry.section(QLatin1Char('\t'), 1)});
    }
    return sites;
}

QJsonArray sitesJson(const QList<BlockedSite> &sites)
{
    QJsonArray array;
    for (const BlockedSite &site : sites)
        array.append(QJsonObject{{QStringLiteral("site"), site.site}, {QStringLiteral("reason"), site.reason}});
    return array;
}

QByteArray httpResponse(int status, const QByteArray &body)
{
    const QByteArray reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found"
                                                   : status == 429 ? "Too Many Requests" : "Bad Request";
    return "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason
        + "\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size())
        + "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n" + body;
}

QByteArray error(const char *message)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("error"), QString::fromLatin1(message)}}).toJson(QJsonDocument::Compact);
}

QString clip(QString text, int length)
{
    text = text.simplified();
    return text.size() > length ? text.left(length) : text;
}

} // namespace

Classroom *Classroom::instance()
{
    static Classroom *classroom = new Classroom(QCoreApplication::instance());
    return classroom;
}

Classroom::Classroom(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<UnblockRequest>();
}

void Classroom::start()
{
    connect(Preferences::instance(), &Preferences::changed, this, &Classroom::refresh, Qt::UniqueConnection);
    refresh();
}

void Classroom::refresh()
{
    if (hasRole(Roles::Role::Teacher) && classOpen())
        serve();
    else
        stopServing();

    const bool following = hasRole(Roles::Role::Student) && inClass();
    if (following && !m_poll) {
        m_poll = new QTimer(this);
        m_poll->setInterval(kPollMs);
        connect(m_poll, &QTimer::timeout, this, &Classroom::poll);
        m_poll->start();
        poll();
    } else if (!following && m_poll) {
        m_poll->deleteLater();
        m_poll = nullptr;
        m_reachable = false;
    }
}

QNetworkAccessManager *Classroom::network()
{
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        // The teacher is on the local network; no proxy stands between.
        m_network->setProxy(QNetworkProxy::NoProxy);
    }
    return m_network;
}

// ---- Teacher -------------------------------------------------------------------

bool Classroom::classOpen() const
{
    return QSettings().value(kTeacher + QStringLiteral("open"), false).toBool();
}

void Classroom::setClassOpen(bool open)
{
    QSettings().setValue(kTeacher + QStringLiteral("open"), open);
    if (open && code().isEmpty())
        newCode();
    refresh();
    emit changed();
}

QString Classroom::className() const
{
    const QString name = QSettings().value(kTeacher + QStringLiteral("name")).toString();
    return name.isEmpty() ? tr("My class") : name;
}

void Classroom::setClassName(const QString &name)
{
    QSettings().setValue(kTeacher + QStringLiteral("name"), clip(name, 60));
    emit changed();
}

QString Classroom::code() const
{
    return QSettings().value(kTeacher + QStringLiteral("code")).toString();
}

void Classroom::newCode()
{
    QSettings().setValue(kTeacher + QStringLiteral("code"), makeCode());
    emit changed();
}

QList<BlockedSite> Classroom::blockedSites() const
{
    return decodeSites(QSettings().value(kTeacher + QStringLiteral("sites")).toStringList());
}

int Classroom::setBlockedSites(const QList<BlockedSite> &sites)
{
    QList<BlockedSite> clean;
    QStringList seen;
    for (const BlockedSite &entry : sites) {
        const QString site = Roles::normalizeSite(entry.site);
        if (site.isEmpty() || seen.contains(site))
            continue;
        seen.append(site);
        clean.append({site, clip(entry.reason, 200)});
        if (clean.size() == kMaxSites)
            break;
    }
    QSettings().setValue(kTeacher + QStringLiteral("sites"), encodeSites(clean));
    emit changed();
    return int(clean.size());
}

void Classroom::answer(const QString &requestId, Answer answer)
{
    for (int i = 0; i < m_requests.size(); ++i) {
        const UnblockRequest request = m_requests.at(i);
        if (request.id != requestId)
            continue;
        m_requests.removeAt(i);
        if (answer == Answer::AllowStudent) {
            QSettings settings;
            const QString key = kTeacher + QStringLiteral("allowed/") + request.studentId;
            QStringList allowed = settings.value(key).toStringList();
            if (!allowed.contains(request.site))
                allowed.append(request.site);
            settings.setValue(key, allowed);
        } else if (answer == Answer::AllowEveryone) {
            QList<BlockedSite> sites = blockedSites();
            sites.removeIf([&request](const BlockedSite &site) { return site.site == request.site; });
            setBlockedSites(sites);
        }
        m_answers[request.studentId].append({request.site, answer != Answer::Decline});
        // The same request from the same student is answered with it.
        m_requests.removeIf([&request](const UnblockRequest &other) {
            return other.studentId == request.studentId && other.site == request.site;
        });
        emit changed();
        return;
    }
}

QList<ClassStudent> Classroom::students() const
{
    QList<ClassStudent> list = m_students.values();
    std::sort(list.begin(), list.end(), [](const ClassStudent &a, const ClassStudent &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return list;
}

bool Classroom::serving() const
{
    return m_server && m_server->isListening();
}

QStringList Classroom::localAddresses()
{
    QStringList addresses;
    for (const QHostAddress &address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()
            && !address.isLinkLocal())
            addresses.append(address.toString());
    }
    return addresses;
}

void Classroom::serve()
{
    if (serving())
        return;
    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &Classroom::handleConnection);
    }
    m_port = 0;
    for (quint16 port = kHttpPort; port < kHttpPort + 20; ++port) {
        if (m_server->listen(QHostAddress::AnyIPv4, port)) {
            m_port = port;
            break;
        }
    }
    if (!m_discovery) {
        m_discovery = new QUdpSocket(this);
        if (m_discovery->bind(QHostAddress::AnyIPv4, kDiscoveryPort,
                              QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            connect(m_discovery, &QUdpSocket::readyRead, this, &Classroom::handleDiscovery);
    }
    emit changed();
}

void Classroom::stopServing()
{
    if (!m_server && !m_discovery)
        return;
    if (m_server)
        m_server->close();
    delete m_discovery;
    m_discovery = nullptr;
    m_port = 0;
    emit changed();
}

void Classroom::handleDiscovery()
{
    while (m_discovery && m_discovery->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_discovery->receiveDatagram(512);
        const QByteArray data = datagram.data();
        if (!data.startsWith("SPCLASS?") || !serving())
            continue;
        if (QString::fromUtf8(data.mid(8)).trimmed().compare(code(), Qt::CaseInsensitive) != 0)
            continue;
        const QByteArray reply = "SPCLASS!" + QByteArray::number(m_port) + '!' + className().toUtf8();
        m_discovery->writeDatagram(reply, datagram.senderAddress(), quint16(datagram.senderPort()));
    }
}

void Classroom::handleConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        auto buffer = std::make_shared<QByteArray>();
        QTimer::singleShot(10000, socket, [socket] { socket->abort(); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
            *buffer += socket->readAll();
            if (buffer->size() > kMaxRequestBytes) {
                socket->abort();
                return;
            }
            QByteArray method, target, body;
            const Parse result = parseHttp(*buffer, &method, &target, &body);
            if (result == Parse::Incomplete)
                return;
            int status = 400;
            const QByteArray json = result == Parse::Bad ? error("bad request")
                                                        : respond(method, target, body, socket->peerAddress(), &status);
            socket->write(httpResponse(result == Parse::Bad ? 400 : status, json));
            socket->disconnectFromHost();
        });
    }
}

QByteArray Classroom::respond(const QByteArray &method, const QByteArray &target, const QByteArray &body,
                              const QHostAddress &peer, int *status)
{
    Q_UNUSED(peer);
    const QUrl url(QStringLiteral("http://teacher") + QString::fromUtf8(target));
    const QUrlQuery query(url);
    *status = 200;

    if (method == "GET" && url.path() == QLatin1String("/v1/class")) {
        if (query.queryItemValue(QStringLiteral("code")).compare(code(), Qt::CaseInsensitive) != 0) {
            *status = 403;
            return error("wrong class code");
        }
        const QString id = clip(query.queryItemValue(QStringLiteral("id"), QUrl::FullyDecoded), 64);
        if (!id.isEmpty()) {
            ClassStudent &student = m_students[id];
            const bool isNew = student.id.isEmpty();
            student.id = id;
            student.name = clip(query.queryItemValue(QStringLiteral("name"), QUrl::FullyDecoded), kMaxName);
            student.lastSeen = QDateTime::currentDateTime();
            if (isNew)
                emit changed();
        }
        QJsonArray answers;
        for (const auto &answer : m_answers.take(id))
            answers.append(QJsonObject{{QStringLiteral("site"), answer.first}, {QStringLiteral("allowed"), answer.second}});
        const QJsonObject reply{
            {QStringLiteral("name"), className()},
            {QStringLiteral("sites"), sitesJson(blockedSites())},
            {QStringLiteral("allowed"),
             QJsonArray::fromStringList(QSettings().value(kTeacher + QStringLiteral("allowed/") + id).toStringList())},
            {QStringLiteral("answers"), answers},
        };
        return QJsonDocument(reply).toJson(QJsonDocument::Compact);
    }

    if (method == "POST" && url.path() == QLatin1String("/v1/request")) {
        const QJsonObject request = QJsonDocument::fromJson(body).object();
        if (request.value(QStringLiteral("code")).toString().compare(code(), Qt::CaseInsensitive) != 0) {
            *status = 403;
            return error("wrong class code");
        }
        UnblockRequest entry;
        entry.studentId = clip(request.value(QStringLiteral("id")).toString(), 64);
        entry.student = clip(request.value(QStringLiteral("name")).toString(), kMaxName);
        entry.site = Roles::normalizeSite(request.value(QStringLiteral("site")).toString());
        entry.note = clip(request.value(QStringLiteral("note")).toString(), kMaxNote);
        if (entry.studentId.isEmpty() || entry.site.isEmpty()) {
            *status = 400;
            return error("missing student or site");
        }
        int pending = 0;
        for (const UnblockRequest &other : std::as_const(m_requests)) {
            if (other.studentId == entry.studentId)
                ++pending;
            if (other.studentId == entry.studentId && other.site == entry.site)
                return QJsonDocument(QJsonObject{{QStringLiteral("ok"), true}}).toJson(QJsonDocument::Compact);
        }
        if (pending >= kMaxPendingPerStudent) {
            *status = 429;
            return error("too many requests waiting");
        }
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry.when = QDateTime::currentDateTime();
        if (entry.student.isEmpty())
            entry.student = tr("A student");
        m_requests.append(entry);
        emit requestReceived(entry);
        emit changed();
        return QJsonDocument(QJsonObject{{QStringLiteral("ok"), true}}).toJson(QJsonDocument::Compact);
    }

    *status = 404;
    return error("not found");
}

// ---- Student -------------------------------------------------------------------

bool Classroom::inClass() const
{
    return !QSettings().value(kStudent + QStringLiteral("code")).toString().isEmpty();
}

QString Classroom::joinedClassName() const
{
    return QSettings().value(kStudent + QStringLiteral("className")).toString();
}

QString Classroom::studentName() const
{
    return QSettings().value(kStudent + QStringLiteral("name")).toString();
}

int Classroom::joinedSiteCount() const
{
    return int(QSettings().value(kStudent + QStringLiteral("sites")).toStringList().size());
}

bool Classroom::blocks(const QUrl &url, QString *site, QString *reason) const
{
    if (!hasRole(Roles::Role::Student) || !inClass())
        return false;
    QSettings settings;
    const QList<BlockedSite> sites = decodeSites(settings.value(kStudent + QStringLiteral("sites")).toStringList());
    const BlockedSite *blocked = match(url, sites, settings.value(kStudent + QStringLiteral("allowed")).toStringList());
    if (!blocked)
        return false;
    if (site)
        *site = blocked->site;
    if (reason)
        *reason = blocked->reason;
    return true;
}

QUrl Classroom::teacherUrl(const QString &path) const
{
    QSettings settings;
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(settings.value(kStudent + QStringLiteral("host")).toString());
    url.setPort(settings.value(kStudent + QStringLiteral("port"), kHttpPort).toInt());
    url.setPath(path);
    return url;
}

void Classroom::discover(const QString &code, std::function<void(QHostAddress, quint16)> found)
{
    auto *socket = new QUdpSocket(this);
    socket->bind(QHostAddress::AnyIPv4, 0);
    const QByteArray question = "SPCLASS?" + code.toUpper().toUtf8();
    // Every network this computer is on, and this computer itself.
    QList<QHostAddress> targets{QHostAddress::Broadcast, QHostAddress::LocalHost};
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (!entry.broadcast().isNull() && !targets.contains(entry.broadcast()))
                targets.append(entry.broadcast());
        }
    }
    for (const QHostAddress &target : std::as_const(targets))
        socket->writeDatagram(question, target, kDiscoveryPort);

    auto done = std::make_shared<bool>(false);
    connect(socket, &QUdpSocket::readyRead, this, [socket, found, done] {
        while (socket->hasPendingDatagrams()) {
            const QNetworkDatagram datagram = socket->receiveDatagram(512);
            const QList<QByteArray> parts = datagram.data().split('!');
            if (*done || parts.size() < 2 || parts.first() != "SPCLASS")
                continue;
            *done = true;
            QHostAddress address = datagram.senderAddress();
            if (address.protocol() == QAbstractSocket::IPv6Protocol)
                address = QHostAddress(address.toIPv4Address());
            found(address, quint16(parts.at(1).toUInt()));
            socket->deleteLater();
            return;
        }
    });
    QTimer::singleShot(3000, socket, [socket, found, done] {
        if (!*done) {
            *done = true;
            found(QHostAddress(), 0);
        }
        socket->deleteLater();
    });
}

void Classroom::join(const QString &text, const QString &name)
{
    QString code, host;
    quint16 port = kHttpPort;
    if (!parseJoinCode(text, &code, &host, &port)) {
        emit joinFinished(false, tr("That is not a class code: it has six letters and digits, like K7MP3X."));
        return;
    }
    const QString studentName = clip(name, kMaxName);
    QSettings settings;
    QString id = settings.value(kStudent + QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(kStudent + QStringLiteral("id"), id);
    }

    const auto connectTo = [this, code, studentName, id](const QHostAddress &address, quint16 port, bool fixed) {
        if (address.isNull()) {
            emit joinFinished(false, tr("No class with that code answered on this network. Check the code, "
                                        "or ask your teacher for their address and type code@address."));
            return;
        }
        QUrl url;
        url.setScheme(QStringLiteral("http"));
        url.setHost(address.toString());
        url.setPort(port);
        url.setPath(QStringLiteral("/v1/class"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("code"), code);
        query.addQueryItem(QStringLiteral("id"), id);
        query.addQueryItem(QStringLiteral("name"), studentName);
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setTransferTimeout(5000);
        QNetworkReply *reply = network()->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, code, studentName, address, port, fixed] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit joinFinished(false, reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 403
                                             ? tr("The teacher's computer answered, but not to that code.")
                                             : tr("The teacher's computer did not answer (%1).").arg(reply->errorString()));
                return;
            }
            QSettings settings;
            settings.setValue(kStudent + QStringLiteral("code"), code);
            settings.setValue(kStudent + QStringLiteral("host"), address.toString());
            settings.setValue(kStudent + QStringLiteral("port"), port);
            settings.setValue(kStudent + QStringLiteral("fixedHost"), fixed);
            settings.setValue(kStudent + QStringLiteral("name"), studentName);
            m_reachable = true;
            m_failures = 0;
            applyClass(reply->readAll());
            refresh();
            emit joinFinished(true, joinedClassName());
        });
    };

    if (!host.isEmpty()) {
        connectTo(QHostAddress(host), port, true);
        return;
    }
    discover(code, [connectTo](QHostAddress address, quint16 port) { connectTo(address, port, false); });
}

void Classroom::leave()
{
    QSettings settings;
    for (const QString &key : {QStringLiteral("code"), QStringLiteral("host"), QStringLiteral("port"),
                               QStringLiteral("fixedHost"), QStringLiteral("className"), QStringLiteral("sites"),
                               QStringLiteral("allowed")})
        settings.remove(kStudent + key);
    refresh();
    emit changed();
}

void Classroom::poll()
{
    if (m_polling || !inClass())
        return;
    m_polling = true;
    QSettings settings;
    QUrl url = teacherUrl(QStringLiteral("/v1/class"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("code"), settings.value(kStudent + QStringLiteral("code")).toString());
    query.addQueryItem(QStringLiteral("id"), settings.value(kStudent + QStringLiteral("id")).toString());
    query.addQueryItem(QStringLiteral("name"), studentName());
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(5000);
    QNetworkReply *reply = network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_polling = false;
        if (reply->error() == QNetworkReply::NoError) {
            const bool was = m_reachable;
            m_reachable = true;
            m_failures = 0;
            applyClass(reply->readAll());
            if (!was)
                emit changed();
            return;
        }
        const bool was = m_reachable;
        m_reachable = false;
        if (was)
            emit changed();
        // The teacher's computer may have a new address: look for the class again.
        QSettings settings;
        if (++m_failures % 2 == 0 && !settings.value(kStudent + QStringLiteral("fixedHost")).toBool()) {
            discover(settings.value(kStudent + QStringLiteral("code")).toString(),
                     [this](QHostAddress address, quint16 port) {
                         if (address.isNull() || !inClass())
                             return;
                         QSettings settings;
                         settings.setValue(kStudent + QStringLiteral("host"), address.toString());
                         settings.setValue(kStudent + QStringLiteral("port"), port);
                         poll();
                     });
        }
    });
}

void Classroom::applyClass(const QByteArray &json)
{
    const QJsonObject reply = QJsonDocument::fromJson(json).object();
    QList<BlockedSite> sites;
    for (const QJsonValue &value : reply.value(QStringLiteral("sites")).toArray()) {
        const QJsonObject site = value.toObject();
        const QString name = Roles::normalizeSite(site.value(QStringLiteral("site")).toString());
        if (!name.isEmpty() && sites.size() < kMaxSites)
            sites.append({name, clip(site.value(QStringLiteral("reason")).toString(), 200)});
    }
    QStringList allowed;
    for (const QJsonValue &value : reply.value(QStringLiteral("allowed")).toArray())
        allowed.append(Roles::normalizeSite(value.toString()));

    QSettings settings;
    const QStringList encoded = encodeSites(sites);
    const bool same = settings.value(kStudent + QStringLiteral("sites")).toStringList() == encoded
        && settings.value(kStudent + QStringLiteral("allowed")).toStringList() == allowed
        && joinedClassName() == reply.value(QStringLiteral("name")).toString();
    settings.setValue(kStudent + QStringLiteral("className"), clip(reply.value(QStringLiteral("name")).toString(), 60));
    settings.setValue(kStudent + QStringLiteral("sites"), encoded);
    settings.setValue(kStudent + QStringLiteral("allowed"), allowed);
    if (!same)
        emit changed();
    for (const QJsonValue &value : reply.value(QStringLiteral("answers")).toArray()) {
        const QJsonObject answer = value.toObject();
        emit answerReceived(answer.value(QStringLiteral("site")).toString(),
                            answer.value(QStringLiteral("allowed")).toBool());
    }
}

void Classroom::ask(const QString &site, const QString &note)
{
    if (!inClass()) {
        emit askFinished(false);
        return;
    }
    QSettings settings;
    const QJsonObject body{
        {QStringLiteral("code"), settings.value(kStudent + QStringLiteral("code")).toString()},
        {QStringLiteral("id"), settings.value(kStudent + QStringLiteral("id")).toString()},
        {QStringLiteral("name"), studentName()},
        {QStringLiteral("site"), site},
        {QStringLiteral("note"), clip(note, kMaxNote)},
    };
    QNetworkRequest request(teacherUrl(QStringLiteral("/v1/request")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(5000);
    QNetworkReply *reply = network()->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        emit askFinished(reply->error() == QNetworkReply::NoError);
    });
}

// ---- Shared --------------------------------------------------------------------

QString Classroom::makeCode()
{
    static const QString letters = QStringLiteral("ABCDEFGHJKMNPQRSTUVWXYZ23456789");
    QString code;
    for (int i = 0; i < 6; ++i)
        code.append(letters.at(QRandomGenerator::system()->bounded(int(letters.size()))));
    return code;
}

bool Classroom::parseJoinCode(const QString &text, QString *code, QString *host, quint16 *port)
{
    const QString trimmed = text.trimmed();
    const QString codePart = trimmed.section(QLatin1Char('@'), 0, 0).toUpper().remove(QLatin1Char(' '));
    static const QRegularExpression valid(QStringLiteral("^[A-Z0-9]{6}$"));
    if (!valid.match(codePart).hasMatch())
        return false;
    *code = codePart;
    host->clear();
    const QString where = trimmed.section(QLatin1Char('@'), 1);
    if (!where.isEmpty()) {
        const QString address = where.section(QLatin1Char(':'), 0, 0);
        if (QHostAddress(address).isNull())
            return false;
        *host = address;
        const QString portText = where.section(QLatin1Char(':'), 1);
        if (!portText.isEmpty()) {
            bool ok = false;
            const uint value = portText.toUInt(&ok);
            if (!ok || value == 0 || value > 65535)
                return false;
            *port = quint16(value);
        }
    }
    return true;
}

const BlockedSite *Classroom::match(const QUrl &url, const QList<BlockedSite> &sites, const QStringList &allowed)
{
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return nullptr;
    if (Roles::isBlocked(url, allowed))
        return nullptr;
    for (const BlockedSite &site : sites) {
        if (Roles::isBlocked(url, {site.site}))
            return &site;
    }
    return nullptr;
}

QString Classroom::blockedPage(const QUrl &url, const QString &site, const QString &reason, const QString &className)
{
    Q_UNUSED(url);
    const QString style = QStringLiteral(
        "body{font:16px/1.5 system-ui,sans-serif;display:grid;place-items:center;min-height:90vh;margin:0;"
        "color:#3b3f4a;background:#f6f7fb}main{max-width:34rem;padding:2rem;text-align:center}"
        "h1{font-size:1.5rem;margin:.5rem 0}.reason{background:#fff;border-radius:12px;padding:1rem 1.25rem;"
        "margin:1rem 0;box-shadow:0 1px 3px #0002;text-align:left}.reason b{display:block;font-size:.8rem;"
        "opacity:.7;margin-bottom:.25rem}textarea{width:100%;box-sizing:border-box;min-height:4.5rem;"
        "font:inherit;border-radius:10px;border:1px solid #c9cdd6;padding:.6rem}button{margin-top:.6rem;"
        "font:inherit;padding:.5rem 1.2rem;border:0;border-radius:999px;background:#3166e0;color:#fff;"
        "cursor:pointer}p{opacity:.85;margin:.25rem 0}@media (prefers-color-scheme: dark){body{color:#d6d9e0;"
        "background:#15171d}.reason{background:#22252d}textarea{background:#22252d;color:inherit;"
        "border-color:#3a3e48}}");
    const QString reasonBlock = reason.trimmed().isEmpty()
        ? QString()
        : QStringLiteral("<div class=\"reason\"><b>%1</b>%2</div>")
              .arg(tr("Your teacher says").toHtmlEscaped(), reason.toHtmlEscaped());
    return QStringLiteral(
               "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
               "<meta name=\"color-scheme\" content=\"light dark\"><title>%1</title><style>%2</style></head>"
               "<body><main><div style=\"font-size:3rem\">🏫</div><h1>%3</h1><p>%4</p>%5"
               "<form action=\"sp://classroom/ask\" method=\"get\"><input type=\"hidden\" name=\"site\" value=\"%6\">"
               "<textarea name=\"note\" maxlength=\"300\" placeholder=\"%7\"></textarea>"
               "<button type=\"submit\">%8</button></form></main></body></html>")
        .arg(tr("Blocked by your class").toHtmlEscaped(), style,
             tr("%1 is blocked in class").arg(site).toHtmlEscaped(),
             tr("%1 has blocked this site for now.").arg(className.isEmpty() ? tr("Your teacher") : className)
                 .toHtmlEscaped(),
             reasonBlock, site.toHtmlEscaped(),
             tr("Why do you need it? (optional)").toHtmlEscaped(), tr("Ask to unblock").toHtmlEscaped());
}

QString Classroom::askedPage(const QString &site, bool sent)
{
    const QString title = sent ? tr("Asked your teacher") : tr("Your teacher could not be reached");
    const QString text = sent
        ? tr("Your teacher has your request to unblock %1. You will see their answer in the browser, and "
             "if they allow it, this page loads by itself.").arg(site)
        : tr("Their computer did not answer. Try again in a moment, or ask them in person.");
    return QStringLiteral(
               "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"color-scheme\" "
               "content=\"light dark\"><title>%1</title><style>body{font:16px/1.5 system-ui,sans-serif;"
               "display:grid;place-items:center;min-height:90vh;margin:0;text-align:center}main{max-width:32rem;"
               "padding:2rem}</style></head><body><main><div style=\"font-size:3rem\">%2</div><h1>%1</h1><p>%3</p>"
               "</main></body></html>")
        .arg(title.toHtmlEscaped(), sent ? QStringLiteral("✉️") : QStringLiteral("📡"), text.toHtmlEscaped());
}

Classroom::Parse Classroom::parseHttp(const QByteArray &data, QByteArray *method, QByteArray *target, QByteArray *body)
{
    const int end = data.indexOf("\r\n\r\n");
    if (end < 0)
        return data.size() > 8192 ? Parse::Bad : Parse::Incomplete;
    const QList<QByteArray> lines = data.left(end).split('\n');
    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    if (requestLine.size() != 3 || !requestLine.at(1).startsWith('/'))
        return Parse::Bad;
    *method = requestLine.at(0);
    *target = requestLine.at(1);
    qsizetype length = 0;
    for (const QByteArray &line : lines.mid(1)) {
        const int colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).trimmed().toLower() == "content-length") {
            bool ok = false;
            length = line.mid(colon + 1).trimmed().toLongLong(&ok);
            if (!ok || length < 0 || length > kMaxRequestBytes)
                return Parse::Bad;
        }
    }
    if (data.size() - (end + 4) < length)
        return Parse::Incomplete;
    *body = data.mid(end + 4, length);
    return Parse::Complete;
}

}
