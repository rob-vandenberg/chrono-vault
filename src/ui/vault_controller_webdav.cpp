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
// vault_controller_webdav.cpp 0.3.62

// --- Version history ----------------------------------------------------
// v0.3.62: The folder browser and New folder; an address read as typed; a non-WebDAV server said in words (D-057)
// v0.3.60: Initial release: a vault kept on a WebDAV server (D-057)

// The controller's WebDAV part (D-057). A vault kept on a server has a
// working copy on this device, which every unlock, save and merge uses as
// a local vault does; WebDavSync keeps it the same as the server's.

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
    const auto place = webDavPlaceOf(path);
    if (!place || currentState != State::Unlocked) {
        emit syncChanged();
        return;
    }
    if (webDavPasswords.contains(place->id)) {
        makeSyncer(*place, webDavPasswords.value(place->id));
        return;
    }
    webDavWaiting = true;
    emit syncChanged();
    const quint64 session = vaultSession;
    QPointer<VaultController> self(this);
    withWebDavPassword(*place, [self, session, place = *place](std::optional<QString> password) {
        if (!self || session != self->vaultSession || self->currentState != State::Unlocked) {
            return;
        }
        if (!password) {
            // Not in the system's store: asked of the person, kept in
            // memory only (D-057).
            self->passwordNeeded = true;
            emit self->syncChanged();
            return;
        }
        self->webDavPasswords.insert(place.id, *password);
        self->makeSyncer(place, *password);
    });
}

void VaultController::withWebDavPassword(const WebDavPlace& place, std::function<void(std::optional<QString>)> then)
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

void VaultController::keepWebDavPassword(const WebDavPlace& place, const QString& password)
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

void VaultController::makeSyncer(const WebDavPlace& place, const QString& password)
{
    webDavWaiting = false;
    passwordNeeded = false;
    syncer = std::make_unique<WebDavSync>(path, place, password, key, &savePool);
    if (syncStepCheck) {
        syncer->setStepCheckForTests(syncStepCheck);
    }
    connect(syncer.get(), &WebDavSync::finished, this, &VaultController::syncFinished);
    connect(syncer.get(), &WebDavSync::workingCopyChanged, this, [this]() {
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
    const bool changed = webDavWaiting || passwordNeeded || !syncTrouble.isEmpty() || !askedCertificate.isEmpty();
    webDavWaiting = false;
    passwordNeeded = false;
    syncTrouble.clear();
    askedCertificate.clear();
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
    } else if (webDavPlaceOf(path)) {
        startWebDav();
    }
}

void VaultController::syncFinished(SyncOutcome outcome, const QByteArray& certificate, const QString& conflictCopy)
{
    askedCertificate.clear();
    if (outcome == SyncOutcome::Synced) {
        syncTrouble.clear();
    } else if (outcome != SyncOutcome::Interrupted) {
        syncTrouble = tr("Not synchronized: %1").arg(syncOutcomeText(outcome));
    }
    if (outcome == SyncOutcome::Certificate) {
        askedCertificate = certificate; // asked of the person (D-057)
    }
    if (outcome == SyncOutcome::Unauthorized && syncer) {
        // The password kept is no longer the server's: asked again.
        webDavPasswords.remove(syncer->place().id);
        passwordNeeded = true;
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
    WebDavPlace place = syncer->place();
    place.trustedCertificate = fingerprint; // remembered for this vault; another certificate asks again
    rememberWebDavPlace(path, place);
    const QString password = webDavPasswords.value(place.id);
    stopWebDav();
    makeSyncer(place, password);
}

void VaultController::giveWebDavPassword(const QString& password)
{
    const auto place = webDavPlaceOf(path);
    if (!place || currentState != State::Unlocked || password.isEmpty()) {
        return;
    }
    keepWebDavPassword(*place, password);
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
        QStringList folders;
        for (const WebDavEntry& entry : entries) {
            if (entry.isFolder) {
                folders.append(entry.name);
            } else if (entry.name.endsWith(vaultSuffix, Qt::CaseInsensitive) &&
                       !entry.name.contains(QLatin1String(".uploading"))) {
                files.append(entry.name);
            }
        }
        files.sort(Qt::CaseInsensitive);
        folders.sort(Qt::CaseInsensitive);
        self->connection = std::move(self->browsing);
        self->connectedFiles = files;
        if (connecting) {
            emit self->webDavConnected(true, QString(), files, QString());
        }
        emit self->webDavFolderShown(asking->account().folder.toDisplayString(QUrl::RemoveUserInfo), folders, files);
    });
}

