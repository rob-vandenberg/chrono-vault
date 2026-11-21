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

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

// --- Version ------------------------------------------------------------
// release_check.h 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: Initial release

// What the update check decides (D-021), with no network of its own: GitHub's
// answer is read, and the newest release above this version that carries this
// computer's installer is found.
namespace chrono_vault {

using Version = QList<int>;

// "1.2.3" (a leading "v" allowed); nothing when it is not three numbers.
[[nodiscard]] std::optional<Version> parseVersion(const QString& text);
[[nodiscard]] QString versionText(const Version& version);

// How this copy was installed; it is updated by that installer only.
enum class InstallKind { None, Deb, Rpm, Windows };

// The ending of this installer's file in a release.
[[nodiscard]] QString installerEnding(InstallKind kind);

struct ReleaseFile
{
    QString name;
    QString address;   // where it is downloaded from
    QString sha256;    // GitHub's fingerprint, 64 lower-case hexadecimal characters
    qint64 size = 0;
};

struct Release
{
    Version version;
    QString page; // the release's page on GitHub
    QList<ReleaseFile> files;
};

inline constexpr qint64 largestInstallerBytes = 1024LL * 1024 * 1024;

// GitHub's list of releases. Anything of the wrong shape is left out, never
// guessed at (OR-4); a file without a SHA-256 fingerprint is left out too.
[[nodiscard]] QList<Release> parseReleases(const QByteArray& json);

struct Offer
{
    Release release;
    ReleaseFile file;
};

// The newest release above the running version that carries this computer's
// installer, unless that version was skipped.
[[nodiscard]] std::optional<Offer> newestUpdate(const QList<Release>& releases, InstallKind kind,
                                                const Version& running, const Version& skipped);

enum class CheckFrequency { Daily, Weekly, Monthly, Never };

// Whether an automatic check is due: never when it is off, always when no
// check was ever answered.
[[nodiscard]] bool isCheckDue(CheckFrequency frequency, const QDateTime& lastAnswered, const QDateTime& now);

} // namespace chrono_vault
