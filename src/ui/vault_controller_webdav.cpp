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


#include "vault_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller_webdav.cpp 0.8.115

// --- Version history ----------------------------------------------------
// v0.8.115: Create and open shared by every storage; a Google sign-in renewed in the browser (D-080)
// v0.8.114: The sync of a vault in any cloud storage, CloudSync; vaultPlace names the storage (D-080)
// v0.5.81: Retry sync answered by the sync it asked for (D-061)
// v0.5.75: Disconnect deletes nothing; moving to a server is the vault menu's Move (D-060)
// v0.3.63: A folder's entries with their size and time of change, for the file dialog (D-057)
// v0.3.62: The folder browser and New folder; an address read as typed; a non-WebDAV server said in words (D-057)
// v0.3.60: Initial release: a vault kept on a WebDAV server (D-057)

// The controller's WebDAV part (D-057). A vault kept on a server has a
// working copy on this device, which every unlock, save and merge uses as
// a local vault does; CloudSync keeps it the same as the storage's.

namespace chrono_vault {

namespace {

constexpr int syncEveryMs = 10 * 60 * 1000; // every 10 minutes on a computer (ref)
constexpr int syncAfterSaveMs = 5000;       // within 5 seconds of a save (ref)
const QString vaultSuffix = QStringLiteral(".chronovault");

} // namespace

QString VaultController::shownFingerprint(const QByteArray& fingerprint)
{
    QString shown;
    const QString hex = QString::fromLatin1(fingerprint).toUpper();
    for (qsizetype index = 0; index < hex.size(); index += 2) {
        if (!shown.isEmpty()) {
            shown += QLatin1Char(':');
        }
        shown += hex.mid(index, 2);
    }
    return shown;
}

bool VaultController::isPlainHttp(const QString& address) const
{
    return address.trimmed().startsWith(QLatin1String("http://"), Qt::CaseInsensitive);
}

QString VaultController::connectProblem(const WebDavReply& answer)
{
    switch (answer.error) {
    case WebDavError::None:
        return QString();
    case WebDavError::NoConnection:
        return tr("The server cannot be reached. Check the address and the network.");
    case WebDavError::Unauthorized:
        return tr("The server refused the user name or password.");
    case WebDavError::NotFound:
        return tr("That folder is not on the server. Create it there first, or check the address.");
    case WebDavError::Certificate:
        return tr("The server's certificate is not trusted by this system.");
    case WebDavError::TooLarge:
        return tr("The file is too large to be a vault.");
    case WebDavError::Damaged:
        return tr("The address does not answer as a WebDAV folder.");
    case WebDavError::NotSupported:
        return tr("This address is not a WebDAV server. Check the address.");
    case WebDavError::Changed:
    case WebDavError::Locked:
    case WebDavError::Server:
    case WebDavError::Superseded:
        break;
    }
    if (answer.status >= 300 && answer.status < 400) {
        return tr("The server sends this address elsewhere (%1). Check the address, for example https:// in place of "
                  "http://.")
            .arg(answer.status);
    }
    return tr("The server refused it (%1).").arg(answer.status);
}

void VaultController::startWebDav()
{
    stopWebDav();
    const auto place = cloudPlaceOf(path);
    if (!place || currentState != State::Unlocked) {
        emit syncChanged();
        return;
    }
    cloudKindNow = place->kind;
    if (webDavPasswords.contains(place->id)) {
        makeSyncer(*place, webDavPasswords.value(place->id));
        return;
    }
    webDavWaiting = true;
    emit syncChanged();
    const quint64 session = vaultSession;
    QPointer<VaultController> self(this);
    withCloudSecret(*place, [self, session, place = *place](std::optional<QString> password) {
        if (!self || session != self->vaultSession || self->currentState != State::Unlocked) {
            return;
        }
        if (!password && place.kind != QLatin1String("webdav")) {
            // No sign-in on this device for it: the person signs in (D-080).
            self->signInNeeded = true;
            emit self->syncChanged();
            return;
        }
        if (!password) {
            // Not in the system's store: asked of the person, kept in
            // memory only (D-057).
            self->passwordNeeded = true;
            if (self->retryWaiting) {
                self->retryWaiting = false;
                emit self->retried(
                    tr("Not synchronized: the server's password is needed. The conflict stays."), true);
            }
            emit self->syncChanged();
            return;
        }
        self->webDavPasswords.insert(place.id, *password);
        self->makeSyncer(place, *password);
    });
}

void VaultController::withCloudSecret(const CloudPlace& place, std::function<void(std::optional<QString>)> then)
{
    if (webDavPasswords.contains(place.id)) {
        then(webDavPasswords.value(place.id));
        return;
    }
    // The store may be slow or ask the person: off the window's thread (OR-5).
    const std::shared_ptr<SecretStore> store = secrets;
    const QString name = place.secretName();
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run([self, store, name, then = std::move(then)]() mutable {
        auto [result, secret] = store ? store->find(name) : std::pair{SecretStore::Result::Unavailable, QByteArray()};
        std::optional<QString> password;
        if (result == SecretStore::Result::Done) {
            password = QString::fromUtf8(secret);
        }
        crypto::wipe(secret);
        QMetaObject::invokeMethod(
            qApp,
            [self, password, then = std::move(then)]() {
                if (self) {
                    then(password);
                }
            },
            Qt::QueuedConnection);
    });
}

void VaultController::keepCloudSecret(const CloudPlace& place, const QString& password)
{
    webDavPasswords.insert(place.id, password);
    const std::shared_ptr<SecretStore> store = secrets;
    if (!store) {
        return;
    }
    const QString name = place.secretName();
    QByteArray secret = password.toUtf8();
    // Not kept there (no keyring on this system): it is asked once per run.
    (void)QtConcurrent::run([store, name, secret]() mutable {
        (void)store->keep(name, secret);
        crypto::wipe(secret);
    });
}

void VaultController::makeSyncer(const CloudPlace& place, const QString& secret)
{
    std::unique_ptr<CloudStorage> storage = makeCloudStorage(place, secret);
    if (!storage) {
        return; // a storage this build does not have (D-080)
    }
    webDavWaiting = false;
    passwordNeeded = false;
    cloudKindNow = place.kind;
    syncer = std::make_unique<CloudSync>(path, place, std::move(storage), key, &savePool);
    if (syncStepCheck) {
        syncer->setStepCheckForTests(syncStepCheck);
    }
    connect(syncer.get(), &CloudSync::finished, this, &VaultController::syncFinished);
    connect(syncer.get(), &CloudSync::workingCopyChanged, this, [this]() {
        // Read again, or merged with a change waiting here at its save
        // (D-032, D-054).
        checkForChangeElsewhere();
    });
    syncTimer.start(syncEveryMs);
    syncer->start(); // every unlock syncs (ref)
    emit syncChanged();
}

void VaultController::stopWebDav()
{
    syncTimer.stop();
    syncSoonTimer.stop();
    if (syncer) {
        // Possibly inside its own signal: put away once that returns. A sync
        // stopped midway loses nothing (OR-15.4).
        syncer->disconnect(this);
        syncer.release()->deleteLater();
    }
    const bool changed =
        webDavWaiting || passwordNeeded || signInNeeded || !syncTrouble.isEmpty() || !askedCertificate.isEmpty();
    webDavWaiting = false;
    passwordNeeded = false;
    signInNeeded = false;
    syncTrouble.clear();
    askedCertificate.clear();
    cloudKindNow.clear();
    if (changed) {
        emit syncChanged();
    }
}

void VaultController::syncNow()
{
    if (currentState != State::Unlocked) {
        return;
    }
    if (syncer) {
        syncer->start();
        emit syncChanged();
    } else if (cloudPlaceOf(path)) {
        startWebDav();
    }
}

void VaultController::syncFinished(SyncOutcome outcome, const QByteArray& certificate, const QString& conflictCopy)
{
    askedCertificate.clear();
    if (outcome == SyncOutcome::Synced) {
        syncTrouble.clear();
    } else if (outcome != SyncOutcome::Interrupted) {
        syncTrouble = tr("Not synchronized: %1").arg(syncOutcomeText(outcome, cloudKindNow));
    }
    if (outcome == SyncOutcome::Certificate) {
        askedCertificate = certificate; // asked of the person (D-057)
    }
    if (outcome == SyncOutcome::Unauthorized && syncer) {
        // The password kept is no longer the server's: asked again; a
        // Google sign-in taken away is renewed in the browser (D-080).
        webDavPasswords.remove(syncer->place().id);
        if (syncer->place().kind == QLatin1String("webdav")) {
            passwordNeeded = true;
        } else {
            signInNeeded = true;
        }
    }
    if (outcome == SyncOutcome::OtherPassword) {
        // The server's version has another master password: this device's
        // version is kept as a conflict copy, and the vault locks (D-057).
        // An edit still open is handed over first, as at a lock: its save
        // finds the other master password and keeps it as a conflict copy of
        // its own (D-032).
        emit locking();
        addVault(conflictCopy);
        forgetUnlockedVault();
        setState(State::Locked);
        say(tr("The vault was given another master password on another device. Unlock it with that one. Your "
               "version is kept as “%1”.")
                .arg(QFileInfo(conflictCopy).completeBaseName()),
            true);
        return;
    }
    if (retryWaiting) {
        // Retry sync asked for this one (D-061); what another device settled
        // arrives with it.
        retryWaiting = false;
        emit retried(outcome == SyncOutcome::Synced
                         ? tr("The vault was synchronized. A conflict still listed is the same there.")
                         : tr("Not synchronized: %1").arg(syncOutcomeText(outcome, cloudKindNow)) + QLatin1Char(' ') +
                               tr("The conflict stays."),
                     outcome != SyncOutcome::Synced);
    }
    emit syncChanged();
}

void VaultController::trustCertificate(bool trust)
{
    if (!syncer || askedCertificate.isEmpty()) {
        return;
    }
    const QByteArray fingerprint = askedCertificate;
    askedCertificate.clear();
    if (!trust) {
        emit syncChanged();
        return;
    }
    WebDavPlace place = WebDavPlace::fromCloud(syncer->place());
    place.trustedCertificate = fingerprint; // remembered for this vault; another certificate asks again
    rememberWebDavPlace(path, place);
    const QString password = webDavPasswords.value(place.id);
    stopWebDav();
    makeSyncer(place.toCloud(), password);
}

void VaultController::giveWebDavPassword(const QString& password)
{
    const auto place = cloudPlaceOf(path);
    if (!place || currentState != State::Unlocked || password.isEmpty()) {
        return;
    }
    keepCloudSecret(*place, password);
    stopWebDav();
    makeSyncer(*place, password);
}

void VaultController::connectWebDav(const QString& address, const QString& user, const QString& password,
                                    const QString& trustedCertificate)
{
    QString given = address.trimmed();
    if (!given.contains(QLatin1String("://"))) {
        given.prepend(QStringLiteral("https://"));
    }
    // Read as typed: a space becomes %20 (the owner's test, 0.3.62).
    const QUrl folder(given, QUrl::TolerantMode);
    const QString scheme = folder.scheme().toLower();
    if (!folder.isValid() || folder.host().isEmpty() || (scheme != QLatin1String("https") && scheme != QLatin1String("http"))) {
        emit webDavConnected(false, tr("That is not a web address."), {}, QString());
        return;
    }
    QByteArray trusted = trustedCertificate.toLatin1().toLower();
    trusted.replace(':', "");
    showWebDavFolder(WebDavAccount{folder.adjusted(QUrl::RemoveUserInfo), user, password, trusted}, true);
}

void VaultController::showWebDavFolder(const WebDavAccount& account, bool connecting)
{
    // The folder shown changes only once its list came: a folder that cannot
    // be read leaves the one on screen, and what is made, opened or moved
    // goes where the person sees (0.3.62 review).
    browsing = std::make_unique<WebDavClient>(account);
    WebDavClient* asking = browsing.get();
    if (connecting) {
        connection.reset();
        connectedFiles.clear();
    }
    QPointer<VaultController> self(this);
    asking->list([self, asking, connecting](const WebDavReply& answer, const QList<WebDavEntry>& entries) {
        if (!self || self->browsing.get() != asking) {
            return;
        }
        if (!answer.ok()) {
            self->browsing.reset();
            if (connecting) {
                emit self->webDavConnected(false, connectProblem(answer), {}, shownFingerprint(answer.certificate));
            } else {
                emit self->webDavDone(false, connectProblem(answer));
            }
            return;
        }
        QStringList files;
        const QVariantList shown = self->shownEntries(entries, files);
        self->connection = std::move(self->browsing);
        self->connectedFiles = files;
        if (connecting) {
            emit self->webDavConnected(true, QString(), files, QString());
        }
        emit self->webDavFolderShown(asking->account().folder.toDisplayString(QUrl::RemoveUserInfo), shown, files);
    });
}

QVariantList VaultController::shownEntries(const QList<CloudEntry>& entries, QStringList& files) const
{
    QVariantList shown; // folders first, then vaults, each by name
    QList<CloudEntry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const CloudEntry& one, const CloudEntry& other) {
        if (one.isFolder != other.isFolder) {
            return one.isFolder;
        }
        return one.name.compare(other.name, Qt::CaseInsensitive) < 0;
    });
    for (const CloudEntry& entry : std::as_const(sorted)) {
        const bool vault = !entry.isFolder && entry.name.endsWith(vaultSuffix, Qt::CaseInsensitive) &&
                           !entry.name.contains(QLatin1String(".uploading"));
        if (!entry.isFolder && !vault) {
            continue;
        }
        if (vault) {
            files.append(entry.name);
        }
        shown.append(QVariantMap{
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("folder"), entry.isFolder},
            {QStringLiteral("size"), entry.isFolder || entry.size < 0 ? QString() : sizeText(entry.size)},
            {QStringLiteral("modified"), entry.modified.isValid()
                                             ? QLocale().toString(entry.modified.toLocalTime(), QLocale::ShortFormat)
                                             : QString()},
        });
    }
    return shown;
}

