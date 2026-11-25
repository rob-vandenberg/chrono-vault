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

// PIN unlock (D-059). The PIN itself is never kept. A key is made from it
// (Argon2id, as a vault's); on disk each vault keeps that key only sealed with
// its own key, so a disk alone never lets a PIN open anything. While a vault is
// unlocked the PIN's key is held, and the vault's key is kept in memory sealed
// with it; at a lock the PIN's key is wiped, so only the PIN opens it again,
// and only until Chrono Vault quits.

#include "vault_controller.h"

#include "crypto.h"
#include "settings_files.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

// --- Version ------------------------------------------------------------
// vault_controller_pin.cpp 0.6.106

// --- Version history ----------------------------------------------------
// v0.6.106: The primary opened for its key store only (D-075)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.4.70: Initial release (D-059)

namespace chrono_vault {

namespace {

const QString pinSaltKey = QStringLiteral("pin/salt");
const QString pinDerivationKey = QStringLiteral("pin/derivation");
const QString pinGroup = QStringLiteral("pin");
const QByteArray pinEntryLabel = QByteArrayLiteral("pin key");
const QByteArray lockedLabel = QByteArrayLiteral("pin locked");

QByteArray derivationOf(const crypto::KdfParameters& parameters)
{
    QByteArray packed;
    QDataStream out(&packed, QIODevice::WriteOnly);
    out << quint8(1) << parameters.memoryKiB << parameters.iterations << parameters.lanes;
    return packed;
}

std::optional<crypto::KdfParameters> derivationFrom(const QByteArray& packed)
{
    QDataStream in(packed);
    quint8 version = 0;
    crypto::KdfParameters parameters;
    in >> version >> parameters.memoryKiB >> parameters.iterations >> parameters.lanes;
    if (in.status() != QDataStream::Ok || version != 1) {
        return std::nullopt;
    }
    return parameters;
}

} // namespace

QString VaultController::pinEntryKey(const QString& vaultFile) const
{
    return pinGroup + QStringLiteral("/vaults/") +
           QString::fromLatin1(QCryptographicHash::hash(vaultFile.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

bool VaultController::pinSet() const
{
    return !VaultsFile().value(pinSaltKey).toByteArray().isEmpty();
}

bool VaultController::pinCanOpen() const
{
    return !path.isEmpty() && pinLocked.contains(path) && pinSet();
}

bool VaultController::pinUnlockReady() const
{
    // When the master password is due again, it is asked, not the PIN.
    return currentState == State::Locked && pinCanOpen() && !reminderDue();
}

void VaultController::setPin(const QString& pin)
{
    if (settingPin) {
        return;
    }
    if (pin.isEmpty()) {
        VaultsFile().remove(pinGroup);
        forgetPinKey();
        for (QByteArray& sealed : pinLocked) {
            crypto::wipe(sealed);
        }
        pinLocked.clear();
        emit pinChanged();
        return;
    }
    if (pin.size() < pinMinimumLength) {
        return;
    }
    settingPin = true;
    emit pinChanged();
    const QByteArray salt = crypto::randomBytes(16);
    const crypto::KdfParameters parameters = kdfParameters;
    QByteArray pinBytes = pin.toUtf8();
    QPointer<VaultController> self(this);
    // Making the key takes a moment: never on the thread that draws (OR-5).
    (void)QtConcurrent::run(&unlockPool, [self, pinBytes, salt, parameters]() mutable {
        std::optional<QByteArray> made = crypto::deriveKey(pinBytes, salt, parameters);
        crypto::wipe(pinBytes);
        QMetaObject::invokeMethod(
            qApp,
            [self, made, salt, parameters]() mutable {
                if (!self) {
                    if (made) {
                        crypto::wipe(*made);
                    }
                    return;
                }
                self->settingPin = false;
                if (!made) {
                    emit self->pinChanged();
                    self->say(tr("The PIN could not be set: not enough memory."), true);
                    return;
                }
                // A new PIN: everything kept for the old one goes.
                VaultsFile settings;
                settings.remove(pinGroup);
                settings.setValue(pinSaltKey, salt);
                settings.setValue(pinDerivationKey, derivationOf(parameters));
                for (QByteArray& sealed : self->pinLocked) {
                    crypto::wipe(sealed);
                }
                self->pinLocked.clear();
                self->forgetPinKey();
                self->pinKey = *made;
                crypto::wipe(*made);
                if (self->currentState == State::Unlocked) {
                    self->keepForPin();
                    // The primary, unlocked alongside, is kept as well (D-030).
                    if (self->primaryKey.isValid() && self->primaryVault() != self->path) {
                        const QString primary = self->primaryVault();
                        settings.setValue(self->pinEntryKey(primary),
                                          crypto::seal(self->primaryKey.key, self->pinKey, pinEntryLabel));
                        self->pinLocked.insert(
                            primary, crypto::seal(self->pinKey, quickSecretOf(self->primaryKey, self->primaryKeyfile),
                                                  lockedLabel));
                    }
                }
                emit self->pinChanged();
            },
            Qt::QueuedConnection);
    });
}

void VaultController::keepForPin()
{
    if ((currentState != State::Unlocked && !onlyKeyStore) || !key.isValid() || !pinSet()) {
        return;
    }
    VaultsFile settings;
    const QString entry = pinEntryKey(path);
    if (pinKey.isEmpty()) {
        // After a start, the vault's own key gives the PIN's back.
        const QByteArray sealed = settings.value(entry).toByteArray();
        if (sealed.isEmpty()) {
            return; // this vault has not been unlocked while the PIN's key was known
        }
        std::optional<QByteArray> opened = crypto::open(key.key, sealed, pinEntryLabel);
        if (!opened) {
            // Made with a key this vault no longer has: the PIN is not offered
            // for it until it is set up again.
            settings.remove(entry);
            pinLocked.remove(path);
            emit pinChanged();
            return;
        }
        pinKey = *opened;
        crypto::wipe(*opened);
    }
    // Written again each time, so it follows a changed master password.
    settings.setValue(entry, crypto::seal(key.key, pinKey, pinEntryLabel));
    QByteArray packed = quickSecretOf(key, keyfileSecret);
    pinLocked.insert(path, crypto::seal(pinKey, packed, lockedLabel));
    crypto::wipe(packed);
    emit pinChanged();
}

void VaultController::forgetPinKey()
{
    crypto::wipe(pinKey);
    pinKey.clear();
}

std::optional<VaultController::PinMeans> VaultController::pinMeans() const
{
    VaultsFile settings;
    const QByteArray salt = settings.value(pinSaltKey).toByteArray();
    const std::optional<crypto::KdfParameters> parameters =
        derivationFrom(settings.value(pinDerivationKey).toByteArray());
    if (salt.isEmpty() || !parameters) {
        return std::nullopt;
    }
    return PinMeans{salt, *parameters};
}

QByteArray VaultController::pinLockedLabel()
{
    return lockedLabel;
}

void VaultController::unlockWithPin(const QString& pin)
{
    // An empty field is no attempt, and never counts toward the waits.
    if (!pinUnlockReady() || pin.isEmpty()) {
        return;
    }
    // The same waits, and the same count, as for the master password (D-041).
    updateWait();
    if (waitSeconds > 0) {
        return;
    }
    VaultsFile settings;
    const QByteArray salt = settings.value(pinSaltKey).toByteArray();
    const std::optional<crypto::KdfParameters> parameters =
        derivationFrom(settings.value(pinDerivationKey).toByteArray());
    if (salt.isEmpty() || !parameters) {
        say(tr("The PIN could not be read. Type the master password."), true);
        return;
    }
    setState(State::Busy);
    say(tr("Unlocking…"), false);
    const QByteArray sealed = pinLocked.value(path);
    const quint64 attempt = ++unlockAttempt;
    QByteArray pinBytes = pin.toUtf8();
    const crypto::KdfParameters derivation = *parameters;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, pinBytes, salt, derivation, sealed, attempt]() mutable {
        std::optional<QByteArray> made = crypto::deriveKey(pinBytes, salt, derivation);
        crypto::wipe(pinBytes);
        std::optional<QByteArray> packed;
        if (made) {
            packed = crypto::open(*made, sealed, lockedLabel);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, made, packed, attempt]() mutable {
                const auto wipeAll = [&made, &packed]() {
                    if (made) {
                        crypto::wipe(*made);
                    }
                    if (packed) {
                        crypto::wipe(*packed);
                    }
                };
                if (!self || attempt != self->unlockAttempt) {
                    wipeAll();
                    return;
                }
                if (!made) {
                    self->setState(State::Locked);
                    self->say(tr("The PIN could not be checked: not enough memory."), true);
                    return;
                }
                std::optional<std::pair<VaultKey, QByteArray>> kept;
                if (packed) {
                    kept = quickSecretFrom(*packed);
                }
                if (!kept) {
                    wipeAll();
                    self->wrongPasswordTyped();
                    self->setState(State::Locked);
                    self->say(tr("That is not the PIN."), true);
                    return;
                }
                self->forgetPinKey();
                self->pinKey = *made;
                wipeAll();
                self->keyfileSecret = kept->second;
                self->openingByPin = true;
                self->startOpenWithKey(kept->first);
                kept->first.wipe();
                crypto::wipe(kept->second);
            },
            Qt::QueuedConnection);
    });
}

} // namespace chrono_vault
