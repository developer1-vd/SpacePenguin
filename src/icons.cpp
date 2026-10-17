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
 * The mark: a padlock. A black hole's accretion disc is the shackle, a penguin
 * peeks out of the lock's body, and the body holds the Earth. Flat colours only,
 * drawn on a 100-unit grid; html/start.html and docs/assets/logo.svg carry the
 * same shapes as SVG.
 */
const QColor kInk(0x16, 0x17, 0x1d);
const QColor kSnow(0xff, 0xff, 0xff);
const QColor kRim(0xff, 0xff, 0xff); // outlines the black parts, for dark backgrounds
const QColor kBeak(0xff, 0xb2, 0x1f);
const QColor kDisc(0xff, 0x7a, 0x1a);
const QColor kOcean(0x6a, 0x4f, 0xe0);
const QColor kLand(0x2f, 0xb7, 0xa3);

/** A shape with straight sides and a round top: the head, the face, the shackle. */
QPainterPath arch(qreal left, qreal right, qreal top, qreal bottom, bool close = true)
{
    const qreal width = right - left;
    QPainterPath path;
    path.moveTo(left, bottom);
    path.lineTo(left, top + width / 2);
    path.arcTo(QRectF(left, top, width, width), 180, -180);
    path.lineTo(right, bottom);
    if (close)
        path.closeSubpath();
    return path;
}

QPainterPath land()
{
    QPainterPath path;
    path.moveTo(33, 64);
    path.cubicTo(37, 61, 43, 61, 46, 64);
    path.cubicTo(48, 67, 45, 69, 45, 72);
    path.cubicTo(45, 76, 49, 78, 48, 82);
    path.cubicTo(47, 87, 42, 89, 39, 87);
    path.cubicTo(36, 85, 37, 80, 34, 77);
    path.cubicTo(31, 74, 28, 71, 29, 68);
    path.cubicTo(30, 66, 31, 65, 33, 64);
    path.closeSubpath();
    path.moveTo(56, 61);
    path.cubicTo(61, 60, 67, 62, 69, 66);
    path.cubicTo(70, 69, 68, 71, 65, 71);
    path.cubicTo(62, 72, 62, 75, 63, 78);
    path.cubicTo(65, 82, 64, 86, 60, 88);
    path.cubicTo(56, 89, 55, 85, 54, 81);
    path.cubicTo(53, 76, 50, 73, 50, 68);
    path.cubicTo(50, 64, 52, 62, 56, 61);
    path.closeSubpath();
    return path;
}

/** Draws the mark into the 24-unit square at the painter's origin. */
void drawPenguin(QPainter *p)
{
    p->save();
    p->scale(0.24, 0.24);
    p->translate(50, 50); // centre the drawing's bounds in the square
    p->scale(0.92, 0.92);
    p->translate(-50, -53.25);

    // The shackle.
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(kDisc, 7, Qt::SolidLine, Qt::FlatCap));
    p->drawPath(arch(25.5, 74.5, 10.5, 54, false));

    // The head and the lock's body, as one black shape with a light rim.
    QPainterPath body;
    body.addRoundedRect(QRectF(17, 47, 66, 50), 14, 14);
    body = body.united(arch(32, 68, 17, 50));
    p->setPen(QPen(kRim, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(kInk);
    p->drawPath(body);
    p->setPen(Qt::NoPen);
    p->drawPath(body);

    // The face.
    p->setBrush(kSnow);
    p->drawPath(arch(37, 63, 23, 47));
    p->setBrush(kInk);
    p->drawEllipse(QPointF(45, 34), 2.6, 2.6);
    p->drawEllipse(QPointF(55, 34), 2.6, 2.6);
    p->setPen(QPen(kBeak, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(kBeak);
    p->drawPolygon(QPolygonF({QPointF(47, 38.5), QPointF(53, 38.5), QPointF(50, 42.5)}));
    p->setPen(Qt::NoPen);

    // The Earth.
    QPainterPath globe;
    globe.addEllipse(QPointF(50, 72), 20, 20);
    p->setBrush(kOcean);
    p->drawPath(globe);
    p->setBrush(kLand);
    p->drawPath(land().intersected(globe));
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
    case Icons::ChevronLeft:
        p->drawPolyline(QPolygonF({QPointF(15, 6), QPointF(9, 12), QPointF(15, 18)}));
        break;
    case Icons::ChevronRight:
        p->drawPolyline(QPolygonF({QPointF(9, 6), QPointF(15, 12), QPointF(9, 18)}));
        break;
    case Icons::SideTabs:
        p->drawRoundedRect(QRectF(3.5, 5, 17, 14), 2.5, 2.5);
        p->drawLine(QPointF(9.5, 5), QPointF(9.5, 19));
        p->drawLine(QPointF(5.8, 9), QPointF(7.2, 9));
        p->drawLine(QPointF(5.8, 12), QPointF(7.2, 12));
        p->drawLine(QPointF(5.8, 15), QPointF(7.2, 15));
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
