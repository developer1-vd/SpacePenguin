#include "cosmeticfilters.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace spacepenguin {
namespace {

QJsonArray toJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            array.append(value.trimmed());
    }
    return array;
}

} // namespace

QString buildCosmeticSource(const QList<CosmeticGroup> &groups)
{
    QJsonArray encoded;
    for (const CosmeticGroup &group : groups) {
        if (group.selectors.isEmpty())
            continue;

        QJsonObject object;
        object.insert(QStringLiteral("d"), toJson(group.domains));
        object.insert(QStringLiteral("x"), toJson(group.excludedDomains));
        object.insert(QStringLiteral("s"), toJson(group.selectors));
        object.insert(QStringLiteral("e"), group.isException);
        encoded.append(object);
    }

    if (encoded.isEmpty())
        return QString();

    const QString payload = QString::fromUtf8(QJsonDocument(encoded).toJson(QJsonDocument::Compact));

    return QStringLiteral(
               "(function () {\n"
               "  'use strict';\n"
               "  if (window.__spCosmetic) return;\n"
               "  window.__spCosmetic = true;\n"
               "  var groups = %1;\n"
               "  var host = String(location.hostname || '').toLowerCase();\n"
               "  var applies = function (group) {\n"
               "    for (var e = 0; e < group.x.length; e++) {\n"
               "      if (host === group.x[e] || host.endsWith('.' + group.x[e])) return false;\n"
               "    }\n"
               "    if (!group.d.length) return true;\n"
               "    for (var d = 0; d < group.d.length; d++) {\n"
               "      if (host === group.d[d] || host.endsWith('.' + group.d[d])) return true;\n"
               "    }\n"
               "    return false;\n"
               "  };\n"
               "  var hidden = [];\n"
               "  var shown = [];\n"
               "  for (var i = 0; i < groups.length; i++) {\n"
               "    var group = groups[i];\n"
               "    if (!applies(group)) continue;\n"
               "    var target = group.e ? shown : hidden;\n"
               "    for (var s = 0; s < group.s.length; s++) {\n"
               "      if (target.indexOf(group.s[s]) < 0) target.push(group.s[s]);\n"
               "    }\n"
               "  }\n"
               "  var selectors = hidden.filter(function (selector) {\n"
               "    return shown.indexOf(selector) < 0;\n"
               "  });\n"
               "  if (!selectors.length) return;\n"
               "  // Each selector becomes its own CSS rule, so a selector the engine\n"
               "  // rejects is dropped by the CSS parser instead of breaking the sheet.\n"
               "  var css = selectors.map(function (selector) {\n"
               "    return selector + '{display:none!important}';\n"
               "  }).join('');\n"
               "  var sheet = document.createElement('style');\n"
               "  sheet.id = 'sp-cosmetic';\n"
               "  sheet.textContent = css;\n"
               "  var install = function () {\n"
               "    var parent = document.head || document.documentElement;\n"
               "    if (!parent || document.getElementById('sp-cosmetic')) return;\n"
               "    parent.appendChild(sheet);\n"
               "  };\n"
               "  install();\n"
               "  if (document.head) return;\n"
               "  document.addEventListener('DOMContentLoaded', install);\n"
               "})();")
        .arg(payload);
}

}