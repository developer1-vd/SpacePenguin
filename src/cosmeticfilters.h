#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace spacepenguin {

struct CosmeticGroup {
    QStringList domains;
    QStringList excludedDomains;
    QStringList selectors;
    bool isException = false;
};

/**
 * Builds the JavaScript injected into every page that hides elements matched by the
 * cosmetic rules applying to that page's hostname.
 *
 * Cosmetic filtering is what removes the ads a network blocker cannot see: overlays,
 * interstitials, in-page popups and "pop-out" players inside the page DOM.
 */
QString buildCosmeticSource(const QList<CosmeticGroup> &groups);

}