void VaultController::openWebDavFolder(const QString& name)
{
    if (!connection) {
        return;
    }
    WebDavAccount account = connection->account();
    if (name.contains(QLatin1String("://"))) {
        // A whole address, for Back in the file dialog: only on the server
        // already signed in to.
        const QUrl address(name, QUrl::TolerantMode);
        if (address.host() != account.folder.host() || address.scheme() != account.folder.scheme() ||
            address.port() != account.folder.port()) {
            return;
        }
        account.folder = address;
    } else if (name == QLatin1String("..")) {
        account.folder = account.folder.resolved(QUrl(QStringLiteral("..")));
    } else {
        account.folder = connection->addressOf(name);
    }
    showWebDavFolder(account, false);
}

void VaultController::makeWebDavFolder(const QString& name)
{
    if (!connection) {
        return;
    }
    const QString problem = webDavNameProblem(name);
    if (!problem.isEmpty()) {
        emit webDavDone(false, problem);
        return;
    }
    WebDavClient* asking = connection.get();
    const QString clean = name.trimmed();
    QPointer<VaultController> self(this);
    asking->makeFolder(clean, [self, asking, clean](const WebDavReply& answer) {
        if (!self || self->connection.get() != asking) {
            return;
        }
        if (!answer.ok()) {
            emit self->webDavDone(false, answer.status == 405 ? tr("A folder or file with that name is already there.")
                                                             : connectProblem(answer));
            return;
        }
        self->openWebDavFolder(clean); // into the new folder
    });
}

