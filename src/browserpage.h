#pragma once

#include <QWebEnginePage>

namespace spacepenguin {

class BrowserPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit BrowserPage(QWebEngineProfile *profile, QObject *parent = nullptr);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

    static bool isAllowedScheme(const QString &scheme);
};

}