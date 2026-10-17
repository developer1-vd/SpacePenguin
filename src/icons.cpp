#include "icons.h"

#include "theme.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QtMath>

namespace spacepenguin {
namespace {

QPainterPath star(qreal cx, qreal cy, qreal outer, qreal inner)
{
    QPainterPath path;
    for (int i = 0; i < 10; ++i) {
        const qreal radius = (i % 2 == 0) ? outer : inner;
        const qreal angle = qDegreesToRadians(-90.0 + i * 36.0);
        const QPointF point(cx + radius * qCos(angle), cy + radius * qSin(angle));
        if (i == 0)
            path.moveTo(point);
        else
            path.lineTo(point);
    }
    path.closeSubpath();
    return path;
}

/*
 * The mark: a penguin curled around the Earth, flat colours only. Drawn on a
 * 100-unit grid; the same shapes are in html/start.html and docs/ as SVG.
 */
const QColor kInk(0x1c, 0x1f, 0x2b);
const QColor kSnow(0xf4, 0xf6, 0xfb);
const QColor kBeak(0xff, 0x9a, 0x1f);
const QColor kOcean(0x2f, 0x6f, 0xe0);
const QColor kLand(0x3d, 0xae, 0x5c);

const QPointF kEarth(56, 57);
constexpr qreal kEarthRadius = 31;
constexpr qreal kBodyInner = 34; // the body hugs the globe with a small gap
constexpr qreal kBodyStart = -80; // degrees: the head, at the top
constexpr qreal kBodySpan = 250;  // round the left side to the bottom right

QPointF onEarth(qreal degrees, qreal radius)
{
    const qreal a = qDegreesToRadians(degrees);
    return kEarth + QPointF(radius * qCos(a), radius * qSin(a));
}

/** Body thickness along the curl, t from 0 (head) to 1 (tail). */
qreal bodyWidth(qreal t)
{
    return 17 * (1 - qPow(t, 2.2));
}

qreal bellyWidth(qreal t)
{
    if (t < 0.12 || t > 0.9)
        return 0;
    return 0.5 * bodyWidth(t) * qSin(M_PI * (t - 0.12) / 0.78);
}

QPainterPath band(qreal (*width)(qreal))
{
    constexpr int steps = 60;
    QPolygonF outer, inner;
    for (int i = 0; i <= steps; ++i) {
        const qreal t = qreal(i) / steps;
        const qreal angle = kBodyStart - kBodySpan * t;
        outer << onEarth(angle, kBodyInner + width(t));
        inner.prepend(onEarth(angle, kBodyInner));
    }
    QPainterPath path;
    path.addPolygon(outer + inner);
    path.closeSubpath();
    return path;
}

QPainterPath land()
{
    QPainterPath path;
    path.moveTo(40, 32);
    path.cubicTo(48, 28, 58, 29, 62, 35);
    path.cubicTo(65, 40, 59, 43, 61, 48);
    path.cubicTo(63, 54, 70, 54, 70, 61);
    path.cubicTo(70, 67, 64, 70, 59, 68);
    path.cubicTo(54, 66, 55, 59, 50, 57);
    path.cubicTo(44, 55, 38, 58, 36, 52);
    path.cubicTo(34, 45, 34, 36, 40, 32);
    path.closeSubpath();
    path.moveTo(74, 64);
    path.cubicTo(79, 62, 85, 64, 87, 69);
    path.cubicTo(88, 74, 84, 81, 78, 84);
    path.cubicTo(73, 86, 69, 83, 70, 78);
    path.cubicTo(71, 74, 71, 66, 74, 64);
    path.closeSubpath();
    path.moveTo(50, 78);
    path.cubicTo(54, 77, 58, 79, 59, 82);
    path.cubicTo(60, 85, 57, 88, 53, 88);
    path.cubicTo(49, 88, 47, 86, 47, 83);
    path.cubicTo(47, 80, 48, 78, 50, 78);
    path.closeSubpath();
    return path;
}

/** Draws the mark into the 24-unit square at the painter's origin. */
void drawPenguin(QPainter *p)
{
    p->save();
    p->setPen(Qt::NoPen);
    p->scale(0.24, 0.24);
    p->translate(9.7, 0.4); // centre the drawing's bounds in the square
    p->scale(0.96, 0.96);

    QPainterPath globe;
    globe.addEllipse(kEarth, kEarthRadius, kEarthRadius);
    p->setBrush(kOcean);
    p->drawPath(globe);
    p->setBrush(kLand);
    p->drawPath(land().intersected(globe));

    p->setBrush(kInk);
    p->drawPath(band(bodyWidth));
    p->setBrush(kSnow);
    p->drawPath(band(bellyWidth));

    // The head, tilted a little towards the beak.
    p->translate(onEarth(kBodyStart, kBodyInner + 8.5));
    p->rotate(-10);
    p->setBrush(kInk);
    p->drawEllipse(QPointF(0, 0), 12, 10.5);
    p->setBrush(kSnow);
    p->drawEllipse(QPointF(4.8, 2.2), 5.6, 5);
    p->setBrush(kInk);
    p->drawEllipse(QPointF(5.4, 0.4), 1.5, 1.5);
    QPainterPath beak;
    beak.moveTo(9, 3.2);
    beak.quadTo(13, 3.8, 15.5, 5.5);
    beak.quadTo(12.9, 7, 9, 7.2);
    beak.closeSubpath();
    p->setBrush(kBeak);
    p->drawPath(beak);
    p->restore();
}

/**
 * The macOS shape: since Big Sur, and enforced by macOS 26 Tahoe, an app icon is a
 * rounded square filling 824 of 1024 units, centred. Icons that leave the shape
 * empty are shown inside a grey tile by Tahoe. A flat white tile with the mark on it.
 */
void drawMacPenguin(QPainter *p)
{
    p->save();
    p->scale(24.0 / 1024.0, 24.0 / 1024.0);
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::white);
    p->drawRoundedRect(QRectF(100, 100, 824, 824), 185.4, 185.4);
    p->restore();