QString VaultController::webDavNameProblem(const QString& name)
{
    // The name is a file name here and on the server: what Windows refuses
    // is refused on every system, so a name works on every device.
    static const QRegularExpression notInAName(QStringLiteral("[/\\\\:*?\"<>|\\x00-\\x1f]"));
    static const QRegularExpression reserved(QStringLiteral("^(con|prn|aux|nul|com[1-9]|lpt[1-9])$"),
                                             QRegularExpression::CaseInsensitiveOption);
    const QString clean = name.trimmed();
    if (clean.isEmpty() || clean.contains(notInAName) || clean.startsWith(QLatin1Char('.')) ||
        clean.endsWith(QLatin1Char('.')) || clean.contains(reserved)) {
        return tr("Choose a name without / \\ : * ? \" < > |, not starting or ending with a dot.");
    }
    return QString();
}

QString VaultController::newWorkingCopyPath(const QString& name) const
{
    return newWorkingCopyPath(QStringLiteral("webdav"), name);
}

QString VaultController::newWorkingCopyPath(const QString& kind, const QString& name) const
{
    QDir().mkpath(cloudDataFolder(kind));
    const QString start = cloudDataFolder(kind) + QLatin1Char('/') + name;
    QString candidate = start + vaultSuffix;
    for (int number = 2; QFileInfo::exists(candidate) || vaultList.contains(candidate); ++number) {
        candidate = start + QStringLiteral(" %1").arg(number) + vaultSuffix;
    }
    return candidate;
}

