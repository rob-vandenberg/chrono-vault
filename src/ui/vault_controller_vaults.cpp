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


#include "settings_files.h"
#include "vault_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

// --- Version ------------------------------------------------------------
// vault_controller_vaults.cpp 0.6.106

// --- Version history ----------------------------------------------------
// v0.6.106: Close and Lock per vault (D-075)
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.5.75: Initial release: the vault menu's Lock, Move… and Forget… (D-060)

// The controller's part for the menu of each vault in Settings > Vaults
// (D-060). A move never loses the vault: the original goes only after its
// copy is in the new place and read back the same (OR-15).

namespace chrono_vault {

namespace {

const QString vaultSuffix = QStringLiteral(".chronovault");
// The largest vault file a move reads.
constexpr qint64 largestVaultBytes = 1024LL * 1024 * 1024;

QString stoppedText()
{
    return VaultController::tr("The move was stopped. The vault stays where it was.");
}

} // namespace

bool VaultController::refusedWhileMoving()
{
    if (!moveRunning || path != movingVault) {
        return false;
    }
    setState(State::Locked);
    say(tr("This vault is being moved. Unlock it once the move is done."), true);
    return true;
}

void VaultController::lockVault(const QString& vaultFile)
{
    if (currentState == State::Busy) {
        return;
    }
    const QString primary = primaryVault();
    if (vaultFile == path && currentState == State::Unlocked) {
        // An open vault that opens without a password is closed first, as a door.
        if (!isKept(vaultFile)) {
            leaveOpenVault();
        }
        return;
    }
    if (!primaryKey.isValid()) {
        return; // it is locked already
    }
    if (vaultFile == primary) {
        forgetPrimary(); // its key store; nothing is deleted, an open vault stays open
        return;
    }
    if (!savedPasswords.contains(vaultFile)) {
        return;
    }
    if (currentState == State::Unlocked && path == primary && (dirty || savesRunning > 0)) {
        emit vaultMenuProblem(tr("Wait until your last change is saved, then try again."));
        return;
    }
    dropSavedPassword(vaultFile);
}

void VaultController::closeOpenVault(const QString& vaultFile)
{
    if (vaultFile != path || currentState != State::Unlocked) {
        return;
    }
    if (dirty || savesRunning > 0) {
        emit vaultMenuProblem(tr("Wait until your last change is saved, then try again."));
        return;
    }
    if (path == primaryVault()) {
        rememberPrimary();
    }
    leaveOpenVault();
}

void VaultController::leaveOpenVault()
{
    // Only the vault that is open: the key store stays as it is (D-075).
    emit locking();
    keepUnsavedWhileLocked();
    forgetUnlockedVault();
    const QString primary = primaryVault();
    if (primaryKey.isValid() && foundList.contains(primary)) {
        setPath(primary, false); // the start-up page begins with the primary
    }
    say(QString(), false);
    setState(State::Locked);
}

bool VaultController::keepsChangesOf(const QString& vaultFile) const
{
    return !unsavedSealed.isEmpty() && unsavedPath == vaultFile;
}

QString VaultController::moveProblem(const QString& vaultFile) const
{
    if (moveRunning) {
        return tr("Another vault is being moved. Wait until that is done.");
    }
    if (!vaultList.contains(vaultFile) || !QFileInfo::exists(vaultFile)) {
        return tr("That vault cannot be found.");
    }
    if (vaultFile == path && currentState == State::Busy) {
        return tr("Wait until the vault is open, then try again.");
    }
    if (vaultFile == path && currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        return tr("Wait until your last change is saved, then try again.");
    }
    if (vaultFile == unsavedPath && !unsavedSealed.isEmpty()) {
        return tr("This vault holds a change kept from before it was locked. Unlock it so the change is saved, then "
                  "move it.");
    }
    if (const auto place = webDavPlaceOf(vaultFile); place && vaultFileStamp(vaultFile) != place->syncedStamp) {
        return tr("This vault has changes that are not on the server yet. Unlock it so it synchronizes, then move "
                  "it.");
    }
    return QString();
}

void VaultController::readForMove(const QString& vaultFile,
                                  std::function<void(std::optional<QByteArray>, QString)> then)
{
    QPointer<VaultController> self(this);
    if (const auto place = webDavPlaceOf(vaultFile)) {
        // The server's version: this device sent all it had (moveProblem),
        // and another device's latest change is in it.
        withWebDavPassword(*place, [self, place = *place, then = std::move(then)](std::optional<QString> password) {
            if (!self) {
                return;
            }
            if (!password) {
                then(std::nullopt, tr("The server's password is not known on this device. Unlock the vault once so "
                                      "it is asked, then move it."));
                return;
            }
            auto* client = new WebDavClient(WebDavAccount{place.folder, place.user, *password, place.trustedCertificate},
                                            self);
            client->get(place.fileName, {}, [self, client, then](const WebDavReply& answer) {
                client->deleteLater();
                if (!self) {
                    return;
                }
                if (!answer.ok()) {
                    then(std::nullopt, connectProblem(answer));
                    return;
                }
                then(answer.body, QString());
            });
        });
        return;
    }
    // Read off the window's thread (OR-5).
    (void)QtConcurrent::run(&filePool, [self, vaultFile, then = std::move(then)]() {
        std::optional<QByteArray> data;
        QFile file(vaultFile);
        if (file.open(QIODevice::ReadOnly) && file.size() <= largestVaultBytes) {
            data = file.readAll();
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, data, then]() {
                if (self) {
                    then(data, data ? QString() : tr("The vault could not be read."));
                }
            },
            Qt::QueuedConnection);
    });
}