void VaultController::openWebDavFolder(const QString& name)
{
    if (!connection) {
        return;
    }
    WebDavAccount account = connection->account();
    if (name == QLatin1String("..")) {
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
    QDir().mkpath(webDavDataFolder());
    const QString start = webDavDataFolder() + QLatin1Char('/') + name;
    QString candidate = start + vaultSuffix;
    for (int number = 2; QFileInfo::exists(candidate) || vaultList.contains(candidate); ++number) {
        candidate = start + QStringLiteral(" %1").arg(number) + vaultSuffix;
    }
    return candidate;
}

void VaultController::createWebDavVault(const QString& name, const QString& password, const QString& repeated,
                                        const QUrl& keyfile, bool savePassword)
{
    const QString cleanName = name.trimmed();
    if (!connection) {
        say(tr("Connect to the WebDAV folder first."), true);
        return;
    }
    if (const QString problem = webDavNameProblem(cleanName); !problem.isEmpty()) {
        say(problem, true);
        return;
    }
    const QString fileName = cleanName + vaultSuffix;
    if (connectedFiles.contains(fileName, Qt::CaseInsensitive)) {
        // Never over a vault already there (OR-15).
        say(tr("A vault with that name is already in that folder. Choose another name."), true);
        return;
    }
    WebDavPlace place;
    place.id = newWebDavId();
    place.folder = connection->account().folder;
    place.fileName = fileName;
    place.user = connection->account().user;
    place.trustedCertificate = connection->account().trustedCertificate;
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
    const QString workingCopy = newWorkingCopyPath(cleanName);
    const QString serverPassword = connection->account().password;
    const QString keyfileFile = keyfile.isLocalFile() ? keyfile.toLocalFile() : keyfile.toString();
    const QString savedPassword = savePassword && canSaveInPrimary() ? password : QString();
    const State before = currentState;
    setState(State::Busy);
    say(tr("Creating the vault on the WebDAV server…"), false);
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
                    QFile::remove(webDavBasePath(place.id));
                    forgetWebDavPlace(workingCopy);
                    self->setState(before == State::Unlocked ? State::Unlocked
                                   : self->path.isEmpty()    ? State::NoVault
                                                             : State::Locked);
                    self->say(problem, true);
                    emit self->webDavCreateFailed(problem);
                };
                if (!fingerprint || status != SaveStatus::Saved) {
                    newKey.wipe();
                    failed(!fingerprint ? tr("The keyfile could not be read. The vault was not created.")
                                        : tr("The new vault could not be created: this computer cannot write its "
                                             "copy, or the disk is full."));
                    return;
                }
                auto* first = new WebDavSync(workingCopy, place, serverPassword, newKey, &self->savePool, self);
                QObject::connect(
                    first, &WebDavSync::finished, self,
                    [self, first, workingCopy, newKey, fingerprint, written, keyfileFile, before, savedPassword, place,
                     serverPassword, failed](SyncOutcome outcome, const QByteArray&, const QString&) mutable {
                        first->deleteLater();
                        if (outcome != SyncOutcome::Synced) {
                            newKey.wipe();
                            failed(tr("The vault could not be put on the WebDAV server: %1. Nothing was kept.")
                                       .arg(syncOutcomeText(outcome)));
                            return;
                        }
                        self->keepWebDavPassword(place, serverPassword);
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
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        emit webDavDone(false, tr("Wait until your last change is saved, then try again."));
        return;
    }
    WebDavClient* asking = connection.get();
    QPointer<VaultController> self(this);
    asking->get(fileName, {}, [self, asking, fileName](const WebDavReply& answer) {
        if (!self || self->connection.get() != asking) {
            return;
        }
        if (!answer.ok()) {
            emit self->webDavDone(false, connectProblem(answer));
            return;
        }
        WebDavPlace place;
        place.id = newWebDavId();
        place.folder = asking->account().folder;
        place.fileName = fileName;
        place.user = asking->account().user;
        place.trustedCertificate = asking->account().trustedCertificate;
        place.tag = answer.tag;
        place.syncedStamp = answer.body.left(52);
        place.lastSynced = QDateTime::currentDateTimeUtc();
        const QString password = asking->account().password;
        const QString workingCopy =
            self->newWorkingCopyPath(fileName.left(fileName.size() - vaultSuffix.size()));
        const QString base = webDavBasePath(place.id);
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
                    rememberWebDavPlace(workingCopy, place);
                    self->keepWebDavPassword(place, password);
                    self->addVault(workingCopy);
                    emit self->webDavDone(true, QString());
                    self->chooseVault(QUrl::fromLocalFile(workingCopy));
                },
                Qt::QueuedConnection);
        });
    });
}

