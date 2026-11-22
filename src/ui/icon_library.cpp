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

#include "icon_library.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

// --- Version ------------------------------------------------------------
// icon_library.cpp 0.0.40

// --- Version history ----------------------------------------------------
// v0.0.40: Initial release (D-050)

namespace chrono_vault {

namespace {
QString& installedForTests()
{
    static QString folder;
    return folder;
}
QString& personalForTests()
{
    static QString folder;
    return folder;
}
// The pictures the picker shows; what readIcon reads (D-018).
const QStringList pictureSuffixes{QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("jpg"),
                                  QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("ico")};
// icons.md is small; a larger file is not read.
constexpr qint64 largestIndexBytes = 64 * 1024;
} // namespace

QString personalIconsFolder()
{
    if (!personalForTests().isEmpty()) {
        return personalForTests();
    }
    // GenericDataLocation is ~/.local/share on Linux and the local (not
    // roaming) AppData on Windows.
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/chrono-vault/icons");
}

QString installedIconsFolder()
{
    if (!installedForTests().isEmpty()) {
        return installedForTests();
    }
    return QCoreApplication::applicationDirPath() + QStringLiteral("/icons");
}

void setIconsFoldersForTests(const QString& installed, const QString& personal)
{
    installedForTests() = installed;
    personalForTests() = personal;
}

bool copyMissingIcons()
{
    const QDir from(installedIconsFolder());
    if (!from.exists()) {
        return true; // nothing installed (a build run from its folder)
    }
    const QString to = personalIconsFolder();
    bool allCopied = QDir().mkpath(to);
    QDirIterator files(from.path(), QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source = files.next();
        const QString target = to + QLatin1Char('/') + from.relativeFilePath(source);
        if (QFileInfo::exists(target)) {
            continue; // never written over: the person's own stays
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(source, target)) {
            allCopied = false;
            continue;
        }
        // A copy from a read-only install is read-only too; the person's
        // copy is theirs to change.
        QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner);
    }
    return allCopied;
}

QList<IconSection> readIconSections(const QString& folder)
{
    QList<IconSection> sections;
    QFile index(folder + QStringLiteral("/icons.md"));
    if (index.size() > largestIndexBytes || !index.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return sections;
    }
    const QStringList lines = QString::fromUtf8(index.readAll()).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        IconSection section;
        section.folder = line.left(colon).trimmed();
        section.heading = line.mid(colon + 1).trimmed();
        // A plain folder name inside the icons folder, nothing else.
        if (section.folder.isEmpty() || section.heading.isEmpty() || section.folder.contains(QLatin1Char('/')) ||
            section.folder.contains(QLatin1Char('\\')) || section.folder.startsWith(QLatin1Char('.'))) {
            continue;
        }
        const QDir dir(folder + QLatin1Char('/') + section.folder);
        if (!dir.exists()) {
            continue;
        }
        QStringList filters;
        for (const QString& suffix : pictureSuffixes) {
            filters << QStringLiteral("*.") + suffix << QStringLiteral("*.") + suffix.toUpper();
        }
        for (const QString& name : dir.entryList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
            section.files << dir.filePath(name);
        }
        sections.append(section);
    }
    return sections;
}

} // namespace chrono_vault
