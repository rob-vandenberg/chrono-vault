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

#include "google_drive_storage.h"

#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

// --- Version ------------------------------------------------------------
// vault_controller_googledrive.cpp 0.8.116

// --- Version history ----------------------------------------------------
// v0.8.116: Every sign-in in the Google Drive window opens the browser; a vault's own sign-in is not reused there (D-080 point 13)
// v0.8.115: Initial release: sign in to Google Drive, its folders, and a vault made, opened or moved there (D-080)

// The controller's Google Drive part (D-080). The sync, the working copy and
// the merge are the same as for WebDAV (CloudSync); here is only what is
// Google Drive's own: the sign-in in the browser, and the folders.

namespace chrono_vault {

namespace {

const QString googleDriveKind = QStringLiteral("googledrive");

} // namespace

QString VaultController::cloudProblem(const QString& kind, const CloudReply& answer)
{
    if (kind != googleDriveKind) {
        return connectProblem(answer);
    }
    switch (answer.error) {
    case CloudError::None:
        return QString();
    case CloudError::NoConnection:
        return tr("Google Drive cannot be reached. Check the network.");
    case CloudError::Unauthorized:
        return tr("Google no longer accepts Chrono Vault's sign-in. Sign in to Google Drive again.");
    case CloudError::NotFound:
        return tr("That folder or vault is no longer on Google Drive.");
    case CloudError::TooLarge:
        return tr("Google Drive is full, or the file is too large to be a vault.");
    case CloudError::Certificate:
    case CloudError::Changed:
    case CloudError::Locked:
    case CloudError::NotSupported:
    case CloudError::Damaged:
    case CloudError::Server:
    case CloudError::Superseded:
        break;
    }
    return tr("Google Drive refused it (%1).").arg(answer.status);
}

std::optional<CloudPlace> VaultController::driveTarget() const
{
    if (!drive) {
        return std::nullopt;
    }
    CloudPlace place;
    place.kind = googleDriveKind;
    place.details.insert(QStringLiteral("folderId"), drive->account().folderId);
    place.details.insert(QStringLiteral("folderPath"), drive->account().folderPath);
    return place;
}

void VaultController::connectGoogleDrive()
{
    if (!googleDriveAvailable()) {
        emit googleDriveConnected(false, tr("This version of Chrono Vault has no Google Drive."));
        return;
    }
    // Every sign-in here opens the browser, so each vault created, added or
    // moved here keeps the Google account picked for it (D-080 point 13).
    googleToken.clear();
    drive.reset();
    driveSigningIn.reset();
    driveFiles.clear();
    // The browser opens, the person signs in at Google, and the answer
    // comes back to Chrono Vault on this computer (D-080).
    googleSignIn = std::make_unique<GoogleSignIn>();
    if (googleBrowserForTests) {
        googleSignIn->setBrowserForTests(googleBrowserForTests);
    }
    GoogleSignIn* asking = googleSignIn.get();
    QPointer<VaultController> self(this);
    connect(asking, &GoogleSignIn::finished, this,
            [self, asking](bool ok, const QString& refreshToken, const QString& problem) {
                if (!self || self->googleSignIn.get() != asking) {
                    return;
                }
                self->googleSignIn.release()->deleteLater();
                if (!ok) {
                    emit self->googleDriveConnected(false, problem);
                    return;
                }
                self->googleToken = refreshToken;
                self->showGoogleDrive();
            });
    asking->start();
}

void VaultController::cancelGoogleSignIn()
{
    if (googleSignIn) {
        googleSignIn.release()->deleteLater();
    }
}

void VaultController::showGoogleDrive()
{
    auto storage = std::make_unique<GoogleDriveStorage>(GoogleDriveAccount{googleToken, {}, {}});
    GoogleDriveStorage* asking = storage.get();
    driveSigningIn = std::move(storage);
    QPointer<VaultController> self(this);
    asking->signIn([self, asking](const CloudReply& answer) {
        if (!self || self->driveSigningIn.get() != asking) {
            return;
        }
        if (!answer.ok()) {
            self->driveSigningIn.reset();
            if (answer.error == CloudError::Unauthorized) {
                self->googleToken.clear(); // taken away: the browser next time
            }
            emit self->googleDriveConnected(false, cloudProblem(googleDriveKind, answer));
            return;
        }
        self->drive = std::move(self->driveSigningIn);
        self->driveFiles.clear();
        self->listGoogleDrive(true);
    });
}

void VaultController::listGoogleDrive(bool connecting)
{
    if (!drive) {
        return;
    }
    GoogleDriveStorage* asking = drive.get();
    QPointer<VaultController> self(this);
    asking->list([self, asking, connecting](const CloudReply& answer, const QList<CloudEntry>& entries) {
        if (!self || self->drive.get() != asking) {
            return;
        }
        if (!answer.ok()) {
            if (connecting) {
                emit self->googleDriveConnected(false, cloudProblem(googleDriveKind, answer));
            } else {
                emit self->webDavDone(false, cloudProblem(googleDriveKind, answer));
            }
            return;
        }
        QStringList files;
        const QVariantList shown = self->shownEntries(entries, files);
        self->driveFiles = files;
        if (connecting) {
            emit self->googleDriveConnected(true, QString());
        }
        emit self->webDavFolderShown(asking->account().folderPath, shown, files);
    });
}

void VaultController::openGoogleDriveFolder(const QString& name)
{
    if (!drive) {
        return;
    }
    bool entered = false;
    if (name == QLatin1String("..") || !name.contains(QLatin1Char('/'))) {
        entered = name != drive->account().folderPath && drive->enterFolder(name);
        if (!entered && name == drive->account().folderPath) {
            entered = true; // the folder shown, listed again
        }
    } else {
        // A whole path, for Back in the file dialog: up to it.
        while (drive->account().folderPath != name && !drive->atTop() && name.size() < drive->account().folderPath.size()) {
            (void)drive->enterFolder(QStringLiteral(".."));
        }
        entered = drive->account().folderPath == name;
    }
    if (!entered) {
        emit webDavDone(false, tr("That folder is not on Google Drive."));
        return;
    }
    listGoogleDrive(false);
}

void VaultController::makeGoogleDriveFolder(const QString& name)
{
    if (!drive) {
        return;
    }
    const QString problem = webDavNameProblem(name);
    if (!problem.isEmpty()) {
        emit webDavDone(false, problem);
        return;
    }
    GoogleDriveStorage* asking = drive.get();
    const QString clean = name.trimmed();
    QPointer<VaultController> self(this);
    asking->makeFolder(clean, [self, asking, clean](const CloudReply& answer) {
        if (!self || self->drive.get() != asking) {
            return;
        }
        if (!answer.ok()) {
            emit self->webDavDone(false, answer.status == 405 ? tr("A folder or file with that name is already there.")
                                                             : cloudProblem(googleDriveKind, answer));
            return;
        }
        self->openGoogleDriveFolder(clean); // into the new folder
    });
}

void VaultController::createGoogleDriveVault(const QString& name, const QString& password, const QString& repeated,
                                             const QUrl& keyfile, bool savePassword)
{
    createCloudVault(driveTarget(), googleToken, driveFiles, name, password, repeated, keyfile, savePassword);
}

void VaultController::openGoogleDriveVault(const QString& fileName)
{
    const auto target = driveTarget();
    if (!target || !driveFiles.contains(fileName)) {
        emit webDavDone(false, tr("Sign in to Google Drive first."));
        return;
    }
    GoogleDriveStorage* asking = drive.get();
    QPointer<VaultController> self(this);
    openCloudVault(
        *target, googleToken, fileName,
        [self, asking](const QString& file, CloudStorage::Done done) {
            if (self && self->drive.get() == asking) {
                asking->get(file, {}, std::move(done));
            }
        },
        [self, asking]() { return self && self->drive.get() == asking; });
}

void VaultController::moveVaultToGoogleDrive(const QString& vaultFile)
{
    moveVaultToCloud(vaultFile, driveTarget(), googleToken, driveFiles, tr("Sign in to Google Drive first."));
}

void VaultController::disconnectVault(const QString& vaultFile, bool alsoDelete)
{
    const auto place = cloudPlaceOf(vaultFile);
    if (!place) {
        return;
    }
    if (!alsoDelete || place->kind != googleDriveKind) {
        disconnectWebDav(vaultFile); // nothing deleted (D-060)
        return;
    }
    // The copy on Google Drive goes to its trash, where the person can still
    // get it back; this device's copy stays a vault of its own (item 12).
    // The sign-in is read before Disconnect removes it from the system's store.
    QPointer<VaultController> self(this);
    const CloudPlace where = *place;
    withCloudSecret(where, [self, where, vaultFile](std::optional<QString> secret) {
        if (!self) {
            return;
        }
        self->disconnectWebDav(vaultFile);
        if (!secret) {
            self->say(tr("The vault on Google Drive could not be deleted: sign in to Google Drive first. It was "
                         "disconnected; delete it there yourself."),
                      true);
            return;
        }
        CloudStorage* storage = makeCloudStorage(where, *secret).release();
        if (!storage) {
            return;
        }
        storage->setParent(self);
        storage->remove(where.fileName, [self, storage, where](const CloudReply& answer) {
            storage->deleteLater();
            if (self && !answer.ok() && answer.error != CloudError::NotFound) {
                self->say(tr("The vault on Google Drive could not be deleted (%1): %2. Delete it there yourself.")
                              .arg(cloudProblem(googleDriveKind, answer), shownCloudAddress(where)),
                          true);
            }
        });
    });
}

void VaultController::signInToCloudAgain()
{
    const auto place = cloudPlaceOf(path);
    if (!place || place->kind != googleDriveKind || currentState != State::Unlocked) {
        return;
    }
    googleSignIn = std::make_unique<GoogleSignIn>();
    if (googleBrowserForTests) {
        googleSignIn->setBrowserForTests(googleBrowserForTests);
    }
    GoogleSignIn* asking = googleSignIn.get();
    const quint64 session = vaultSession;
    QPointer<VaultController> self(this);
    connect(asking, &GoogleSignIn::finished, this,
            [self, asking, session, where = *place](bool ok, const QString& refreshToken, const QString& problem) {
                if (!self || self->googleSignIn.get() != asking) {
                    return;
                }
                self->googleSignIn.release()->deleteLater();
                if (!ok) {
                    self->say(problem, true);
                    return;
                }
                if (session != self->vaultSession || self->currentState != State::Unlocked) {
                    return;
                }
                self->keepCloudSecret(where, refreshToken);
                self->stopWebDav();
                self->makeSyncer(where, refreshToken);
            });
    asking->start();
}

} // namespace chrono_vault
