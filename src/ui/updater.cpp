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

#include "updater.h"

#include "app_setup.h"

#include "version.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#endif

// --- Version ------------------------------------------------------------
// updater.cpp 0.0.12

// --- Version history ----------------------------------------------------
// v0.0.12: --reset: the check frequency not read at the start (D-026)
// v0.0.9: The windows of Chrono Mail: a found update offered, its problem, page and folder; Restart or Close
// v0.0.6: The default check frequency, for Restore defaults (D-022)
// v0.0.5: Initial release; a refused install or a broken-off download can be tried again

namespace chrono_vault {

namespace {

const QString releasesAddress = QStringLiteral("https://api.github.com/repos/rob-vandenberg/chrono-vault/releases?per_page=30");
const QString frequencyKey = QStringLiteral("updates/frequency");
constexpr int defaultFrequency = 0; // daily (D-021)
const QString lastAnsweredKey = QStringLiteral("updates/lastAnswered");
const QString skippedKey = QStringLiteral("updates/skipped");
constexpr qint64 largestAnswerBytes = 4 * 1024 * 1024;
constexpr int hourMilliseconds = 60 * 60 * 1000;
constexpr int firstCheckMilliseconds = 10 * 1000;
constexpr uint packageKitSuccess = 1;

const QString packageKit = QStringLiteral("org.freedesktop.PackageKit");
const QString linuxProgramFolder = QStringLiteral("/opt/chrono-vault");

// After an update on Linux: wait until this program has ended (at most two
// minutes), then start the new version.
const QString restartScript =
    QStringLiteral("i=0; while kill -0 \"$1\" 2>/dev/null; do i=$((i+1)); [ \"$i\" -gt 600 ] && exit 0; sleep 0.2; done; exec \"$2\"");

QNetworkRequest githubRequest(const QUrl& address)
{
    QNetworkRequest request(address);
    // Nothing about the person: the program's own name only.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Chrono-Vault/%1").arg(programVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(30000);
    return request;
}

} // namespace

Updater::Updater(QObject* parent) : QObject(parent)
{
    kind = detectInstallKind();
    // --reset: not read from the settings file at this start (D-026).
    checkFrequency = static_cast<CheckFrequency>(
        skipReadingSettings() ? defaultFrequency
                              : std::clamp(QSettings().value(frequencyKey, defaultFrequency).toInt(), 0, 3));
    hourly.setInterval(hourMilliseconds);
    connect(&hourly, &QTimer::timeout, this, [this]() { checkSoon(false); });
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &Updater::afterQuit);
    }
}

Updater::~Updater()
{
    if (reply) {
        reply->abort();
    }
}

QString Updater::newVersion() const
{
    return offer ? versionText(offer->release.version) : QString();
}

void Updater::setFrequency(int frequency)
{
    frequency = std::clamp(frequency, 0, 3);
    if (frequency != static_cast<int>(checkFrequency)) {
        checkFrequency = static_cast<CheckFrequency>(frequency);
        QSettings().setValue(frequencyKey, frequency);
        emit frequencyChanged();
    }
}

QString Updater::qtVersion() const
{
    return QStringLiteral(QT_VERSION_STR);
}

void Updater::startAutomaticChecks()
{
    QTimer::singleShot(firstCheckMilliseconds, this, [this]() { checkSoon(false); });
    hourly.start();
}

void Updater::checkNow()
{
    checkSoon(true);
}

void Updater::setState(State state, const QString& message, const QString& problem)
{
    currentState = state;
    currentMessage = message;
    currentProblem = problem;
    emit stateChanged();
}

void Updater::checkSoon(bool byHand)
{
    if (currentState == State::Checking || currentState == State::Downloading || currentState == State::Installing ||
        currentState == State::Downloaded || currentState == State::Installed) {
        return;
    }
    if (!byHand) {
        const QDateTime last = QSettings().value(lastAnsweredKey).toDateTime();
        if (kind == InstallKind::None || currentState == State::Available ||
            !isCheckDue(checkFrequency, last, QDateTime::currentDateTimeUtc())) {
            return;
        }
    } else if (kind == InstallKind::None) {
        setState(State::Failed, tr("This copy of Chrono Vault was not installed with an installer, so it cannot be "
                                   "updated through one. It is version %1.")
                                    .arg(programVersion()));
        return;
    }
    setState(State::Checking, byHand ? tr("Looking for a newer version…") : QString());
    QNetworkReply* answer = network.get(githubRequest(QUrl(releasesAddress)));
    reply = answer;
    connect(answer, &QNetworkReply::finished, this, [this, answer, byHand]() {
        answer->deleteLater();
        const QByteArray body = answer->read(largestAnswerBytes + 1);
        if (answer->error() != QNetworkReply::NoError || body.size() > largestAnswerBytes) {
            // Offline or GitHub not reachable: an automatic check says nothing
            // and is tried again later.
            setState(byHand ? State::Unreachable : State::Idle,
                     byHand ? tr("GitHub could not be reached. Try again later.") : QString());
            return;
        }
        answered(body, byHand);
    });
}

