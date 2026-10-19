#include "userextensions.h"

#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace spacepenguin {
namespace {

QStringList metadataValues(const QString &contents, const QString &key)
{
    QStringList values;
    const QString tag = QLatin1String("@") + key;
    const bool hasBlock = contents.contains(QLatin1String("==UserScript=="));
    bool inBlock = !hasBlock;
    for (const QString &line : contents.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1String("//")))
            continue;
        const QString comment = trimmed.mid(2).trimmed();
        if (comment.startsWith(QLatin1String("==UserScript==")))
            inBlock = true;
        else if (comment.startsWith(QLatin1String("==/UserScript==")))
            break;
        if (!inBlock)
            continue;
        const int space = comment.indexOf(QRegularExpression(QStringLiteral("\\s")));
        if (space > 0 && comment.left(space) == tag)
            values.append(comment.mid(space + 1).trimmed());
        else if (comment == tag)
            values.append(QString());
    }
    return values;
}

QString metadataValue(const QString &contents, const QString &key, const QString &fallback = QString())
{
    const QStringList values = metadataValues(contents, key);
    return values.isEmpty() ? fallback : values.first();
}

/** Each pattern's values, split at commas as an older SpacePenguin wrote them. */
QStringList patternValues(const QString &contents, const QString &key)
{
    QStringList patterns;
    for (const QString &value : metadataValues(contents, key)) {
        for (const QString &pattern : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (!pattern.trimmed().isEmpty())
                patterns.append(pattern.trimmed());
        }
    }
    return patterns;
}

QString globToRegExp(const QString &glob, const QString &star = QStringLiteral(".*"))
{
    QString escaped = QRegularExpression::escape(glob);
    escaped.replace(QStringLiteral("\\*"), star);
    return escaped;
}

/** One @match or @include pattern as a regular expression for a whole address. */
QString patternToRegExp(const QString &pattern)
{
    if (pattern == QLatin1String("<all_urls>") || pattern == QLatin1String("*"))
        return QStringLiteral(".*");
    if (pattern.size() > 2 && pattern.startsWith(QLatin1Char('/')) && pattern.endsWith(QLatin1Char('/')))
        return pattern.mid(1, pattern.size() - 2); // an @include regular expression
    static const QRegularExpression parts(QStringLiteral("^([^:/]+)://([^/]*)(/.*)?$"));
    const QRegularExpressionMatch match = parts.match(pattern);
    if (!match.hasMatch())
        return globToRegExp(pattern);
    const QString scheme = match.captured(1) == QLatin1String("*") ? QStringLiteral("https?")
                                                                   : globToRegExp(match.captured(1));
    QString host = match.captured(2);
    if (host == QLatin1String("*"))
        host = QStringLiteral("[^/]*");
    else if (host.startsWith(QLatin1String("*.")))
        host = QStringLiteral("(?:[^/]*\\.)?") + globToRegExp(host.mid(2), QStringLiteral("[^/]*"));
    else
        host = globToRegExp(host, QStringLiteral("[^/]*"));
    const QString path = match.captured(3).isEmpty() ? QStringLiteral("/.*") : globToRegExp(match.captured(3));
    return scheme + QStringLiteral("://") + host + QStringLiteral("(?::\\d+)?") + path;
}

/**
 * The GM_ functions scripts get from Tampermonkey and Violentmonkey, and their
 * GM. promise forms. Values are kept in the site's storage under the script's own
 * key, so a script's settings are kept per site.
 */
