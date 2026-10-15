// Regenerates the icon files in packaging/ from the vector drawing in
// src/icons.cpp, so the app mark lives in one place:
//
//   cmake -B build -DSPACEPENGUIN_BUILD_TOOLS=ON
//   cmake --build build --target spacepenguin-generate-icons
//   QT_QPA_PLATFORM=offscreen ./build/spacepenguin-generate-icons packaging
//
// Writes icons/spacepenguin-<size>.png (Linux), windows/spacepenguin.ico and
// macos/SpacePenguin.icns (Apple's rounded-square shape, as macOS 26 Tahoe wants).
// ICO and ICNS are written here directly: both are a small header around PNGs.

#include "icons.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QtEndian>

#include <cstdio>

using spacepenguin::Icons;

namespace {

QByteArray png(int size, Icons::AppIconShape shape)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    Icons::appIconImage(size, shape).save(&buffer, "PNG");
    return data;
}

bool write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(path));
        return false;
    }
    std::printf("wrote %s (%lld bytes)\n", qPrintable(path), static_cast<long long>(data.size()));
    return true;
}

// ICO: ICONDIR, one ICONDIRENTRY per image, then the PNG data (Vista and later).
QByteArray ico(const QList<int> &sizes)
{
    QList<QByteArray> images;
    for (const int size : sizes)
        images.append(png(size, Icons::AppIconShape::Round));

    QByteArray out;
    auto u16 = [&out](quint16 v) { v = qToLittleEndian(v); out.append(reinterpret_cast<const char *>(&v), 2); };
    auto u32 = [&out](quint32 v) { v = qToLittleEndian(v); out.append(reinterpret_cast<const char *>(&v), 4); };

    u16(0);
    u16(1); // icon
    u16(static_cast<quint16>(images.size()));
    quint32 offset = 6 + 16 * images.size();
    for (int i = 0; i < images.size(); ++i) {
        const int size = sizes.at(i);
        out.append(static_cast<char>(size >= 256 ? 0 : size)); // 0 means 256
        out.append(static_cast<char>(size >= 256 ? 0 : size));
        out.append('\0');                                      // no palette
        out.append('\0');
        u16(1);  // planes
        u16(32); // bits per pixel
        u32(static_cast<quint32>(images.at(i).size()));
        u32(offset);
        offset += images.at(i).size();
    }
    for (const QByteArray &image : images)
        out.append(image);
    return out;
}

// ICNS: "icns", total length, then (type, length, PNG) entries.
QByteArray icns()
{
    const QList<std::pair<const char *, int>> entries = {
        {"icp4", 16},  {"icp5", 32},  {"icp6", 64},   {"ic07", 128}, {"ic08", 256},
        {"ic09", 512}, {"ic10", 1024}, {"ic11", 32},  {"ic12", 64},  {"ic13", 256},
        {"ic14", 512},
    };
    QByteArray body;
    for (const auto &[type, size] : entries) {
        const QByteArray image = png(size, Icons::AppIconShape::MacOS);
        const quint32 length = qToBigEndian(static_cast<quint32>(image.size() + 8));
        body.append(type, 4);
        body.append(reinterpret_cast<const char *>(&length), 4);
        body.append(image);
    }
    QByteArray out("icns");
    const quint32 total = qToBigEndian(static_cast<quint32>(body.size() + 8));
    out.append(reinterpret_cast<const char *>(&total), 4);
    out.append(body);
    return out;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <packaging directory>\n", argv[0]);
        return 2;
    }
    const QDir packaging(QString::fromLocal8Bit(argv[1]));
    packaging.mkpath(QStringLiteral("icons"));
    packaging.mkpath(QStringLiteral("windows"));
    packaging.mkpath(QStringLiteral("macos"));

    bool ok = true;
    for (const int size : {16, 32, 48, 64, 128, 256, 512}) {
        ok &= write(packaging.filePath(QStringLiteral("icons/spacepenguin-%1.png").arg(size)),
                    png(size, Icons::AppIconShape::Round));
    }
    ok &= write(packaging.filePath(QStringLiteral("windows/spacepenguin.ico")),
                ico({16, 24, 32, 48, 64, 128, 256}));
    ok &= write(packaging.filePath(QStringLiteral("macos/SpacePenguin.icns")), icns());
    return ok ? 0 : 1;
}
