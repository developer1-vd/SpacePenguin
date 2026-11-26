#pragma once

#include <QRegularExpression>
#include <QString>

namespace spacepenguin::UserAgent {

/**
 * The browser's user agent without QtWebEngine's own "QtWebEngine/6.x" token,
 * so it reads like any Chromium browser. Bot protection (eBay's, among others)
 * takes that token for an automated browser and refuses the page; the rest of
 * the string, and the client hints, are Chromium's already.
 */
inline QString withoutEngineToken(QString userAgent)
{
    static const QRegularExpression token(QStringLiteral(" ?QtWebEngine/[^ ]+"));
    return userAgent.remove(token);
}

}
