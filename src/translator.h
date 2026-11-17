#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

namespace spacepenguin {

/**
 * Translating pages and selected text with an online service the person picks.
 *
 * - Google translates whole pages through its proxy (*.translate.goog), which
 *   fetches the page itself, so it works for public pages, not ones behind a
 *   sign-in.
 * - LibreTranslate (a server of the person's choosing, possibly their own)
 *   translates the page in place: its text goes to that server and comes back.
 * - Bing, DeepL and Yandex translate text in their own translator pages; for a
 *   page, its article's text goes there.
 *
 * Nothing is sent until the person asks for a translation.
 */
class Translator : public QObject
{
    Q_OBJECT

public:
    enum class Service { Google, Bing, DeepL, Yandex, LibreTranslate };

    static QList<Service> services();
    static QString key(Service service);
    static Service fromKey(const QString &key);
    static QString name(Service service);

    /** Whether @p service can translate a whole page where it is (Google, LibreTranslate). */
    static bool translatesPages(Service service);

    /** The plain language to translate into, from a tag ("pt-BR" → "pt"; Chinese keeps its form). */
    static QString targetLanguage(const QString &tag);

    /** Google's translation of the page at @p url; invalid for other services or other pages. */
    static QUrl pageUrl(Service service, const QUrl &url, const QString &target);
    /** The service's translator with @p text in it, cut to what it accepts. */
    static QUrl textUrl(Service service, const QString &text, const QString &target,
                        const QUrl &libreTranslateServer = QUrl());
    /** The most text a translator page takes in its address. */
    static int textLimit(Service service);

    /** Whether @p url is one of the translators' own pages or proxies. */
    static bool isTranslatorUrl(const QUrl &url);

    /**
     * LibreTranslate: translates @p texts (with their HTML) into @p target on
     * @p server, then calls @p done with the translations in the same order, or
     * an error message.
     */
    static void translateTexts(QNetworkAccessManager *network, const QUrl &server, const QString &apiKey,
                               const QStringList &texts, const QString &target,
                               std::function<void(QStringList translations, QString error)> done);

    /** The page script that collects text to translate in place, and the one that puts it back. */
    static QString collectScript();
    static QString applyScript(const QStringList &translations);
};

}