void VaultController::moveToWebDav(const QString& vaultFile)
{
    if (!connection) {
        emit webDavDone(false, tr("Connect to the WebDAV folder first."));
        return;
    }
    if (webDavPlaceOf(vaultFile)) {
        emit webDavDone(false, tr("That vault is already kept on a WebDAV server."));
        return;
    }
    const QString fileName = QFileInfo(vaultFile).completeBaseName() + vaultSuffix;
    if (connectedFiles.contains(fileName, Qt::CaseInsensitive)) {
        emit webDavDone(false, tr("A vault with the name “%1” is already in that folder.").arg(fileName));
        return;
    }
    WebDavPlace place;
    place.id = newWebDavId();
    place.folder = connection->account().folder;
    place.fileName = fileName;
    place.user = connection->account().user;
    place.trustedCertificate = connection->account().trustedCertificate;
    // The vault's own file becomes its working copy; its first sync puts it
    // on the server (REQUIREMENTS.md 5.4.4).
    rememberWebDavPlace(vaultFile, place);
    keepWebDavPassword(place, connection->account().password);
    emit vaultsChanged();
    emit webDavDone(true, QString());
    if (vaultFile == path && currentState == State::Unlocked) {
        startWebDav();
    }
}

void VaultController::disconnectWebDav(const QString& vaultFile, bool deleteOnServer)
{
    const auto place = webDavPlaceOf(vaultFile);
    if (!place) {
        return;
    }
    const auto forget = [this, vaultFile, place = *place]() {
        if (vaultFile == path) {
            stopWebDav();
        }
        forgetWebDavPlace(vaultFile);
        QFile::remove(webDavBasePath(place.id));
        webDavPasswords.remove(place.id);
        if (const std::shared_ptr<SecretStore> store = secrets) {
            const QString name = place.secretName();
            (void)QtConcurrent::run([store, name]() { (void)store->remove(name); });
        }
        emit vaultsChanged();
        emit syncChanged();
        emit webDavDone(true, QString());
    };
    if (!deleteOnServer) {
        forget(); // the server's copy stays (ref)
        return;
    }
    QPointer<VaultController> self(this);
    withWebDavPassword(*place, [self, place = *place, forget](std::optional<QString> password) {
        if (!self) {
            return;
        }
        auto* client = new WebDavClient(
            WebDavAccount{place.folder, place.user, password.value_or(QString()), place.trustedCertificate}, self);
        client->remove(place.fileName, [self, client, forget](const WebDavReply& answer) {
            client->deleteLater();
            if (!self) {
                return;
            }
            if (!answer.ok() && answer.error != WebDavError::NotFound) {
                // Still synced: nothing is half done.
                emit self->webDavDone(false, connectProblem(answer));
                return;
            }
            forget();
        });
    });
}

QVariantMap VaultController::vaultPlace(const QString& vaultFile) const
{
    const auto place = webDavPlaceOf(vaultFile);
    if (!place) {
        return {{QStringLiteral("onWebDav"), false}, {QStringLiteral("address"), QDir::toNativeSeparators(vaultFile)}};
    }
    const QString synced = place->lastSynced.isValid()
                               ? QLocale().toString(place->lastSynced.toLocalTime(), QLocale::ShortFormat)
                               : tr("Not yet");
    return {
        {QStringLiteral("onWebDav"), true},
        {QStringLiteral("address"), place->shownAddress()},
        {QStringLiteral("user"), place->user},
        {QStringLiteral("lastSynced"), synced},
    };
}

} // namespace chrono_vault