QString gmApi()
{
    return QStringLiteral(R"js(
  var unsafeWindow = window;
  var GM_info = {script: spInfo, scriptHandler: 'SpacePenguin', version: '1.0'};
  var GM_log = console.log.bind(console);
  var spMemory = {};
  var spStore = null;
  try { spStore = window.localStorage; } catch (e) {}
  function GM_getValue(key, fallback) {
    var raw = Object.prototype.hasOwnProperty.call(spMemory, key) ? spMemory[key]
            : (spStore ? spStore.getItem(spKey + key) : null);
    if (raw === null || raw === undefined) return fallback;
    try { return JSON.parse(raw); } catch (e) { return fallback; }
  }
  function GM_setValue(key, value) {
    var raw = JSON.stringify(value);
    spMemory[key] = raw;
    try { if (spStore) spStore.setItem(spKey + key, raw); } catch (e) {}
  }
  function GM_deleteValue(key) {
    delete spMemory[key];
    try { if (spStore) spStore.removeItem(spKey + key); } catch (e) {}
  }
  function GM_listValues() {
    var keys = Object.keys(spMemory);
    if (spStore) {
      for (var i = 0; i < spStore.length; ++i) {
        var k = spStore.key(i);
        if (k.indexOf(spKey) === 0 && keys.indexOf(k.slice(spKey.length)) < 0) keys.push(k.slice(spKey.length));
      }
    }
    return keys;
  }
  function GM_addStyle(css) {
    var style = document.createElement('style');
    style.textContent = css;
    (document.head || document.documentElement).appendChild(style);
    return style;
  }
  function GM_addElement(parent, tag, attributes) {
    if (typeof parent === 'string') { attributes = tag; tag = parent; parent = null; }
    var element = document.createElement(tag);
    for (var name in (attributes || {})) {
      if (name === 'textContent') element.textContent = attributes[name];
      else element.setAttribute(name, attributes[name]);
    }
    (parent || document.head || document.documentElement).appendChild(element);
    return element;
  }
  // Commands for the extensions menu; the browser reads this list from the page.
  var spMenu = window.__spMenu = window.__spMenu || [];
  function GM_registerMenuCommand(name, fn) {
    spMenu.push({id: spMenu.length, script: spInfo.name, name: String(name), fn: fn});
    return spMenu.length - 1;
  }
  function GM_unregisterMenuCommand(id) {
    if (spMenu[id] && spMenu[id].script === spInfo.name) spMenu[id] = null;
  }
  function GM_openInTab(url) {
    var opened = window.open(url, '_blank');
    return {close: function () { if (opened) opened.close(); }, closed: !opened};
  }
  function GM_setClipboard(text) {
    var copy = function () {
      var area = document.createElement('textarea');
      area.value = text;
      document.documentElement.appendChild(area);
      area.select();
      document.execCommand('copy');
      area.remove();
    };
    try { navigator.clipboard.writeText(String(text)).catch(copy); } catch (e) { copy(); }
  }
  function GM_notification(details, title, image, onclick) {
    if (typeof details === 'string') details = {text: details, title: title, image: image, onclick: onclick};
    var show = function () {
      var shown = new Notification(details.title || spInfo.name, {body: details.text || '', icon: details.image});
      if (details.onclick) shown.onclick = details.onclick;
    };
    if (Notification.permission === 'granted') show();
    else Notification.requestPermission().then(function (p) { if (p === 'granted') show(); });
  }
  function GM_xmlhttpRequest(details) {
    var abort = new AbortController();
    var answer = {readyState: 4, finalUrl: details.url, status: 0, statusText: '', responseHeaders: '',
                  responseText: '', response: null, context: details.context};
    var timer = details.timeout ? setTimeout(function () {
      abort.abort();
      if (details.ontimeout) details.ontimeout(answer);
    }, details.timeout) : 0;
    fetch(details.url, {method: details.method || 'GET', headers: details.headers || {}, body: details.data,
                        credentials: details.anonymous ? 'omit' : 'include', signal: abort.signal})
      .then(function (reply) {
        answer.status = reply.status;
        answer.statusText = reply.statusText;
        answer.finalUrl = reply.url;
        reply.headers.forEach(function (value, name) { answer.responseHeaders += name + ': ' + value + '\r\n'; });
        var type = details.responseType;
        return (type === 'arraybuffer' ? reply.arrayBuffer() : type === 'blob' ? reply.blob() : reply.text())
          .then(function (body) {
            if (typeof body === 'string') {
              answer.responseText = body;
              answer.response = body;
              if (type === 'json') { try { answer.response = JSON.parse(body); } catch (e) { answer.response = null; } }
            } else {
              answer.response = body;
            }
            clearTimeout(timer);
            if (details.onload) details.onload(answer);
          });
      })
      .catch(function (error) {
        clearTimeout(timer);
        answer.error = String(error);
        if (abort.signal.aborted) { if (details.onabort) details.onabort(answer); }
        else if (details.onerror) details.onerror(answer);
      });
    return {abort: function () { abort.abort(); }};
  }
  var spPromise = function (fn) {
    return function () { var args = arguments; return new Promise(function (resolve) { resolve(fn.apply(null, args)); }); };
  };
  var GM = {
    info: GM_info, log: GM_log,
    getValue: spPromise(GM_getValue), setValue: spPromise(GM_setValue),
    deleteValue: spPromise(GM_deleteValue), listValues: spPromise(GM_listValues),
    addStyle: spPromise(GM_addStyle), addElement: spPromise(GM_addElement),
    registerMenuCommand: spPromise(GM_registerMenuCommand), unregisterMenuCommand: spPromise(GM_unregisterMenuCommand),
    openInTab: spPromise(GM_openInTab), setClipboard: spPromise(GM_setClipboard),
    notification: spPromise(GM_notification),
    xmlHttpRequest: function (details) {
      return new Promise(function (resolve, reject) {
        var done = details.onload, failed = details.onerror;
        details.onload = function (r) { if (done) done(r); resolve(r); };
        details.onerror = function (r) { if (failed) failed(r); reject(r); };
        GM_xmlhttpRequest(details);
      });
    }
  };
)js");
}

