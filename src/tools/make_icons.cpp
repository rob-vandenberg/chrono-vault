// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg <RobVandenberg64@gmail.com>
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License version 3 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
// General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QSvgRenderer>
#include <QtEndian>

#include <cstdio>

// --- Version ------------------------------------------------------------
// make_icons.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

// A tool of the build, never shipped: turns the program's own icon,
// art/chrono-vault.svg, into what each system wants (D-019), so the SVG stays
// the one source:
//   make_icons <svg> <output folder>
// writes chrono-vault.ico (16 to 256 pixels in one file, for Windows) and
// chrono-vault-<size>.png at 16, 24, 32, 48, 64, 128 and 256 (for Linux).

namespace {

QImage render(QSvgRenderer& renderer, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    return image;
}

QByteArray png(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// An .ico file holding PNG pictures, as Windows reads them since Vista.
QByteArray ico(const QList<QPair<int, QByteArray>>& pictures)
{
    QByteArray out(6 + 16 * pictures.size(), '\0');
    auto* bytes = reinterpret_cast<uchar*>(out.data());
    qToLittleEndian<quint16>(0, bytes);
    qToLittleEndian<quint16>(1, bytes + 2);
    qToLittleEndian<quint16>(static_cast<quint16>(pictures.size()), bytes + 4);
    quint32 offset = static_cast<quint32>(out.size());
    for (qsizetype index = 0; index < pictures.size(); ++index) {
        uchar* entry = reinterpret_cast<uchar*>(out.data()) + 6 + 16 * index;
        const int size = pictures.at(index).first;
        entry[0] = static_cast<uchar>(size >= 256 ? 0 : size);
        entry[1] = static_cast<uchar>(size >= 256 ? 0 : size);
        qToLittleEndian<quint16>(1, entry + 4);
        qToLittleEndian<quint16>(32, entry + 6);
        qToLittleEndian<quint32>(static_cast<quint32>(pictures.at(index).second.size()), entry + 8);
        qToLittleEndian<quint32>(offset, entry + 12);
        offset += static_cast<quint32>(pictures.at(index).second.size());
    }
    for (const auto& picture : pictures) {
        out.append(picture.second);
    }
    return out;
}

bool write(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "usage: make_icons <svg> <output folder>\n");
        return 2;
    }
    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid()) {
        std::fprintf(stderr, "make_icons: %s is not a picture it can read\n", argv[1]);
        return 1;
    }
    const QString folder = QString::fromLocal8Bit(argv[2]);
    QList<QPair<int, QByteArray>> pictures;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        const QByteArray picture = png(render(renderer, size));
        if (!write(QStringLiteral("%1/chrono-vault-%2.png").arg(folder).arg(size), picture)) {
            return 1;
        }
        pictures.append({size, picture});
    }
    return write(folder + QStringLiteral("/chrono-vault.ico"), ico(pictures)) ? 0 : 1;
}
