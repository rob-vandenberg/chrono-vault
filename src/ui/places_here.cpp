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
#include "places_here.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLayout>
#include <QListView>
#include <QSet>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyle>
#include <QXmlStreamReader>

// --- Version ------------------------------------------------------------
// places_here.cpp 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Initial release, after Chrono Viewer's (D-055)

namespace chrono_vault::places_here {

namespace {

// A list of places is small: a larger file is damaged or no such list (OR-4).
constexpr qint64 largestPlacesFile = 1024 * 1024;
// Past this many places nobody finds anything in a sidebar.
constexpr int mostPlaces = 60;

QList<QUrl> usualPlaces()
{
    QList<QUrl> places;
    for (const auto which : {QStandardPaths::HomeLocation, QStandardPaths::DesktopLocation,
                             QStandardPaths::DocumentsLocation, QStandardPaths::DownloadLocation,
                             QStandardPaths::MusicLocation, QStandardPaths::PicturesLocation,
                             QStandardPaths::MoviesLocation}) {
        const QString folder = QStandardPaths::writableLocation(which);
        if (!folder.isEmpty()) {
            places << QUrl::fromLocalFile(folder);
        }
    }
    return places;
}

// The same place written two ways is one place.
QList<QUrl> eachOnce(const QList<QUrl>& places)
{
    QList<QUrl> once;
    QSet<QString> seen;
    for (const QUrl& place : places) {
        QString path = place.toLocalFile();
        if (path.isEmpty() && place != QUrl(QStringLiteral("file:"))) {
            continue;
        }
        if (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
            path.chop(1);
        }
        if (seen.contains(path)) {
            continue;
        }
        seen.insert(path);
        once << (path.isEmpty() ? place : QUrl::fromLocalFile(path));
        if (once.size() >= mostPlaces) {
            break;
        }
    }
    return once;
}

} // namespace

QString whereTheDesktopKeepsItsPlaces()
{
#ifdef Q_OS_WIN
    return {};
#else
    // The file every file manager of the freedesktop standard reads and
    // writes: what stands in it is what the person sees beside their folders.
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return data.isEmpty() ? QString() : data + QStringLiteral("/user-places.xbel");
#endif
}

QList<QUrl> placesInFile(const QString& file)
{
    QList<QUrl> places;
    QFile list(file);
    if (file.isEmpty() || !list.exists() || list.size() > largestPlacesFile ||
        !list.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return places;
    }
    QXmlStreamReader reading(&list);
    while (!reading.atEnd()) {
        if (reading.readNext() != QXmlStreamReader::StartElement || reading.name() != QLatin1String("bookmark")) {
            continue;
        }
        const QString where = reading.attributes().value(QLatin1String("href")).toString();
        // Whether the person hid it is written among the place's own notes,
        // so the whole bookmark is read first.
        bool hidden = false;
        int depth = 1;
        while (depth > 0 && !reading.atEnd()) {
            switch (reading.readNext()) {
            case QXmlStreamReader::StartElement:
                ++depth;
                if (reading.name() == QLatin1String("IsHidden") &&
                    reading.readElementText().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
                    hidden = true;
                    --depth; // readElementText read this element's end too
                }
                break;
            case QXmlStreamReader::EndElement:
                --depth;
                break;
            default:
                break;
            }
        }
        const QUrl place(where);
        // Only a folder on this machine is something a file dialog can hand
        // back; a cloud drive mounted here is one.
        if (hidden || !place.isLocalFile()) {
            continue;
        }
        places << place;
        if (places.size() >= mostPlaces) {
            break;
        }
    }
    return eachOnce(places);
}

QList<QUrl> everyPlace()
{
#ifdef Q_OS_WIN
    QList<QUrl> places{QUrl(QStringLiteral("file:"))}; // This PC, as Qt's dialog names the whole machine
    places += usualPlaces();
    for (const QFileInfo& drive : QDir::drives()) {
        places << QUrl::fromLocalFile(drive.absoluteFilePath());
    }
    return eachOnce(places);
#else
    QList<QUrl> places{QUrl::fromLocalFile(QStringLiteral("/"))};
    const QList<QUrl> theirs = placesInFile(whereTheDesktopKeepsItsPlaces());
    places += theirs.isEmpty() ? usualPlaces() : theirs;
    return eachOnce(places);
#endif
}

void showEveryPlaceWhole(QFileDialog& dialog)
{
    auto* places = dialog.findChild<QListView*>(QStringLiteral("sidebar"));
    auto* divider = dialog.findChild<QSplitter*>(QStringLiteral("splitter"));
    if (places == nullptr || divider == nullptr || divider->count() < 2) {
        return;
    }
    if (QLayout* whole = dialog.layout()) {
        whole->activate();
    }
    const int room = divider->width();
    const int widest = places->sizeHintForColumn(0) + 2 * places->frameWidth() +
                       places->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, places);
    const int wanted = qMin(widest, room / 3);
    if (room > 0 && wanted > 0) {
        divider->setSizes({wanted, room - wanted});
    }
}

} // namespace chrono_vault::places_here