void Updater::answered(const QByteArray& body, bool byHand)
{
    QSettings settings;
    settings.setValue(lastAnsweredKey, QDateTime::currentDateTimeUtc());
    const auto running = parseVersion(programVersion()).value_or(Version{0, 0, 0});
    const auto skipped = parseVersion(settings.value(skippedKey).toString()).value_or(Version{});
    offer = newestUpdate(parseReleases(body), kind, running, byHand ? Version{} : skipped);
    if (offer) {
        setState(State::Available, tr("Chrono Vault %1 is available. You have version %2.")
                                       .arg(versionText(offer->release.version), programVersion()));
        emit offered(byHand);
    } else {
        setState(byHand ? State::Current : State::Idle,
                 byHand ? tr("You have the newest version, %1.").arg(programVersion()) : QString());
    }
}

void Updater::answerForTests(const QByteArray& releasesJson, bool byHand)
{
    answered(releasesJson, byHand);
}

void Updater::skipThisVersion()
{
    if (offer) {
        QSettings().setValue(skippedKey, versionText(offer->release.version));
    }
    offer.reset();
    setState(State::Idle, QString());
}

void Updater::download()
{
    if (!offer || currentState == State::Downloading) {
        return;
    }
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    downloadedPath = QDir(folder).filePath(offer->file.name);
    partFile.setFileName(downloadedPath + QStringLiteral(".part"));
    if (!partFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString problem = tr("The update could not be saved in %1. Choose Download to try again.")
                                    .arg(QDir::toNativeSeparators(folder));
        setState(State::Available, problem, problem);
        return;
    }
    hasher.reset();
    brokenOff = false;
    downloadProgress = 0;
    emit progressChanged();
    setState(State::Downloading, tr("Downloading Chrono Vault %1…").arg(versionText(offer->release.version)));

    QNetworkReply* file = network.get(githubRequest(QUrl(offer->file.address)));
    reply = file;
    connect(file, &QNetworkReply::readyRead, this, [this, file]() {
        const QByteArray chunk = file->readAll();
        if (partFile.pos() + chunk.size() > offer->file.size || partFile.write(chunk) != chunk.size()) {
            brokenOff = true; // larger than GitHub said, or the disk is full
            file->abort();
            return;
        }
        hasher.addData(chunk);
        downloadProgress = static_cast<double>(partFile.pos()) / static_cast<double>(offer->file.size);
        emit progressChanged();
    });
    connect(file, &QNetworkReply::finished, this, &Updater::finishDownload);
}

void Updater::stopDownload()
{
    if (reply && currentState == State::Downloading) {
        reply->abort();
    }
}

void Updater::finishDownload()
{
    auto* file = qobject_cast<QNetworkReply*>(sender());
    if (file) {
        file->deleteLater();
    }
    const bool stopped = file && file->error() == QNetworkReply::OperationCanceledError && !brokenOff;
    const bool complete = file && file->error() == QNetworkReply::NoError && partFile.pos() == offer->file.size;
    partFile.close();
    if (!complete) {
        QFile::remove(partFile.fileName());
        // The offer stays: Download can be tried again.
        const QString problem =
            stopped ? QString()
                    : brokenOff ? tr("The download did not complete: the update could not be written, so check the free "
                                     "space in your Downloads folder. Nothing was installed. Choose Download to try again.")
                                : tr("The download did not complete. Nothing was installed. Choose Download to try again.");
        setState(State::Available,
                 stopped ? tr("The download was stopped. Chrono Vault %1 is available.").arg(newVersion()) : problem,
                 problem);
        return;
    }
    if (QString::fromLatin1(hasher.result().toHex()) != offer->file.sha256) {
        QFile::remove(partFile.fileName());
        const QString problem = tr("The downloaded file does not match GitHub's fingerprint, so it was not kept and "
                                   "nothing was installed. Choose Download to try again.");
        setState(State::Available, problem, problem);
        return;
    }
    QFile::remove(downloadedPath);
    if (!QFile::rename(partFile.fileName(), downloadedPath)) {
        QFile::remove(partFile.fileName());
        const QString problem = tr("The update could not be saved. Nothing was installed. Choose Download to try again.");
        setState(State::Available, problem, problem);
        return;
    }
    downloadProgress = 1;
    emit progressChanged();
    setState(State::Downloaded,
             tr("The update to Chrono Vault %1 was downloaded and checked: it is complete and unchanged.").arg(newVersion()));
}

