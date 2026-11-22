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

#include "release_check.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QNetworkReply;

// --- Version ------------------------------------------------------------
// updater.h 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: The windows of Chrono Mail: a found update offered, its problem, page and folder; Restart or Close
// v0.0.6: The default check frequency, for Restore defaults (D-022)
// v0.0.5: Initial release

namespace chrono_vault {

// Updates as Chrono Mail does (D-021): it knows how this copy was installed,
// looks on GitHub for a newer release carrying that installer (by itself as
// the setting says, or when asked), downloads it into the Downloads folder,
// checks it against GitHub's SHA-256 fingerprint, installs it through that
// installer, and starts the new version once this one has ended.
class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QString newVersion READ newVersion NOTIFY stateChanged)
    Q_PROPERTY(bool canUpdate READ canUpdate CONSTANT)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    // What went wrong with the last download or install, for the window that
    // offers it again; empty otherwise.
    Q_PROPERTY(QString problem READ problem NOTIFY stateChanged)
    // The new version's page on GitHub ("What is new"); empty when the answer
    // held no page of Chrono Vault's own releases.
    Q_PROPERTY(QString releasePage READ releasePage NOTIFY stateChanged)
    // The folder the update was saved in.
    Q_PROPERTY(QUrl downloadFolder READ downloadFolder NOTIFY stateChanged)
    // The same folder as the system writes it, with its own separators.
    Q_PROPERTY(QString downloadFolderName READ downloadFolderName NOTIFY stateChanged)
    // Linux: installed, waiting for the person's Restart or Close.
    Q_PROPERTY(bool waitsForRestartChoice READ waitsForRestartChoice NOTIFY stateChanged)

public:
    enum class State { Idle, Checking, Current, Available, Unreachable, Downloading, Downloaded, Installing, Installed, Failed };
    Q_ENUM(State)

    explicit Updater(QObject* parent = nullptr);
    ~Updater() override;

    [[nodiscard]] State state() const { return currentState; }
    [[nodiscard]] QString message() const { return currentMessage; }
    [[nodiscard]] QString newVersion() const;
    [[nodiscard]] bool canUpdate() const { return kind != InstallKind::None; }
    [[nodiscard]] double progress() const { return downloadProgress; }
    // 0 daily, 1 weekly, 2 monthly, 3 never.
    [[nodiscard]] int frequency() const { return static_cast<int>(checkFrequency); }
    void setFrequency(int frequency);
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString problem() const { return currentProblem; }
    [[nodiscard]] QString releasePage() const;
    [[nodiscard]] QUrl downloadFolder() const;
    [[nodiscard]] QString downloadFolderName() const;
    [[nodiscard]] bool waitsForRestartChoice() const { return restartChoicePending; }

    // The automatic checks: one shortly after the start when one is due, and
    // a look every hour while Chrono Vault runs.
    void startAutomaticChecks();

    // Looks on GitHub now (About's Check for update), whatever the setting.
    Q_INVOKABLE void checkNow();
    Q_INVOKABLE void skipThisVersion();
    Q_INVOKABLE void download();
    Q_INVOKABLE void stopDownload();
    // Installs the downloaded update; the program then asks to quit, and the
    // new version starts once this one has ended.
    Q_INVOKABLE void install();
    // The quit an install asked for was held up or cancelled (unsaved work):
    // nothing is set off at a later, ordinary quit.
    Q_INVOKABLE void quitHeldUp();
    // Linux, after the install: Chrono Vault closes either way; with restart
    // the new version starts once it has ended.
    Q_INVOKABLE void finishUpdate(bool restart);
    // The check frequency before the person changes it (Restore defaults, D-022).
    [[nodiscard]] Q_INVOKABLE int defaultCheckFrequency() const;

    // How this copy was installed, from where the program runs and what the
    // system's package records say.
    [[nodiscard]] static InstallKind detectInstallKind();
    [[nodiscard]] static InstallKind installKindFrom(const QString& programFolder, bool debRecordExists,
                                                     bool rpmRecordExists, bool windowsUninstallerExists);
    // The SHA-256 fingerprint of a file, in lower-case hexadecimal; empty when
    // it cannot be read.
    [[nodiscard]] static QString fingerprintOf(const QString& path);

    // Tests only: GitHub's answer instead of a request, and how this copy
    // counts as installed.
    void answerForTests(const QByteArray& releasesJson, bool byHand = true);
    // Tests only: the offered update counts as downloaded to this file.
    void downloadedForTests(const QString& path)
    {
        downloadedPath = path;
        setState(State::Downloaded, QString());
    }
    void setInstallKindForTests(InstallKind installKind) { kind = installKind; }
    // Tests only: an installed update waits for the quit; and whether one does.
    void armRestartForTests() { restartAfterQuit = true; }
    [[nodiscard]] bool armedForTests() const { return restartAfterQuit || !setupToStart.isEmpty(); }
    // Tests only: the system's installer answers an install that is running.
    void installAnsweredForTests(bool succeeded, const QString& details)
    {
        currentState = State::Installing;
        installedOnLinux(succeeded, details);
    }

signals:
    void stateChanged();
    void progressChanged();
    void frequencyChanged();
    // A newer version was found: by an automatic check, or by hand (About).
    void offered(bool byHand);
    // The update is installed or handed to the setup: the program should quit
    // (asking about unsaved work first), and the rest follows.
    void quitRequested();

private:
    // A problem, when given, is what went wrong with a download or install.
    void setState(State state, const QString& message, const QString& problem = {});
    void checkSoon(bool byHand);
    void answered(const QByteArray& body, bool byHand);
    void finishDownload();
    void installedOnLinux(bool succeeded, const QString& details);
    void afterQuit();
    void watchPackageKit(const QString& transactionPath);

private slots:
    void packageKitFinished(uint exit, uint runtime);
    void packageKitError(uint code, const QString& details);

private:
    InstallKind kind = InstallKind::None;
    CheckFrequency checkFrequency = CheckFrequency::Daily;
    State currentState = State::Idle;
    QString currentMessage;
    std::optional<Offer> offer;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    QFile partFile;
    QCryptographicHash hasher{QCryptographicHash::Sha256};
    QString downloadedPath;
    double downloadProgress = 0;
    QTimer hourly;
    QString packageKitDetails;
    QString setupToStart;     // Windows: the setup, started once this program has ended
    bool restartAfterQuit = false; // Linux: start the new version once this one has ended
    bool brokenOff = false;        // the download was broken off by the program, not by Stop
    QString currentProblem;
    bool restartChoicePending = false;
    bool finishing = false; // Restart or Close was chosen; the quit is under way
};

} // namespace chrono_vault