QString patternsToRegExp(const QStringList &patterns, const QString &fallback)
{
    QStringList sources;
    for (const QString &pattern : patterns)
        sources.append(QStringLiteral("(?:") + patternToRegExp(pattern) + QStringLiteral(")"));
    if (sources.isEmpty())
        return fallback;
    return QStringLiteral("^(?:") + sources.join(QLatin1Char('|')) + QStringLiteral(")$");
}

/** @p text as a JavaScript string literal. */
QString jsString(const QString &text)
{
    const QByteArray array = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

} // namespace

UserScripts::UserScripts(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
}

void UserScripts::setDirectory(const QString &path)
{
    m_directory = path;
    reloadFromDisk();
}

bool UserScripts::addScript(const QString &filePath, QString *error)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        if (error)
            *error = tr("No such file: %1").arg(filePath);
        return false;
    }

    if (info.suffix().compare(QLatin1String("js"), Qt::CaseInsensitive) != 0) {
        if (error)
            *error = tr("User scripts must be .js files.");
        return false;
    }

    if (m_directory.isEmpty()) {
        if (error)
            *error = tr("No extension directory is configured.");
        return false;
    }

    QDir().mkpath(m_directory);
    const QString target = QDir(m_directory).filePath(info.fileName());
    const bool alreadyInstalled = QFileInfo(target).absoluteFilePath() == info.absoluteFilePath();

    if (!alreadyInstalled) {
        if (QFile::exists(target)) {
            if (error)
                *error = tr("An extension named %1 already exists.").arg(info.fileName());
            return false;
        }

        if (!QFile::copy(info.absoluteFilePath(), target)) {
            if (error)
                *error =
                    tr("Could not copy %1 into the extension directory.").arg(info.fileName());
            return false;
        }
    }

    reloadFromDisk();
    return true;
}

bool UserScripts::removeScript(const QString &id)
{
    for (const UserScript &script : std::as_const(m_scripts)) {
        if (script.id != id)
            continue;

        QFile::remove(script.filePath);
        reloadFromDisk();
        return true;
    }

    return false;
}

bool UserScripts::setScriptEnabled(const QString &id, bool enabled)
{
    for (UserScript &script : m_scripts) {
        if (script.id != id)
            continue;

        script.enabled = enabled;
        apply();
        return true;
    }

    return false;
}

bool UserScripts::isEnabled(const QString &id) const
{
    for (const UserScript &script : m_scripts) {
        if (script.id == id)
            return script.enabled;
    }
    return false;
}

