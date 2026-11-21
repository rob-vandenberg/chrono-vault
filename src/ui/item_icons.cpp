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

#include "item_icons.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>

// --- Version ------------------------------------------------------------
// item_icons.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

namespace chrono_vault {

namespace {

const QStringList supportedFormats = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                                      QStringLiteral("bmp"), QStringLiteral("ico"), QStringLiteral("svg")};

// The largest picture that is decoded at all, so a damaged or hostile file
// never asks for gigabytes of memory (OR-4).
constexpr int largestSide = 8192;

} // namespace

IconResult readIcon(const QString& path)
{
    IconResult result;
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) {
        result.status = IconStatus::NotFound;
        return result;
    }
    if (file.size() > largestIconFileBytes) {
        result.status = IconStatus::TooLarge;
        return result;
    }
    if (!supportedFormats.contains(file.suffix().toLower())) {
        result.status = IconStatus::NotSupported;
        return result;
    }

    QImageReader reader(path);
    reader.setDecideFormatFromContent(true);
    const QByteArray format = reader.format().toLower();
    const bool isVector = format == "svg" || format == "svgz";
    const QSize size = reader.size();
    if (!reader.canRead() || (!isVector && (size.width() > largestSide || size.height() > largestSide))) {
        result.status = IconStatus::NotAPicture;
        return result;
    }
    if (isVector || (size.isValid() && (size.width() > iconSide || size.height() > iconSide))) {
        QSize scaled = size.isValid() ? size : QSize(iconSide, iconSide);
        scaled.scale(iconSide, iconSide, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled);
    }
    const QImage picture = reader.read();
    if (picture.isNull()) {
        result.status = IconStatus::NotAPicture;
        return result;
    }

    // Centred on a clear square, never stretched.
    const QImage fitted = picture.width() > iconSide || picture.height() > iconSide
                              ? picture.scaled(iconSide, iconSide, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                              : picture;
    QImage square(iconSide, iconSide, QImage::Format_ARGB32_Premultiplied);
    square.fill(Qt::transparent);
    {
        QPainter painter(&square);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage((iconSide - fitted.width()) / 2, (iconSide - fitted.height()) / 2, fitted);
    }
    QBuffer buffer(&result.png);
    buffer.open(QIODevice::WriteOnly);
    if (!square.save(&buffer, "PNG")) {
        result.png.clear();
        result.status = IconStatus::NotAPicture;
        return result;
    }
    result.status = IconStatus::Read;
    return result;
}

QString iconStatusText(IconStatus status)
{
    switch (status) {
    case IconStatus::Read: return {};
    case IconStatus::NotFound:
        return QCoreApplication::translate("ItemIcons", "That picture could not be found.");
    case IconStatus::TooLarge:
        return QCoreApplication::translate("ItemIcons", "That picture is larger than 5 MB. Choose a smaller one.");
    case IconStatus::NotAPicture:
        return QCoreApplication::translate("ItemIcons", "That file could not be read as a picture.");
    case IconStatus::NotSupported:
        return QCoreApplication::translate("ItemIcons", "Choose a JPG, PNG, BMP, ICO or SVG picture.");
    }
    return {};
}

} // namespace chrono_vault
