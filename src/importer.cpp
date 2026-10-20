#include "importer.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace spacepenguin {
namespace Importer {
namespace {

void collectChromium(const QJsonObject &node, QStringList folders, QList<ImportedBookmark> *out)
{
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("url")) {
        const QUrl url(node.value(QStringLiteral("url")).toString());
        if (url.isValid())
            out->append({folders, node.value(QStringLiteral("name")).toString(), url});
        return;
    }
    for (const QJsonValue &value : node.value(QStringLiteral("children")).toArray()) {
        const QJsonObject child = value.toObject();
        QStringList path = folders;
        if (child.value(QStringLiteral("type")).toString() == QLatin1String("folder"))
            path.append(child.value(QStringLiteral("name")).toString());
        collectChromium(child, path, out);
    }
}

QString unescapeHtml(QString text)
{
    static const QRegularExpression numeric(QStringLiteral("&#(x?)([0-9a-fA-F]+);"));
    QRegularExpressionMatch match;
    while ((match = numeric.match(text)).hasMatch()) {
        bool ok = false;
        const uint code = match.captured(2).toUInt(&ok, match.captured(1).isEmpty() ? 10 : 16);
        const char32_t character = ok ? char32_t(code) : U'?';
        text.replace(match.capturedStart(), match.capturedLength(), QString::fromUcs4(&character, 1));
    }
    text.replace(QLatin1String("&lt;"), QLatin1String("<"));
    text.replace(QLatin1String("&gt;"), QLatin1String(">"));
    text.replace(QLatin1String("&quot;"), QLatin1String("\""));
    text.replace(QLatin1String("&#39;"), QLatin1String("'"));
    text.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return text.trimmed();
}

QList<QStringList> parseCsv(const QString &text)
{
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field += c;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char(',')) {
            row.append(field);
            field.clear();
        } else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            if (c == QLatin1Char('\r') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n'))
                ++i;
            row.append(field);
            field.clear();
            if (!(row.size() == 1 && row.first().isEmpty()))
                rows.append(row);
            row.clear();
        } else {
            field += c;
        }
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row.append(field);
        rows.append(row);
    }
    return rows;
}

} // namespace

QList<Browser> installedBrowsers()
{
    struct Known {
        const char *name;
        const char *onLinux;
        const char *onMac;
        const char *onWindows;
    };
    static const Known known[] = {
        {"Google Chrome", ".config/google-chrome", "Library/Application Support/Google/Chrome",
         "Google/Chrome/User Data"},
        {"Microsoft Edge", ".config/microsoft-edge", "Library/Application Support/Microsoft Edge",
         "Microsoft/Edge/User Data"},
        {"Brave", ".config/BraveSoftware/Brave-Browser", "Library/Application Support/BraveSoftware/Brave-Browser",
         "BraveSoftware/Brave-Browser/User Data"},
        {"Chromium", ".config/chromium", "Library/Application Support/Chromium", "Chromium/User Data"},
        {"Vivaldi", ".config/vivaldi", "Library/Application Support/Vivaldi", "Vivaldi/User Data"},
    };
    QList<Browser> found;
    const QString home = QDir::homePath();
#if defined(Q_OS_WIN)
    const QString base = QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA"));
#endif
    for (const Known &browser : known) {
#if defined(Q_OS_WIN)
        const QString folder = QDir(base).filePath(QString::fromLatin1(browser.onWindows));
#elif defined(Q_OS_MACOS)
        const QString folder = QDir(home).filePath(QString::fromLatin1(browser.onMac));
#else
        const QString folder = QDir(home).filePath(QString::fromLatin1(browser.onLinux));
#endif
        const QString file = QDir(folder).filePath(QStringLiteral("Default/Bookmarks"));
        if (QFile::exists(file))
            found.append({QString::fromLatin1(browser.name), file});
    }
    return found;
}

