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
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <optional>

// --- Version ------------------------------------------------------------
// icon_library.cpp 0.2.56

// --- Version history ----------------------------------------------------
// v0.2.56: Folders inside folders; "-" lines; shipped lines added to the person's icons.md (the owner, D-050)
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

// One line of icons.md: "folder: Heading", the folder a path inside the
// icons folder ("symbols/uppercase"). A line starting with "-" names a folder
// the person took out: never shown, never added again (the owner).
struct IndexLine
{
    QString folder;
    QString heading;
    bool removed = false;
};

std::optional<IndexLine> readIndexLine(const QString& line)
{
    QString text = line.trimmed();
    IndexLine read;
    if (text.startsWith(QLatin1Char('-'))) {
        read.removed = true;
        text = text.mid(1).trimmed();
    }
    const qsizetype colon = text.indexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return std::nullopt;
    }
    read.folder = text.left(colon).trimmed();
    read.heading = text.mid(colon + 1).trimmed();
    // A path inside the icons folder, nothing else: no "..", no ".", no
    // empty part, no backslash, not from the root.
    if (read.folder.isEmpty() || read.heading.isEmpty() || read.folder.contains(QLatin1Char('\\'))) {
        return std::nullopt;
    }
    for (const QString& part : read.folder.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part.startsWith(QLatin1Char('.'))) {
            return std::nullopt;
        }
    }
    return read;
}

QStringList readIndex(const QString& folder)
{
    QFile index(folder + QStringLiteral("/icons.md"));
    if (index.size() > largestIndexBytes || !index.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(index.readAll()).split(QLatin1Char('\n'));
}

// A folder that ships and that the person's icons.md does not name at all,
// shown or taken out, is added at its end, so new icons appear after an
// update; the person's own lines stay as they are (the owner).
bool addShippedLines(const QString& installed, const QString& personal)
{
    const QStringList shipped = readIndex(installed);
    const QStringList own = readIndex(personal);
    if (shipped.isEmpty() || own.isEmpty()) {
        return true; // nothing to add, or the whole file was just copied
    }
    QSet<QString> named;
    for (const QString& line : own) {
        if (const auto read = readIndexLine(line)) {
            named.insert(read->folder);
        }
    }
    QStringList added;
    for (const QString& line : shipped) {
        const auto read = readIndexLine(line);
        if (read && !read->removed && !named.contains(read->folder)) {
            added << read->folder + QStringLiteral(": ") + read->heading;
            named.insert(read->folder);
        }
    }
    if (added.isEmpty()) {
        return true;
    }
    QStringList whole = own;
    while (!whole.isEmpty() && whole.constLast().trimmed().isEmpty()) {
        whole.removeLast();
    }
    whole << added;
    QSaveFile file(personal + QStringLiteral("/icons.md"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write((whole.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
    return file.commit();
}
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
    return addShippedLines(from.path(), to) && allCopied;
}

QList<IconSection> readIconSections(const QString& folder)
{
    QList<IconSection> sections;
    for (const QString& line : readIndex(folder)) {
        const auto read = readIndexLine(line);
        if (!read || read->removed) {
            continue;
        }
        IconSection section;
        section.folder = read->folder;
        section.heading = read->heading;
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