void VaultController::createWebDavVault(const QString& name, const QString& password, const QString& repeated,
                                        const QUrl& keyfile, bool savePassword)
{
    std::optional<CloudPlace> target;
    QString secret;
    if (connection) {
        WebDavPlace place;
        place.folder = connection->account().folder;
        place.user = connection->account().user;
        place.trustedCertificate = connection->account().trustedCertificate;
        target = place.toCloud();
        secret = connection->account().password;
    }
    createCloudVault(target, secret, connectedFiles, name, password, repeated, keyfile, savePassword);
}

void VaultController::createCloudVault(const std::optional<CloudPlace>& target, const QString& secret,
                                       const QStringList& files, const QString& name, const QString& password,
                                       const QString& repeated, const QUrl& keyfile, bool savePassword)
{
    const QString cleanName = name.trimmed();
    const bool webDav = !target || target->kind == QLatin1String("webdav");
    if (!target) {
        say(webDav ? tr("Connect to the WebDAV folder first.") : tr("Sign in to Google Drive first."), true);
        return;
    }
    if (const QString problem = webDavNameProblem(cleanName); !problem.isEmpty()) {
        say(problem, true);
        return;
    }
    const QString fileName = cleanName + vaultSuffix;
    if (files.contains(fileName, Qt::CaseInsensitive)) {
        // Never over a vault already there (OR-15).
        say(tr("A vault with that name is already in that folder. Choose another name."), true);
        return;
    }
    CloudPlace place = *target;
    place.id = newCloudId();
    place.fileName = fileName;
    if (password.isEmpty()) {
        say(tr("Choose a master password."), true);
        return;
    }
    if (password != repeated) {
        say(tr("The two passwords are not the same."), true);
        return;
    }
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        say(tr("Wait until your last change is saved, then try again."), true);
        return;
    }
    // The new vault is made here, put on the server, and only then kept:
    // a first upload that fails leaves nothing behind (the owner, 0.3.62).
    const QString workingCopy = newWorkingCopyPath(place.kind, cleanName);
    const QString serverPassword = secret;
    const QString keyfileFile = keyfile.isLocalFile() ? keyfile.toLocalFile() : keyfile.toString();
    const QString savedPassword = savePassword && canSaveInPrimary() ? password : QString();
    const State before = currentState;
    setState(State::Busy);
    say(webDav ? tr("Creating the vault on the WebDAV server…") : tr("Creating the vault on Google Drive…"), false);
    QByteArray passwordBytes = password.toUtf8();
    const crypto::KdfParameters parameters = kdfParameters;
    const quint64 attempt = ++unlockAttempt;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, workingCopy, passwordBytes, parameters, attempt, keyfileFile, before,
                                          savedPassword, place, serverPassword]() mutable {
        std::optional<QByteArray> fingerprint = QByteArray();
        if (!keyfileFile.isEmpty()) {
            fingerprint = keyfileFingerprint(keyfileFile);
        }
        VaultKey newKey;
        SaveStatus status = SaveStatus::CannotWrite;
        QByteArray written;
        if (fingerprint) {
            newKey = makeVaultKey(passwordBytes, parameters, *fingerprint);
            status = newKey.isValid() ? saveVaultFile(workingCopy, contentsToJson({}), newKey, {}, &written)
                                      : SaveStatus::CannotWrite;
        }
        crypto::wipe(passwordBytes);
        QMetaObject::invokeMethod(
            qApp,
            [self, workingCopy, newKey, status, attempt, keyfileFile, before, savedPassword, fingerprint, written,
             place, serverPassword]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    newKey.wipe();
                    QFile::remove(workingCopy);
                    return;
                }
                const auto failed = [self, workingCopy, before, place](const QString& problem) {
                    QFile::remove(workingCopy);
                    QFile::remove(cloudBasePath(place));
                    forgetCloudPlace(workingCopy);
                    self->setState(before == State::Unlocked ? State::Unlocked
                                   : self->path.isEmpty()    ? State::NoVault
                                                             : State::Locked);
                    self->say(problem, true);
                    if (place.kind == QLatin1String("webdav")) {
                        emit self->webDavCreateFailed(problem);
                    } else {
                        emit self->googleDriveCreateFailed(problem);
                    }
                };
                if (!fingerprint || status != SaveStatus::Saved) {
                    newKey.wipe();
                    failed(!fingerprint ? tr("The keyfile could not be read. The vault was not created.")
                                        : tr("The new vault could not be created: this computer cannot write its "
                                             "copy, or the disk is full."));
                    return;
                }
                std::unique_ptr<CloudStorage> storage = makeCloudStorage(place, serverPassword);
                if (!storage) {
                    newKey.wipe();
                    failed(tr("That storage is not in this version of Chrono Vault. Nothing was kept."));
                    return;
                }
                auto* first = new CloudSync(workingCopy, place, std::move(storage), newKey, &self->savePool, self);
                QObject::connect(
                    first, &CloudSync::finished, self,
                    [self, first, workingCopy, newKey, fingerprint, written, keyfileFile, before, savedPassword, place,
                     serverPassword, failed](SyncOutcome outcome, const QByteArray&, const QString&) mutable {
                        first->deleteLater();
                        if (outcome != SyncOutcome::Synced) {
                            newKey.wipe();
                            failed((place.kind == QLatin1String("webdav")
                                        ? tr("The vault could not be put on the WebDAV server: %1. Nothing was kept.")
                                        : tr("The vault could not be put on Google Drive: %1. Nothing was kept."))
                                       .arg(syncOutcomeText(outcome, place.kind)));
                            return;
                        }
                        self->keepCloudSecret(place, serverPassword);
                        self->finishCreating(workingCopy, newKey, *fingerprint, written, keyfileFile, before,
                                             savedPassword);
                    });
                if (self->syncStepCheck) {
                    first->setStepCheckForTests(self->syncStepCheck);
                }
                first->start();
            },
            Qt::QueuedConnection);
    });
}

