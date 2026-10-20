#include "readerview.h"

namespace spacepenguin {

QString readerViewScript()
{
    return QStringLiteral(R"js((function () {
  if (document.documentElement.dataset.spReader) {
    location.reload();
    return 'off';
  }
  // The element whose own paragraphs hold the most text is the article.
  var best = null, bestScore = 0;
  document.querySelectorAll('article, main, [role="main"], section, div').forEach(function (element) {
    var score = 0;
    element.querySelectorAll(':scope > p, :scope > div > p, :scope > section > p').forEach(function (p) {
      score += p.textContent.trim().length;
    });
    if (element.tagName === 'ARTICLE' || element.getAttribute('role') === 'main') score *= 1.25;
    if (score > bestScore) { bestScore = score; best = element; }
  });
  if (!best || bestScore < 400) return 'none';

  var content = best.cloneNode(true);
  content.querySelectorAll('img[data-src]').forEach(function (img) { img.src = img.getAttribute('data-src'); });
  content.querySelectorAll('script, style, link, iframe, form, button, input, select, textarea, nav, aside, '
                           + 'footer, noscript, svg, canvas, [aria-hidden="true"], [hidden]').forEach(function (node) {
    node.remove();
  });
  var keep = ['href', 'src', 'srcset', 'alt', 'title', 'colspan', 'rowspan'];
  content.querySelectorAll('*').forEach(function (node) {
    Array.from(node.attributes).forEach(function (attribute) {
      if (keep.indexOf(attribute.name) < 0) node.removeAttribute(attribute.name);
    });
  });

  var heading = best.querySelector('h1') || document.querySelector('h1');
  var title = (heading && heading.textContent.trim()) || document.title;
  if (heading) {
    var copy = content.querySelector('h1');
    if (copy) copy.remove();
  }
  var authorMeta = document.querySelector('meta[name="author"], meta[property="article:author"]');
  var byline = authorMeta ? authorMeta.content : '';
  var site = location.hostname.replace(/^www\./, '');

  var escape = function (text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  };
  document.documentElement.dataset.spReader = '1';
  document.documentElement.removeAttribute('class');
  document.documentElement.removeAttribute('style');
  document.head.innerHTML = '<meta charset="utf-8"><meta name="color-scheme" content="light dark">'
    + '<title>' + escape(title) + '</title><style>'
    + ':root{--bg:#fbf8f2;--text:#22201c;--muted:#6b665c;--link:#2f5fe0;--rule:#e4ded2}'
    + '@media (prefers-color-scheme: dark){:root{--bg:#17181c;--text:#e6e3dc;--muted:#a19d94;--link:#8fa8ff;--rule:#2c2e35}}'
    + 'html{background:var(--bg);color:var(--text)}'
    + 'body{max-width:40rem;margin:0 auto;padding:3rem 1.5rem 5rem;font:1.2rem/1.7 Georgia,"Iowan Old Style",serif}'
    + 'h1{font-size:2.1rem;line-height:1.2;margin:0 0 .5rem}'
    + '.sp-byline{color:var(--muted);font:.9rem/1.4 system-ui,sans-serif;margin:0 0 2rem;'
    + 'padding-bottom:1.25rem;border-bottom:1px solid var(--rule)}'
    + 'h2,h3,h4{line-height:1.3;margin:2rem 0 .75rem}'
    + 'img,video,figure{max-width:100%;height:auto}figure{margin:1.5rem 0}figcaption{color:var(--muted);font-size:.9rem}'
    + 'a{color:var(--link)}blockquote{margin:1.5rem 0;padding-left:1rem;border-left:3px solid var(--rule);color:var(--muted)}'
    + 'pre,code{font-size:.9rem}pre{overflow-x:auto}table{border-collapse:collapse}td,th{border:1px solid var(--rule);padding:.3rem .5rem}'
    + '</style>';
  document.body.removeAttribute('class');
  document.body.removeAttribute('style');
  document.body.innerHTML = '<h1>' + escape(title) + '</h1><p class="sp-byline">'
    + escape([byline, site].filter(Boolean).join(' · ')) + '</p>';
  while (content.firstChild) document.body.appendChild(content.firstChild);
  window.scrollTo(0, 0);
  return 'on';
})())js");
}

}