    p->save();
    p->translate(12, 12);
    p->scale(0.62, 0.62);
    p->translate(-12, -12);
    drawPenguin(p);
    p->restore();
}

void drawGlyph(QPainter *p, Icons::Glyph glyph, const QColor &color, const QColor &accent, bool on)
{
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);

    switch (glyph) {
    case Icons::Back:
        p->drawLine(QPointF(19, 12), QPointF(5.5, 12));
        p->drawPolyline(QPolygonF({QPointF(11, 6), QPointF(5, 12), QPointF(11, 18)}));
        break;
    case Icons::Forward:
        p->drawLine(QPointF(5, 12), QPointF(18.5, 12));
        p->drawPolyline(QPolygonF({QPointF(13, 6), QPointF(19, 12), QPointF(13, 18)}));
        break;
    case Icons::Reload: {
        const QRectF circle(5, 5, 14, 14);
        QPainterPath arc;
        arc.arcMoveTo(circle, 40);
        arc.arcTo(circle, 40, 290);
        p->drawPath(arc);
        p->drawPolyline(QPolygonF({QPointF(18.8, 4.2), QPointF(18.6, 8.6), QPointF(14.2, 8.4)}));
        break;
    }
    case Icons::Stop:
    case Icons::Close:
        p->drawLine(QPointF(7, 7), QPointF(17, 17));
        p->drawLine(QPointF(17, 7), QPointF(7, 17));
        break;
    case Icons::Home:
        p->drawPolyline(QPolygonF({QPointF(4, 11.5), QPointF(12, 4.5), QPointF(20, 11.5)}));
        p->drawPolyline(QPolygonF({QPointF(6.5, 9.5), QPointF(6.5, 19.5), QPointF(17.5, 19.5),
                                   QPointF(17.5, 9.5)}));
        p->drawPolyline(QPolygonF({QPointF(10.2, 19.5), QPointF(10.2, 14.5), QPointF(13.8, 14.5),
                                   QPointF(13.8, 19.5)}));
        break;
    case Icons::WindowMinimize:
        p->drawLine(QPointF(7, 12.5), QPointF(17, 12.5));
        break;
    case Icons::WindowMaximize:
        p->drawRect(QRectF(7, 7, 10, 10));
        break;
    case Icons::WindowRestore:
        p->drawRect(QRectF(6.5, 9, 8.5, 8.5));
        p->drawPolyline(QPolygonF({QPointF(9, 9), QPointF(9, 6.5), QPointF(17.5, 6.5),
                                   QPointF(17.5, 15), QPointF(15, 15)}));
        break;
    case Icons::Plus:
        p->drawLine(QPointF(12, 5), QPointF(12, 19));
        p->drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    case Icons::Menu:
        p->drawLine(QPointF(5, 7), QPointF(19, 7));
        p->drawLine(QPointF(5, 12), QPointF(19, 12));
        p->drawLine(QPointF(5, 17), QPointF(19, 17));
        break;
    case Icons::Star:
        if (on) {
            p->setPen(QPen(accent, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(accent);
        }
        p->drawPath(star(12, 12.6, 8.6, 3.9));
        break;
    case Icons::Shield: {
        QPainterPath shield;
        shield.moveTo(12, 3.5);
        shield.lineTo(19, 6.2);
        shield.lineTo(19, 11);
        shield.cubicTo(19, 15.6, 16, 18.9, 12, 20.6);
        shield.cubicTo(8, 18.9, 5, 15.6, 5, 11);
        shield.lineTo(5, 6.2);
        shield.closeSubpath();
        p->drawPath(shield);
        if (on)
            p->drawPolyline(QPolygonF({QPointF(9, 12), QPointF(11.2, 14.2), QPointF(15.2, 9.6)}));
        break;
    }
    case Icons::Download:
        p->drawLine(QPointF(12, 4), QPointF(12, 15));
        p->drawPolyline(QPolygonF({QPointF(7, 10), QPointF(12, 15), QPointF(17, 10)}));
        p->drawLine(QPointF(5, 19.5), QPointF(19, 19.5));
        break;
    case Icons::Lock: {
        p->drawRoundedRect(QRectF(6, 10.5, 12, 9.5), 2, 2);
        QPainterPath shackle;
        shackle.moveTo(8.8, 10.5);
        shackle.lineTo(8.8, 8);
        shackle.arcTo(QRectF(8.8, 4.3, 6.4, 6.4), 180, -180);
        shackle.lineTo(15.2, 10.5);
        p->drawPath(shackle);
        break;
    }
    case Icons::Warning: {
        QPainterPath triangle;
        triangle.moveTo(12, 4);
        triangle.lineTo(21, 19.5);
        triangle.lineTo(3, 19.5);
        triangle.closeSubpath();
        p->drawPath(triangle);
        p->drawLine(QPointF(12, 9.5), QPointF(12, 14));
        p->setBrush(color);
        p->setPen(Qt::NoPen);
        p->drawEllipse(QPointF(12, 16.8), 1.1, 1.1);
        break;
    }
    case Icons::Info:
        p->drawEllipse(QPointF(12, 12), 8.2, 8.2);
        p->drawLine(QPointF(12, 11), QPointF(12, 16.5));
        p->setBrush(color);
        p->setPen(Qt::NoPen);
        p->drawEllipse(QPointF(12, 7.9), 1.1, 1.1);
        break;
    case Icons::Search:
        p->drawEllipse(QPointF(10.5, 10.5), 5.6, 5.6);
        p->drawLine(QPointF(14.7, 14.7), QPointF(19.5, 19.5));
        break;
    case Icons::ChevronUp:
        p->drawPolyline(QPolygonF({QPointF(6, 15), QPointF(12, 9), QPointF(18, 15)}));
        break;
    case Icons::ChevronDown:
        p->drawPolyline(QPolygonF({QPointF(6, 9), QPointF(12, 15), QPointF(18, 9)}));
        break;
    case Icons::History:
        p->drawEllipse(QPointF(12, 12), 8.2, 8.2);
        p->drawPolyline(QPolygonF({QPointF(12, 7.5), QPointF(12, 12), QPointF(15.2, 14)}));
        break;
    case Icons::Bookmarks:
        p->drawPolyline(QPolygonF({QPointF(7, 20), QPointF(7, 5.5), QPointF(17, 5.5),
                                   QPointF(17, 20), QPointF(12, 16), QPointF(7, 20)}));
        break;
    case Icons::Settings: {
        p->drawEllipse(QPointF(12, 12), 2.8, 2.8);
        p->drawEllipse(QPointF(12, 12), 6.6, 6.6);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = qDegreesToRadians(i * 45.0);
            p->drawLine(QPointF(12 + 6.6 * qCos(angle), 12 + 6.6 * qSin(angle)),
                        QPointF(12 + 9.0 * qCos(angle), 12 + 9.0 * qSin(angle)));
        }
        break;
    }
    case Icons::Extensions: {
        QPainterPath piece;
        piece.moveTo(5, 9);
        piece.lineTo(9.5, 9);
        piece.arcTo(QRectF(9.5, 5.5, 4.5, 4.5), 200, -220);
        piece.lineTo(18.5, 9);
        piece.lineTo(18.5, 12.5);
        piece.arcTo(QRectF(17, 12.5, 4.5, 4.5), 110, -220);
        piece.lineTo(18.5, 19.5);
        piece.lineTo(5, 19.5);
        piece.closeSubpath();
        p->drawPath(piece);
        break;
    }
    case Icons::Globe:
        p->drawEllipse(QPointF(12, 12), 8.2, 8.2);
        p->drawEllipse(QPointF(12, 12), 3.6, 8.2);
        p->drawLine(QPointF(4, 12), QPointF(20, 12));
        break;
    case Icons::Private:
        p->drawPolyline(QPolygonF({QPointF(3.5, 10.5), QPointF(20.5, 10.5)}));
        p->drawPolyline(QPolygonF({QPointF(6.5, 10.5), QPointF(8.5, 4.8), QPointF(15.5, 4.8),
                                   QPointF(17.5, 10.5)}));
        p->drawEllipse(QPointF(8, 15.5), 3, 3);
        p->drawEllipse(QPointF(16, 15.5), 3, 3);
        p->drawLine(QPointF(11, 15.2), QPointF(13, 15.2));
        break;
    case Icons::Penguin:
        drawPenguin(p);
        break;
    case Icons::Folder:
        p->drawPolyline(QPolygonF({QPointF(3.5, 18.5), QPointF(3.5, 6), QPointF(9.5, 6),
                                   QPointF(11.5, 8.5), QPointF(20.5, 8.5), QPointF(20.5, 18.5),
                                   QPointF(3.5, 18.5)}));
        break;
    case Icons::Open:
        p->drawPolyline(QPolygonF({QPointF(18, 13.5), QPointF(18, 19), QPointF(5, 19),
                                   QPointF(5, 6), QPointF(10.5, 6)}));
        p->drawLine(QPointF(11, 13), QPointF(19, 5));
        p->drawPolyline(QPolygonF({QPointF(14, 5), QPointF(19, 5), QPointF(19, 10)}));
        break;
    case Icons::Trash:
        p->drawLine(QPointF(4.5, 7), QPointF(19.5, 7));
        p->drawPolyline(QPolygonF({QPointF(9.5, 7), QPointF(9.5, 4.5), QPointF(14.5, 4.5),
                                   QPointF(14.5, 7)}));
        p->drawPolyline(QPolygonF({QPointF(6.5, 7), QPointF(7.5, 20), QPointF(16.5, 20),
                                   QPointF(17.5, 7)}));
        break;
    }
}

class GlyphEngine : public QIconEngine
{
public:
    explicit GlyphEngine(Icons::Glyph glyph)
        : m_glyph(glyph)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const QPalette palette = QApplication::palette();
        QColor color = palette.color(QPalette::Active, QPalette::ButtonText);
        if (mode == QIcon::Disabled)
            color.setAlphaF(0.35);
        const QColor accent = Theme::currentColors().accent;

        const qreal side = qMin(rect.width(), rect.height());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.x() + (rect.width() - side) / 2.0,
                           rect.y() + (rect.height() - side) / 2.0);
        painter->scale(side / 24.0, side / 24.0);
        drawGlyph(painter, m_glyph, color, accent, state == QIcon::On);
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state,
                         qreal scale) override
    {
        Q_UNUSED(scale)
        return pixmap(size, mode, state);
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }

    QIconEngine *clone() const override { return new GlyphEngine(m_glyph); }
    QString key() const override { return QStringLiteral("spacepenguin-glyph"); }
    bool isNull() override { return false; }

private:
    Icons::Glyph m_glyph;
};

} // namespace

QIcon Icons::icon(Glyph glyph)
{
    return QIcon(new GlyphEngine(glyph));
}

QImage Icons::appIconImage(int size, AppIconShape shape)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);
    if (shape == AppIconShape::MacOS)
        drawMacPenguin(&painter);
    else
        drawPenguin(&painter);
    painter.end();
    return image;
}

QIcon Icons::appIcon()
{
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 24.0, size / 24.0);
        drawPenguin(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

}