void VaultController::moveVaultTo(const QString& vaultFile, const QUrl& file)
{
    const QString target = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QString problem = moveProblem(vaultFile);
    if (problem.isEmpty() && (target.isEmpty() || QFileInfo::exists(target))) {
        // Never over a file that is there (OR-15).
        problem = tr("A file with that name is already there. Choose another name.");
    }
    if (!problem.isEmpty()) {
        emit moveDone(false, problem);
        return;
    }
    moveRunning = true;
    movingVault = vaultFile;
    if (vaultFile == path && currentState == State::Unlocked) {
        lockEverything(); // nothing changes while it is copied (the owner)
        setState(State::Locked);
    }
    QPointer<VaultController> self(this);
    readForMove(vaultFile, [self, vaultFile, target](std::optional<QByteArray> data, const QString& readProblem) {
        if (!self) {
            return;
        }
        if (!data) {
            self->moveFailed(readProblem);
            return;
        }
        if (!self->moveGoesOn(MoveStep::Read)) {
            self->moveFailed(stoppedText());
            return;
        }
        // Written, flushed, read back and compared by saveVaultData, then read
        // back once more, all on the save thread (OR-5, OR-15).
        (void)QtConcurrent::run(&self->savePool, [self, vaultFile, target, data = *data]() {
            const bool written = saveVaultData(target, data) == SaveStatus::Saved;
            bool same = false;
            if (written) {
                QFile copy(target);
                same = copy.open(QIODevice::ReadOnly) && copy.readAll() == data;
            }
            QMetaObject::invokeMethod(
                qApp,
                [self, vaultFile, target, written, same]() {
                    if (!self) {
                        return;
                    }
                    if (!written) {
                        self->moveFailed(tr("The vault could not be written there: the folder cannot be written, or "
                                            "the disk is full. The vault stays where it was."));
                        return;
                    }
                    const auto undo = [self, target](const QString& why) {
                        QFile::remove(target);
                        self->moveFailed(why);
                    };
                    if (!self->moveGoesOn(MoveStep::Written)) {
                        undo(stoppedText());
                        return;
                    }
                    if (!same) {
                        undo(tr("The copy did not read back the same. The vault stays where it was."));
                        return;
                    }
                    if (!self->moveGoesOn(MoveStep::Verified)) {
                        undo(stoppedText());
                        return;
                    }
                    self->finishMove(vaultFile, target);
                },
                Qt::QueuedConnection);
        });
    });
}

