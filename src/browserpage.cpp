#include "browserpage.h"

#include "classroom.h"
#include "logins.h"
#include "preferences.h"
#include "roles.h"
#include "tor.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrlQuery>

#include <QWebEngineSettings>

#include <memory>

namespace spacepenguin {
namespace {

constexpr qint64 kHoverGraceMs = 1500;

} // namespace

BrowserPage::BrowserPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    // Video players may fill the screen; the window takes them there (see BrowserWindow).
    settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    // PDFs open in Chromium's own viewer, which Qt counts as a plugin.
    settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    // Through Tor, WebRTC may not tell sites the addresses of this computer.
    if (Tor::isActive())
        settings()->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true);

    // Remember the internal link last under the pointer. The hover is cleared just
    // before a click navigates, so keep it for a moment afterwards.
    connect(this, &QWebEnginePage::linkHovered, this, [this](const QString &url) {
        const QUrl link(url);
        if (isAboutPage(link)) {
            m_hoveredLink = link;
            m_hoverEnded.invalidate();
        } else if (url.isEmpty()) {
            m_hoverEnded.start();
        } else {
            m_hoveredLink.clear();
        }
    });

    // Chromium renders only about:blank. A link to any other about: page is swapped
    // for about:blank#blocked inside the renderer, without a navigation request we
    // could intercept; the link under the pointer still says where the user meant to go.
    connect(this, &QWebEnginePage::urlChanged, this, [this](const QUrl &url) {
        if (url != QUrl(QStringLiteral("about:blank#blocked")) || !isAboutPage(m_hoveredLink))
            return;
        if (m_hoverEnded.isValid() && m_hoverEnded.elapsed() > kHoverGraceMs)
            return;
        const QUrl target = m_hoveredLink;
        m_hoveredLink.clear();
        emit internalPageRequested(target);
    });
}

bool BrowserPage::isAboutPage(const QUrl &url)
{
    return url.scheme().compare(QLatin1String("about"), Qt::CaseInsensitive) == 0
        && !url.matches(QUrl(QStringLiteral("about:blank")), QUrl::RemoveFragment);
}

bool BrowserPage::isAllowedScheme(const QString &scheme)
{
    return scheme.compare(QLatin1String("http"), Qt::CaseInsensitive) == 0
        || scheme.compare(QLatin1String("https"), Qt::CaseInsensitive) == 0
        || scheme.compare(QLatin1String("sp"), Qt::CaseInsensitive) == 0
        || scheme.compare(QLatin1String("qrc"), Qt::CaseInsensitive) == 0
        || scheme.compare(QLatin1String("about"), Qt::CaseInsensitive) == 0
        || scheme.compare(QLatin1String("data"), Qt::CaseInsensitive) == 0
        // Page source, for developers (Ctrl+U).
        || scheme.compare(QLatin1String("view-source"), Qt::CaseInsensitive) == 0
        // Local files: Chromium itself keeps websites from opening them.
        || scheme.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0;
}

bool BrowserPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (!isMainFrame)
        return true;

    // Student mode: "Ask to unblock" on a class's blocked page goes to the teacher.
    if (url.scheme() == QLatin1String("sp") && url.host() == QLatin1String("classroom")
        && url.path() == QLatin1String("/ask")) {
        const QUrlQuery query(url);
        const QString site = query.queryItemValue(QStringLiteral("site"), QUrl::FullyDecoded);
        const QString note = query.queryItemValue(QStringLiteral("note"), QUrl::FullyDecoded).replace(QLatin1Char('+'), QLatin1Char(' '));
        // The page keeps the blocked site's address, so an allowed site loads in its place.
        const QUrl blocked = this->url();
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(Classroom::instance(), &Classroom::askFinished, this,
                              [this, site, blocked, connection](bool ok) {
                                  disconnect(*connection);
                                  setHtml(Classroom::askedPage(site, ok), blocked);
                              });
        Classroom::instance()->ask(site, note);
        return false;
    }
    // Student mode: the class's blocked sites, with the teacher's reason.
    QString classSite, classReason;
    if (Classroom::instance()->blocks(url, &classSite, &classReason)) {
        const QString className = Classroom::instance()->joinedClassName();
        QTimer::singleShot(0, this, [this, url, classSite, classReason, className] {
            setHtml(Classroom::blockedPage(url, classSite, classReason, className), url);
        });
        return false;
    }
    // Focus mode (student, worker) keeps its sites away, with a page that says so.
    const Preferences *preferences = Preferences::instance();
    if (preferences->focusMode() && Roles::hasFocusMode(preferences->roles())
        && Roles::isBlocked(url, preferences->focusSites())) {
        QTimer::singleShot(0, this, [this, url] { setHtml(Roles::blockedPage(url), url); });
        return false;
    }
    if (m_navigationFilter && !m_navigationFilter(url, type))
        return false;
    // Onion sites exist only on the Tor network, which only Tor windows reach.
    if (!Tor::isActive() && Tor::isOnion(url)) {
        emit onionRequested(url);
        return false;
    }

    const bool userInitiated = type == NavigationTypeLinkClicked
        || type == NavigationTypeFormSubmitted || type == NavigationTypeTyped;
    if (userInitiated && isAboutPage(url)) {
        emit internalPageRequested(url);
        return false;
    }
    if (userInitiated && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"))
        && url.path().endsWith(QLatin1String(".user.js"), Qt::CaseInsensitive)) {
        emit userScriptRequested(url);
        return false;
    }

    return isAllowedScheme(url.scheme());
}

void BrowserPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                           const QString &message, int lineNumber,
                                           const QString &sourceID)
{
    // Logins reported by the capture script, from its isolated world.
    if (message.startsWith(LoginStore::captureToken())) {
        const QJsonObject login =
            QJsonDocument::fromJson(message.mid(LoginStore::captureToken().size()).toUtf8()).object();
        const QString password = login.value(QStringLiteral("p")).toString();
        if (!password.isEmpty())
            emit loginSubmitted(login.value(QStringLiteral("u")).toString(), password);
        return;
    }
    static const bool forward = qEnvironmentVariableIntValue("SPACEPENGUIN_JS_CONSOLE") != 0;
    if (forward)
        QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
}

}