void Updater::install()
{
    if (currentState != State::Downloaded || !offer) {
        return;
    }
    // Still exactly the file that was checked.
    if (fingerprintOf(downloadedPath) != offer->file.sha256) {
        const QString problem = tr("The update has changed since it was checked, so it was not installed.");
        setState(State::Failed, problem, problem);
        return;
    }
    if (kind == InstallKind::Windows) {
        setupToStart = downloadedPath;
        setState(State::Installed, tr("The setup of Chrono Vault %1 starts as soon as Chrono Vault has closed, and "
                                      "starts the new version when it is done.")
                                       .arg(newVersion()));
        emit quitRequested();
        return;
    }
#ifdef Q_OS_LINUX
    QDBusInterface service(packageKit, QStringLiteral("/org/freedesktop/PackageKit"), packageKit,
                           QDBusConnection::systemBus());
    const QDBusReply<QDBusObjectPath> transaction = service.call(QStringLiteral("CreateTransaction"));
    if (!transaction.isValid()) {
        // The checked file stays: Install can be tried again without a new download.
        const QString problem = tr("This system's software installer could not be reached: %1. Choose Install to "
                                   "try again.")
                                    .arg(transaction.error().message());
        setState(State::Downloaded, problem, problem);
        return;
    }
    const QString path = transaction.value().path();
    watchPackageKit(path);
    QDBusInterface work(packageKit, path, packageKit + QStringLiteral(".Transaction"), QDBusConnection::systemBus());
    work.call(QStringLiteral("SetHints"), QStringList{QStringLiteral("interactive=true")});
    packageKitDetails.clear();
    setState(State::Installing, tr("Installing the update… your system may ask for its password."));
    // The installer reports the outcome with its Finished signal; a request it
    // refuses outright is answered with an error instead, which ends it too.
    auto* call = new QDBusPendingCallWatcher(
        work.asyncCall(QStringLiteral("InstallFiles"), quint64(0), QStringList{downloadedPath}), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* answer) {
        answer->deleteLater();
        const QDBusPendingReply<> reply = *answer;
        if (reply.isError()) {
            installedOnLinux(false, reply.error().message());
        }
    });
#else
    setState(State::Failed, tr("This system cannot install the update."));
#endif
}

void Updater::watchPackageKit(const QString& transactionPath)
{
#ifdef Q_OS_LINUX
    const QString interface = packageKit + QStringLiteral(".Transaction");
    QDBusConnection::systemBus().connect(packageKit, transactionPath, interface, QStringLiteral("Finished"), this,
                                         SLOT(packageKitFinished(uint, uint)));
    QDBusConnection::systemBus().connect(packageKit, transactionPath, interface, QStringLiteral("ErrorCode"), this,
                                         SLOT(packageKitError(uint, QString)));
#else
    Q_UNUSED(transactionPath);
#endif
}

void Updater::packageKitError(uint code, const QString& details)
{
    Q_UNUSED(code);
    packageKitDetails = details;
}

void Updater::packageKitFinished(uint exit, uint runtime)
{
    Q_UNUSED(runtime);
    installedOnLinux(exit == packageKitSuccess, packageKitDetails);
}

void Updater::installedOnLinux(bool succeeded, const QString& details)
{
    if (currentState != State::Installing) {
        return;
    }
    if (!succeeded) {
        // A cancelled password prompt is a normal event: the checked file stays,
        // and Install can be tried again without a new download.
        const QString problem =
            details.isEmpty()
                ? tr("The update was not installed. It is still in its folder; choose Install to try again.")
                : tr("The update was not installed: %1. It is still in its folder; choose Install to try again.").arg(details);
        setState(State::Downloaded, problem, problem);
        return;
    }
    // As in Chrono Mail: the person chooses Restart or Close; Chrono Vault
    // closes either way, its files being the new version's now.
    restartChoicePending = true;
    setState(State::Installed, tr("Chrono Vault was updated to %1. Restart it to use the new version.").arg(newVersion()));
}