void VaultController::moveVaultToWebDav(const QString& vaultFile)
{
    QString problem = moveProblem(vaultFile);
    const QString fileName = QFileInfo(vaultFile).completeBaseName() + vaultSuffix;
    if (problem.isEmpty() && !connection) {
        problem = tr("Connect to the WebDAV folder first.");
    }
    if (problem.isEmpty() && connectedFiles.contains(fileName, Qt::CaseInsensitive)) {
        problem = tr("A vault with the name “%1” is already in that folder.").arg(fileName);
    }
    if (!problem.isEmpty()) {
        emit moveDone(false, problem);
        return;
    }
    moveRunning = true;
    movingVault = vaultFile;
    if (vaultFile == path && currentState == State::Unlocked) {
        lockEverything();
        setState(State::Locked);
    }
    const WebDavAccount account = connection->account();
    QPointer<VaultController> self(this);
    readForMove(vaultFile, [self, vaultFile, fileName, account](std::optional<QByteArray> data,
                                                                const QString& readProblem) {
        if (!self) {
            return;
        }
        if (!data) {
            self->moveFailed(readProblem);
            return;
        }
        if (!self->moveGoesOn(MoveStep::Read)) {
            self->moveFailed(stoppedText());
            return;
        }
        WebDavPlace place;
        place.id = newWebDavId();
        place.folder = account.folder;
        place.fileName = fileName;
        place.user = account.user;
        place.trustedCertificate = account.trustedCertificate;
        // Put under a passing name, moved into place, then read back: a
        // broken-off upload never stands under the vault's name.
        const QString passing = fileName + QLatin1Char('.') + place.id + QStringLiteral(".uploading");
        auto* client = new WebDavClient(account, self);
        const auto fail = [self, client](const QString& why, const QString& removeFirst) {
            if (removeFirst.isEmpty()) {
                client->deleteLater();
                self->moveFailed(why);
                return;
            }
            client->remove(removeFirst, [self, client, why](const WebDavReply&) {
                client->deleteLater();
                if (self) {
                    self->moveFailed(why);
                }
            });
        };
        client->put(passing, *data, [self, client, fail, passing, fileName, place, data = *data, vaultFile,
                                     account](const WebDavReply& put) mutable {
            if (!self) {
                return;
            }
            if (!put.ok()) {
                fail(connectProblem(put), passing);
                return;
            }
            if (!self->moveGoesOn(MoveStep::Written)) {
                fail(stoppedText(), passing);
                return;
            }
            // Never over a vault another device put there meanwhile (OR-15).
            client->move(passing, fileName, {}, [self, client, fail, passing, fileName, place, data, vaultFile,
                                                 account](const WebDavReply& moved) mutable {
                if (!self) {
                    return;
                }
                if (!moved.ok()) {
                    fail(moved.status == 412 ? tr("A vault with the name “%1” is already in that folder.").arg(fileName)
                                             : connectProblem(moved),
                         passing);
                    return;
                }
                client->get(fileName, {}, [self, client, fail, fileName, place, data, vaultFile,
                                           account](const WebDavReply& back) mutable {
                    if (!self) {
                        return;
                    }
                    if (!back.ok() || back.body != data) {
                        fail(tr("The copy on the server did not read back the same. The vault stays where it was."),
                             fileName);
                        return;
                    }
                    if (!self->moveGoesOn(MoveStep::Verified)) {
                        fail(stoppedText(), fileName);
                        return;
                    }
                    place.tag = back.tag;
                    place.syncedStamp = data.left(52);
                    place.lastSynced = QDateTime::currentDateTimeUtc();
                    const QString workingCopy = self->newWorkingCopyPath(QFileInfo(fileName).completeBaseName());
                    const QString base = webDavBasePath(place.id);
                    (void)QtConcurrent::run(&self->savePool, [self, client, fail, fileName, place, data, vaultFile,
                                                              account, workingCopy, base]() {
                        const bool written = saveVaultData(workingCopy, data) == SaveStatus::Saved &&
                                             saveVaultData(base, data) == SaveStatus::Saved;
                        QMetaObject::invokeMethod(
                            qApp,
                            [self, client, fail, fileName, place, vaultFile, account, workingCopy, base, written]() {
                                if (!self) {
                                    return;
                                }
                                if (!written) {
                                    QFile::remove(workingCopy);
                                    QFile::remove(base);
                                    fail(tr("This device cannot write the vault's working copy, or the disk is "
                                            "full. The vault stays where it was."),
                                         fileName);
                                    return;
                                }
                                client->deleteLater();
                                rememberWebDavPlace(workingCopy, place);
                                self->keepWebDavPassword(place, account.password);
                                self->finishMove(vaultFile, workingCopy);
                            },
                            Qt::QueuedConnection);
                    });
                });
            }, false);
        });
    });
}

