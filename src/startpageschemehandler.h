#pragma once

#include <QBuffer>
#include <QWebEngineUrlSchemeHandler>

namespace spacepenguin {

class StartPageSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit StartPageSchemeHandler(QObject *parent = nullptr);
    ~StartPageSchemeHandler() override;

    void requestStarted(QWebEngineUrlRequestJob *job) override;

private:
    QBuffer m_document;
};

}