void VaultController::openWebDavVault(const QString& fileName)
{
    if (!connection || !connectedFiles.contains(fileName)) {
        emit webDavDone(false, tr("Connect to the WebDAV folder first."));
        return;
    }
    WebDavPlace onServer;
    onServer.folder = connection->account().folder;
    onServer.user = connection->account().user;
    onServer.trustedCertificate = connection->account().trustedCertificate;
    WebDavClient* asking = connection.get();
    QPointer<VaultController> self(this);
    openCloudVault(
        onServer.toCloud(), connection->account().password, fileName,
        [self, asking](const QString& name, CloudStorage::Done done) {
            if (self && self->connection.get() == asking) {
                asking->get(name, {}, std::move(done));
            }
        },
        [self, asking]() { return self && self->connection.get() == asking; });
}

void VaultController::openCloudVault(const CloudPlace& target, const QString& secret, const QString& fileName,
                                     std::function<void(const QString&, CloudStorage::Done)> read,
                                     std::function<bool()> stillAsked)
{
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        emit webDavDone(false, tr("Wait until your last change is saved, then try again."));
        return;
    }
    QPointer<VaultController> self(this);
    read(fileName, [self, target, secret, fileName, stillAsked](const CloudReply& answer) {
        if (!self || !stillAsked()) {
            return;
        }
        if (!answer.ok()) {
            emit self->webDavDone(false, cloudProblem(target.kind, answer));
            return;
        }
        CloudPlace place = target;
        place.id = newCloudId();
        place.fileName = fileName;
        place.tag = answer.tag;
        place.syncedStamp = answer.body.left(52);
        place.lastSynced = QDateTime::currentDateTimeUtc();
        const QString password = secret;
        const QString workingCopy =
            self->newWorkingCopyPath(place.kind, fileName.left(fileName.size() - vaultSuffix.size()));
        const QString base = cloudBasePath(place);
        // Written as it came, safely, on the save thread (OR-5, OR-15); only
        // a vault is ever written as one (OR-4).
        (void)QtConcurrent::run(&self->savePool, [self, data = answer.body, workingCopy, base, place, password]() {
            const SaveStatus status = saveVaultData(workingCopy, data);
            if (status == SaveStatus::Saved) {
                (void)saveVaultData(base, data);
            }
            QMetaObject::invokeMethod(
                qApp,
                [self, status, workingCopy, place, password]() {
                    if (!self) {
                        return;
                    }
                    if (status != SaveStatus::Saved) {
                        emit self->webDavDone(false, tr("That file is not a Chrono Vault vault, or this device "
                                                        "cannot write its copy."));
                        return;
                    }
                    rememberCloudPlace(workingCopy, place);
                    self->keepCloudSecret(place, password);
                    self->addVault(workingCopy);
                    emit self->webDavDone(true, QString());
                    self->chooseVault(QUrl::fromLocalFile(workingCopy));
                },
                Qt::QueuedConnection);
        });
    });
}