void VaultController::finishMove(const QString& vaultFile, const QString& newPath)
{
    // The list names the new place first: whatever happens after, the vault
    // is found there (OR-15).
    const qsizetype index = vaultList.indexOf(vaultFile);
    if (index >= 0) {
        vaultList[index] = newPath;
        if (VaultsFile().value(QStringLiteral("vaults/primary")).toString() == vaultFile) {
            VaultsFile().setValue(QStringLiteral("vaults/primary"), newPath);
        }
    } else {
        vaultList.append(newPath);
    }
    VaultsFile().setValue(QStringLiteral("vaults/list"), vaultList);
    foundList.removeAll(vaultFile);
    if (!foundList.contains(newPath)) {
        foundList.append(newPath);
    }
    carryVaultSettings(vaultFile, newPath);
    if (returnTo == vaultFile) {
        returnTo = newPath;
        emit returnChanged();
    }
    if (lastUsed == vaultFile) {
        lastUsed = newPath;
        VaultsFile().setValue(QStringLiteral("vault/lastPath"), newPath);
    }
    if (path == vaultFile) {
        setPath(newPath);
        if (currentState != State::NoVault) {
            setState(State::Locked);
        }
    }
    moveRunning = false;
    movingVault.clear();
    emit vaultsChanged();
    emit vaultsFoundChanged();
    emit syncChanged();
    emit moveDone(true, QString());

    // Then the original goes.
    const auto place = webDavPlaceOf(vaultFile);
    if (!place) {
        if (!QFile::remove(vaultFile)) {
            say(tr("The vault was moved, but its old file could not be deleted: %1. Delete it yourself.")
                    .arg(QDir::toNativeSeparators(vaultFile)),
                true);
        }
        return;
    }
    forgetWebDavPlace(vaultFile);
    QFile::remove(vaultFile);
    QFile::remove(webDavBasePath(place->id));
    QPointer<VaultController> self(this);
    withWebDavPassword(*place, [self, place = *place](std::optional<QString> password) {
        if (!self) {
            return;
        }
        const auto tell = [self, place](const QString& why) {
            self->say(tr("The vault was moved, but its old copy on the server could not be deleted (%1): %2. Delete it "
                         "there yourself.")
                          .arg(why, place.shownAddress()),
                      true);
        };
        self->webDavPasswords.remove(place.id);
        if (const std::shared_ptr<SecretStore> store = self->secrets) {
            const QString name = place.secretName();
            (void)QtConcurrent::run([store, name]() { (void)store->remove(name); });
        }
        if (!password) {
            tell(tr("the server's password is not known"));
            return;
        }
        auto* client = new WebDavClient(WebDavAccount{place.folder, place.user, *password, place.trustedCertificate},
                                        self);
        client->remove(place.fileName, [self, client, tell](const WebDavReply& answer) {
            client->deleteLater();
            if (self && !answer.ok() && answer.error != WebDavError::NotFound) {
                tell(connectProblem(answer));
            }
        });
    });
}

void VaultController::moveFailed(const QString& problem)
{
    moveRunning = false;
    movingVault.clear();
    emit moveDone(false, problem);
}

void VaultController::forgetVault(const QString& vaultFile)
{
    if (vaultFile.isEmpty() || vaultFile == primaryVault() || !vaultList.contains(vaultFile) ||
        (moveRunning && movingVault == vaultFile)) {
        return;
    }
    if (vaultFile == path && currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        say(tr("Wait until your last change is saved, then try again."), true);
        return;
    }
    if (vaultFile == path && currentState == State::Unlocked) {
        lockEverything();
        setState(State::Locked);
    }
    if (vaultFile == unsavedPath) {
        discardKeptChanges();
    }
    if (const auto place = webDavPlaceOf(vaultFile)) {
        // The working copy goes at once, a change not yet on the server with
        // it (the owner, D-060); the server's copy stays.
        if (vaultFile == path) {
            stopWebDav();
        }
        forgetWebDavPlace(vaultFile);
        QFile::remove(vaultFile);
        QFile::remove(webDavBasePath(place->id));
        webDavPasswords.remove(place->id);
        if (const std::shared_ptr<SecretStore> store = secrets) {
            const QString name = place->secretName();
            (void)QtConcurrent::run([store, name]() { (void)store->remove(name); });
        }
    }
    carryVaultSettings(vaultFile, QString());
    vaultList.removeAll(vaultFile);
    VaultsFile().setValue(QStringLiteral("vaults/list"), vaultList);
    foundList.removeAll(vaultFile);
    if (missingLast == vaultFile) {
        missingLast.clear();
    }
    if (returnTo == vaultFile) {
        returnTo.clear();
        emit returnChanged();
    }
    if (path == vaultFile) {
        // Back to the primary's unlock page (taken in autonomous mode, Q39).
        setPath(primaryVault());
        setState(path.isEmpty() ? State::NoVault : State::Locked);
        say(QString(), false);
    }
    emit vaultsChanged();
    emit vaultsFoundChanged();
    emit syncChanged();
    checkVaults();
}

} // namespace chrono_vault
