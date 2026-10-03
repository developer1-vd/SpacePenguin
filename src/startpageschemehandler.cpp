#include "startpageschemehandler.h"

#include <QFile>
#include <QWebEngineUrlRequestJob>

namespace spacepenguin {

StartPageSchemeHandler::StartPageSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
{
    QFile file(QStringLiteral(":/html/start.html"));
    if (file.open(QIODevice::ReadOnly))
        m_document.setData(file.readAll());

    m_document.open(QIODevice::ReadOnly);
}

StartPageSchemeHandler::~StartPageSchemeHandler() = default;

void StartPageSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    if (m_document.data().isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    m_document.seek(0);
    job->reply(QByteArrayLiteral("text/html; charset=utf-8"), &m_document);
}

}