QList<ImportedBookmark> parseChromiumBookmarks(const QByteArray &json)
{
    QList<ImportedBookmark> bookmarks;
    const QJsonObject roots = QJsonDocument::fromJson(json).object().value(QStringLiteral("roots")).toObject();
    // The bookmarks bar's contents stay at the top; the other roots become folders.
    collectChromium(roots.value(QStringLiteral("bookmark_bar")).toObject(), {}, &bookmarks);
    for (const QString &root : {QStringLiteral("other"), QStringLiteral("synced")}) {
        const QJsonObject node = roots.value(root).toObject();
        collectChromium(node, {node.value(QStringLiteral("name")).toString()}, &bookmarks);
    }
    return bookmarks;
}

QList<ImportedBookmark> parseBookmarksHtml(const QString &html)
{
    // <DT><H3 …>Folder</H3> opens a folder that its <DL> holds; </DL> closes it.
    static const QRegularExpression token(
        QStringLiteral("<H3([^>]*)>(.*?)</H3>|<A\\s([^>]*)>(.*?)</A>|</DL>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression href(QStringLiteral("HREF\\s*=\\s*\"([^\"]*)\""),
                                         QRegularExpression::CaseInsensitiveOption);
    QList<ImportedBookmark> bookmarks;
    QStringList folders;
    QList<bool> toolbar; // whether each open folder is the browser's toolbar, kept at the top
    for (auto it = token.globalMatch(html); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        if (match.capturedLength(2) > 0 || match.captured(0).startsWith(QLatin1String("<H3"), Qt::CaseInsensitive)) {
            const bool isToolbar = match.captured(1).contains(QLatin1String("PERSONAL_TOOLBAR_FOLDER"), Qt::CaseInsensitive);
            toolbar.append(isToolbar);
            folders.append(unescapeHtml(match.captured(2)));
        } else if (match.captured(0).startsWith(QLatin1String("<A"), Qt::CaseInsensitive)) {
            const QUrl url(unescapeHtml(href.match(match.captured(3)).captured(1)));
            if (!url.isValid() || url.isEmpty() || url.scheme() == QLatin1String("place")
                || url.scheme() == QLatin1String("javascript")) {
                continue;
            }
            QStringList path;
            for (int i = 0; i < folders.size(); ++i) {
                if (!toolbar.at(i))
                    path.append(folders.at(i));
            }
            bookmarks.append({path, unescapeHtml(match.captured(4)), url});
        } else if (!folders.isEmpty()) {
            folders.removeLast();
            toolbar.removeLast();
        }
    }
    return bookmarks;
}

QList<Login> parseLoginsCsv(const QString &csv)
{
    QList<Login> logins;
    const QList<QStringList> rows = parseCsv(csv);
    if (rows.isEmpty())
        return logins;
    const QStringList header = rows.first();
    const auto column = [&header](const QStringList &names) {
        for (int i = 0; i < header.size(); ++i) {
            if (names.contains(header.at(i).trimmed().toLower()))
                return i;
        }
        return -1;
    };
    // Chrome and Edge: name,url,username,password; Firefox: url,username,password,…;
    // Bitwarden: …,login_uri,login_username,login_password.
    const int url = column({QStringLiteral("url"), QStringLiteral("login_uri"), QStringLiteral("website"),
                            QStringLiteral("origin")});
    const int user = column({QStringLiteral("username"), QStringLiteral("login_username"), QStringLiteral("login")});
    const int password = column({QStringLiteral("password"), QStringLiteral("login_password")});
    if (url < 0 || password < 0)
        return logins;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int r = 1; r < rows.size(); ++r) {
        const QStringList &row = rows.at(r);
        if (row.size() <= qMax(url, password))
            continue;
        const QString origin = LoginStore::originOf(QUrl(row.at(url).trimmed()));
        if (origin.isEmpty() || row.at(password).isEmpty())
            continue;
        logins.append(Login{origin, user >= 0 && user < row.size() ? row.at(user) : QString(),
                            row.at(password), now, QDateTime()});
    }
    return logins;
}

}
}