void VaultController::disconnectWebDav(const QString& vaultFile)
{
    const auto place = cloudPlaceOf(vaultFile);
    if (!place) {
        return;
    }
    // Two vaults from now on: this device's and the server's; nothing is
    // deleted (the owner, D-060).
    if (vaultFile == path) {
        stopWebDav();
    }
    forgetCloudPlace(vaultFile);
    QFile::remove(cloudBasePath(*place));
    webDavPasswords.remove(place->id);
    if (const std::shared_ptr<SecretStore> store = secrets) {
        const QString name = place->secretName();
        (void)QtConcurrent::run([store, name]() { (void)store->remove(name); });
    }
    emit vaultsChanged();
    emit syncChanged();
    emit webDavDone(true, QString());
}

QString VaultController::storageName() const
{
    if (!inCloud()) {
        return QString();
    }
    const auto kind = cloudKindOf(cloudKindNow);
    return kind ? kind->name : QString();
}

QVariantMap VaultController::vaultPlace(const QString& vaultFile) const
{
    const auto place = cloudPlaceOf(vaultFile);
    const auto kind = place ? cloudKindOf(place->kind) : std::nullopt;
    if (!place || !kind) {
        return {
            {QStringLiteral("inCloud"), false},
            {QStringLiteral("kind"), QString()},
            {QStringLiteral("storageName"), QString()},
            {QStringLiteral("placePicture"), QStringLiteral("qrc:/icons/places-device.svg")},
            {QStringLiteral("onWebDav"), false},
            {QStringLiteral("address"), QDir::toNativeSeparators(vaultFile)},
        };
    }
    const QString synced = place->lastSynced.isValid()
                               ? QLocale().toString(place->lastSynced.toLocalTime(), QLocale::ShortFormat)
                               : tr("Not yet");
    const bool webDav = place->kind == QLatin1String("webdav");
    return {
        {QStringLiteral("inCloud"), true},
        {QStringLiteral("kind"), place->kind},
        {QStringLiteral("storageName"), kind->name},
        {QStringLiteral("placePicture"), kind->placePicture},
        {QStringLiteral("onWebDav"), webDav},
        {QStringLiteral("address"), shownCloudAddress(*place)},
        {QStringLiteral("user"), webDav ? WebDavPlace::fromCloud(*place).user : QString()},
        {QStringLiteral("lastSynced"), synced},
    };
}

} // namespace chrono_vault
