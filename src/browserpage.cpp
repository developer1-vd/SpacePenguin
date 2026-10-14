#include "browserpage.h"

#include <QWebEngineSettings>

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
    settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);

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
        || scheme.compare(QLatin1String("data"), Qt::CaseInsensitive) == 0;
}

bool BrowserPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (!isMainFrame)
        return true;

    const bool userInitiated = type == NavigationTypeLinkClicked
        || type == NavigationTypeFormSubmitted || type == NavigationTypeTyped;
    if (userInitiated && isAboutPage(url)) {
        emit internalPageRequested(url);
        return false;
    }

    return isAllowedScheme(url.scheme());
}

}