void UserScripts::reloadFromDisk()
{
    m_scripts.clear();

    if (m_directory.isEmpty())
        return;

    QDir directory(m_directory);
    const QStringList files = directory.entryList({QStringLiteral("*.js")}, QDir::Files,
                                                  QDir::Name);
    for (const QString &file : files) {
        QFile handle(directory.filePath(file));
        if (!handle.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString contents = QString::fromUtf8(handle.readAll());
        handle.close();
        m_scripts.append(parse(directory.filePath(file), contents));
    }

    apply();
}

void UserScripts::apply()
{
    if (!m_profile)
        return;

    QWebEngineScriptCollection *collection = m_profile->scripts();

    // Only drop the user scripts; browser-owned scripts share this collection.
    const QList<QWebEngineScript> existing = collection->toList();
    for (const QWebEngineScript &script : existing) {
        if (script.name().startsWith(reservedPrefix()))
            continue;
        collection->remove(script);
    }

    for (const UserScript &script : std::as_const(m_scripts)) {
        if (!script.enabled)
            continue;

        QFile handle(script.filePath);
        if (!handle.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QString source = QString::fromUtf8(handle.readAll());
        handle.close();
        // Its libraries run first, as part of it, as in other script managers.
        QString libraries;
        for (const QString &require : script.libraries) {
            QFile library(requireFile(QUrl(require)));
            if (library.open(QIODevice::ReadOnly | QIODevice::Text))
                libraries += QString::fromUtf8(library.readAll()) + QLatin1Char('\n');
        }
        source.prepend(libraries);

        QWebEngineScript engineScript;
        engineScript.setName(script.name);
        // Scripts expect the page to be there unless they ask to start earlier.
        if (script.runAt == QLatin1String("document-start"))
            engineScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
        else if (script.runAt == QLatin1String("document-end") || script.runAt == QLatin1String("document-body"))
            engineScript.setInjectionPoint(QWebEngineScript::DocumentReady);
        else
            engineScript.setInjectionPoint(QWebEngineScript::Deferred);
        engineScript.setRunsOnSubFrames(!script.noFrames);
        engineScript.setWorldId(QWebEngineScript::ApplicationWorld);
        engineScript.setSourceCode(buildInjectedSource(script, source));
        collection->insert(engineScript);
    }

    emit changed();
}

UserScript UserScripts::parse(const QString &filePath, const QString &contents)
{
    UserScript script;
    script.filePath = filePath;
    script.id = QFileInfo(filePath).fileName();
    script.name = parseName(contents, script.id);
    script.matchPatterns = parseMatchPatterns(contents);
    script.excludePatterns = patternValues(contents, QStringLiteral("exclude"))
        + patternValues(contents, QStringLiteral("exclude-match"));
    script.libraries = parseRequires(contents);
    script.runAt = metadataValue(contents, QStringLiteral("run-at"));
    script.version = metadataValue(contents, QStringLiteral("version"));
    script.description = metadataValue(contents, QStringLiteral("description"));
    script.noFrames = !metadataValues(contents, QStringLiteral("noframes")).isEmpty();
    return script;
}

QString UserScripts::parseName(const QString &contents, const QString &fallback)
{
    return metadataValue(contents, QStringLiteral("name"), fallback);
}

QStringList UserScripts::parseMatchPatterns(const QString &contents)
{
    return patternValues(contents, QStringLiteral("match")) + patternValues(contents, QStringLiteral("include"));
}

QStringList UserScripts::parseRequires(const QString &contents)
{
    return metadataValues(contents, QStringLiteral("require"));
}

QString UserScripts::requireFile(const QUrl &url) const
{
    const QByteArray hash = QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha1).toHex();
    return QDir(m_directory).filePath(QStringLiteral("required/%1.js").arg(QString::fromLatin1(hash)));
}

QList<QUrl> UserScripts::missingRequires(const QString &contents) const
{
    QList<QUrl> missing;
    for (const QString &require : parseRequires(contents)) {
        const QUrl url(require);
        if (url.isValid() && !QFile::exists(requireFile(url)))
            missing.append(url);
    }
    return missing;
}

QString UserScripts::reservedPrefix()
{
    return QStringLiteral("spacepenguin:");
}

QString UserScripts::buildInjectedSource(const UserScript &script, const QString &source)
{
    const QJsonObject info{{QStringLiteral("name"), script.name},
                           {QStringLiteral("version"), script.version},
                           {QStringLiteral("description"), script.description}};
    // The wrapper's own names start with sp so they do not clash with the script's.
    return QStringLiteral(
               "(function() {\n"
               "  var spInclude = new RegExp(%1);\n"
               "  var spExclude = new RegExp(%2);\n"
               "  if (!spInclude.test(location.href) || spExclude.test(location.href)) return;\n"
               "  var spInfo = %3;\n"
               "  var spKey = %4;\n"
               "%5"
               "  try {\n%6\n"
               "  } catch (error) {\n"
               "    console.warn('SpacePenguin user script failed:', error);\n"
               "  }\n"
               "})();")
        .arg(jsString(patternsToRegExp(script.matchPatterns, QStringLiteral(".*"))),
             jsString(patternsToRegExp(script.excludePatterns, QStringLiteral("(?!)"))),
             QString::fromUtf8(QJsonDocument(info).toJson(QJsonDocument::Compact)),
             jsString(QStringLiteral("sp-gm:%1:").arg(script.id)), gmApi(), source);
}

QString UserScripts::menuCommandsScript()
{
    return QStringLiteral("(window.__spMenu || []).filter(function (c) { return c; })"
                          ".map(function (c) { return [c.id, c.script, c.name]; })");
}

QString UserScripts::runMenuCommandScript(int id)
{
    return QStringLiteral("(function () { var c = (window.__spMenu || [])[%1]; if (c) c.fn(); })()").arg(id);
}

}
