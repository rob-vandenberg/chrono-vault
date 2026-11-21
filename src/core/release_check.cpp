// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
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

#include "release_check.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>

// --- Version ------------------------------------------------------------
// release_check.cpp 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: Initial release

namespace chrono_vault {

std::optional<Version> parseVersion(const QString& text)
{
    QString clean = text.trimmed();
    if (clean.startsWith(QLatin1Char('v'))) {
        clean.remove(0, 1);
    }
    const QStringList parts = clean.split(QLatin1Char('.'));
    if (parts.size() != 3) {
        return std::nullopt;
    }
    Version version;
    for (const QString& part : parts) {
        bool ok = false;
        const int number = part.toInt(&ok);
        if (!ok || number < 0 || part.isEmpty() || part.size() > 9) {
            return std::nullopt;
        }
        version.append(number);
    }
    return version;
}

QString versionText(const Version& version)
{
    QStringList parts;
    for (const int number : version) {
        parts << QString::number(number);
    }
    return parts.join(QLatin1Char('.'));
}

QString installerEnding(InstallKind kind)
{
    switch (kind) {
    case InstallKind::Deb: return QStringLiteral("_amd64.deb");
    case InstallKind::Rpm: return QStringLiteral("_x86_64.rpm");
    case InstallKind::Windows: return QStringLiteral("-setup.exe");
    case InstallKind::None: break;
    }
    return {};
}

namespace {

std::optional<ReleaseFile> readFile(const QJsonValue& value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    ReleaseFile file;
    file.name = object.value(QStringLiteral("name")).toString();
    file.address = object.value(QStringLiteral("browser_download_url")).toString();
    const QString digest = object.value(QStringLiteral("digest")).toString();
    const double size = object.value(QStringLiteral("size")).toDouble(-1);
    if (file.name.isEmpty() || !file.address.startsWith(QLatin1String("https://")) ||
        !digest.startsWith(QLatin1String("sha256:")) || size <= 0 || size > static_cast<double>(largestInstallerBytes)) {
        return std::nullopt;
    }
    file.sha256 = digest.mid(7);
    if (file.sha256.size() != 64 ||
        !std::all_of(file.sha256.cbegin(), file.sha256.cend(), [](QChar character) {
            return (character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
                   (character >= QLatin1Char('a') && character <= QLatin1Char('f'));
        })) {
        return std::nullopt;
    }
    file.size = static_cast<qint64>(size);
    return file;
}

} // namespace

QList<Release> parseReleases(const QByteArray& json)
{
    QList<Release> releases;
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray()) {
        return releases;
    }
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("draft")).toBool() || object.value(QStringLiteral("prerelease")).toBool()) {
            continue;
        }
        const auto version = parseVersion(object.value(QStringLiteral("tag_name")).toString());
        if (!version) {
            continue;
        }
        Release release;
        release.version = *version;
        release.page = object.value(QStringLiteral("html_url")).toString();
        for (const QJsonValue& asset : object.value(QStringLiteral("assets")).toArray()) {
            if (const auto file = readFile(asset)) {
                release.files.append(*file);
            }
        }
        releases.append(release);
    }
    return releases;
}

std::optional<Offer> newestUpdate(const QList<Release>& releases, InstallKind kind, const Version& running,
                                  const Version& skipped)
{
    const QString ending = installerEnding(kind);
    if (ending.isEmpty()) {
        return std::nullopt;
    }
    std::optional<Offer> best;
    for (const Release& release : releases) {
        if (release.version <= running || release.version == skipped) {
            continue;
        }
        for (const ReleaseFile& file : release.files) {
            if (file.name.startsWith(QLatin1String("chrono-vault")) && file.name.endsWith(ending)) {
                if (!best || release.version > best->release.version) {
                    best = Offer{release, file};
                }
                break;
            }
        }
    }
    return best;
}

bool isCheckDue(CheckFrequency frequency, const QDateTime& lastAnswered, const QDateTime& now)
{
    if (frequency == CheckFrequency::Never) {
        return false;
    }
    if (!lastAnswered.isValid() || lastAnswered > now) {
        return true;
    }
    switch (frequency) {
    case CheckFrequency::Daily: return lastAnswered.addDays(1) <= now;
    case CheckFrequency::Weekly: return lastAnswered.addDays(7) <= now;
    case CheckFrequency::Monthly: return lastAnswered.addMonths(1) <= now;
    case CheckFrequency::Never: break;
    }
    return false;
}

} // namespace chrono_vault
