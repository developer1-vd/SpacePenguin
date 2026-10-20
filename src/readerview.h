#pragma once

#include <QString>

namespace spacepenguin {

/**
 * Reader view: the page's article alone, in clean type. The script finds the
 * element whose paragraphs hold the most text and redraws the page as that
 * article, its title and its author; run again, it reloads the page as it was.
 * It answers "on", "off", or "none" when the page has no article.
 */
QString readerViewScript();

}
