#include "browserpage.h"

#include <QWebEngineSettings>

namespace spacepenguin {

BrowserPage::BrowserPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
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
    Q_UNUSED(type)
    if (!isMainFrame)
        return true;
    return isAllowedScheme(url.scheme());
}

}