void Updater::finishUpdate(bool restart)
{
    if (!restartChoicePending) {
        return;
    }
    restartChoicePending = false;
    restartAfterQuit = restart;
    finishing = true;
    emit stateChanged();
    emit quitRequested();
}

void Updater::quitHeldUp()
{
    // The quit after Restart or Close was held up or cancelled: Restart or
    // Close is asked again, the new version still waiting for it (D-024).
    if (finishing) {
        finishing = false;
        restartAfterQuit = false;
        restartChoicePending = true;
        emit stateChanged();
        return;
    }
    if (!setupToStart.isEmpty()) {
        setupToStart.clear();
        const QString problem = tr("The update was not installed, because Chrono Vault did not close. Choose "
                                   "Install again when you are ready.");
        setState(State::Downloaded, problem, problem);
    } else if (restartAfterQuit) {
        restartAfterQuit = false;
        setState(State::Installed, tr("Chrono Vault %1 is installed and runs the next time you start Chrono Vault.")
                                       .arg(newVersion()));
    }
}

void Updater::afterQuit()
{
    if (!setupToStart.isEmpty()) {
        QProcess::startDetached(setupToStart, {QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
                                               QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS")});
    }
    if (restartAfterQuit) {
        QProcess::startDetached(QStringLiteral("/bin/sh"),
                                {QStringLiteral("-c"), restartScript, QStringLiteral("chrono-vault-restart"),
                                 QString::number(QCoreApplication::applicationPid()),
                                 linuxProgramFolder + QStringLiteral("/chrono-vault")});
    }
}

InstallKind Updater::installKindFrom(const QString& programFolder, bool debRecordExists, bool rpmRecordExists,
                                     bool windowsUninstallerExists)
{
#ifdef Q_OS_WIN
    Q_UNUSED(programFolder);
    Q_UNUSED(debRecordExists);
    Q_UNUSED(rpmRecordExists);
    return windowsUninstallerExists ? InstallKind::Windows : InstallKind::None;
#else
    Q_UNUSED(windowsUninstallerExists);
    // Only the packaged program, never a build in a project folder.
    if (QDir::cleanPath(programFolder) != linuxProgramFolder) {
        return InstallKind::None;
    }
    if (debRecordExists) {
        return InstallKind::Deb;
    }
    return rpmRecordExists ? InstallKind::Rpm : InstallKind::None;
#endif
}

InstallKind Updater::detectInstallKind()
{
    const QString folder = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    return installKindFrom(folder, false, false, QFileInfo::exists(folder + QStringLiteral("/unins000.exe")));
#else
    if (QDir::cleanPath(folder) != linuxProgramFolder) {
        return InstallKind::None;
    }
    const bool deb = QFileInfo::exists(QStringLiteral("/var/lib/dpkg/info/chrono-vault.list"));
    bool rpm = false;
    if (!deb) {
        QProcess query;
        query.start(QStringLiteral("rpm"), {QStringLiteral("-q"), QStringLiteral("--quiet"), QStringLiteral("chrono-vault")});
        rpm = query.waitForFinished(5000) && query.exitStatus() == QProcess::NormalExit && query.exitCode() == 0;
    }
    return installKindFrom(folder, deb, rpm, false);
#endif
}

QString Updater::fingerprintOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString Updater::releasePage() const
{
    // Only a page of Chrono Vault's own releases, whatever the answer held.
    const QString page = offer ? offer->release.page : QString();
    return page.startsWith(QStringLiteral("https://github.com/rob-vandenberg/chrono-vault/releases/")) ? page : QString();
}

QUrl Updater::downloadFolder() const
{
    return downloadedPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(QFileInfo(downloadedPath).absolutePath());
}

QString Updater::downloadFolderName() const
{
    return downloadedPath.isEmpty() ? QString() : QDir::toNativeSeparators(QFileInfo(downloadedPath).absolutePath());
}

int Updater::defaultCheckFrequency() const
{
    return defaultFrequency;
}

} // namespace chrono_vault
