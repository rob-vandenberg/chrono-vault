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

// A PIN per vault, and the lock as a screen lock (D-059, D-076). The PIN itself
// is never kept: a key is made from it (Argon2id, as a vault's), and on disk
// each vault keeps that key only sealed with its own key, so a disk alone never
// lets a PIN open anything. At a lock, what was open is sealed in memory under
// the vault the person logged in with: with that vault's key (its password or
// finger gives it back) and with its PIN's key. A PIN opens nothing else, and
// only until Chrono Vault quits.

#include "vault_controller.h"

#include "crypto.h"
#include "settings_files.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

// --- Version ------------------------------------------------------------
// vault_controller_pin.cpp 0.8.128

// --- Version history ----------------------------------------------------
// v0.8.128: The open vault's PIN set without its password (D-076)
// v0.7.107: A PIN per vault; the lock as a screen lock (D-076)
// v0.6.106: The primary opened for its key store only (D-075)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.4.70: Initial release (D-059)

namespace chrono_vault {

namespace {

const QString pinsGroup = QStringLiteral("pins/");
const QString oldPinGroup = QStringLiteral("pin"); // one PIN for every vault, before 0.7.107
const QByteArray pinEntryLabel = QByteArrayLiteral("pin key");
const QByteArray byKeyLabel = QByteArrayLiteral("session");
const QByteArray byPinLabel = QByteArrayLiteral("session pin");

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

// What a lock puts away: the vault that was open, its key, the key store.
struct Session
{
    QString open;
    QByteArray openSecret;
    QByteArray primarySecret;
    QHash<QString, QString> saved;

    void wipe()
    {
        crypto::wipe(openSecret);
        crypto::wipe(primarySecret);
        saved.clear();
    }
};

QByteArray packSession(const Session& session)
{
    QByteArray packed;
    QDataStream out(&packed, QIODevice::WriteOnly);
    out << quint8(1) << session.open << session.openSecret << session.primarySecret << session.saved;
    return packed;
}

std::optional<Session> unpackSession(const QByteArray& packed)
{
    QDataStream in(packed);
    quint8 version = 0;
    Session session;
    in >> version >> session.open >> session.openSecret >> session.primarySecret >> session.saved;
    if (in.status() != QDataStream::Ok || version != 1) {
        session.wipe();
        return std::nullopt;
    }
    return session;
}

} // namespace

QString VaultController::pinGroupOf(const QString& vaultFile)
{
    return pinsGroup +
           QString::fromLatin1(QCryptographicHash::hash(vaultFile.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

void VaultController::dropOldPin()
{
    // The one PIN of every vault may have reached vaults it was never meant
    // for; it is not carried over (the owner, D-076).
    VaultsFile settings;
    if (settings.childGroups().contains(oldPinGroup) || settings.contains(oldPinGroup + QStringLiteral("/salt"))) {
        settings.remove(oldPinGroup);
    }
}

bool VaultController::hasPin(const QString& vaultFile) const
{
    return !vaultFile.isEmpty() && !VaultsFile().value(pinGroupOf(vaultFile) + QStringLiteral("/entry")).toByteArray().isEmpty();
}

bool VaultController::pinUnlockReady() const
{
    // Only after a lock, for the vault logged in with; when the master password
    // is due again, it is asked, not the PIN (D-049).
    return currentState == State::Locked && sessionPinReady() && !reminderDue();
}

bool VaultController::sessionPinReady() const
{
    return !sessionByPin.isEmpty() && path == sessionLogin && hasPin(path);
}

void VaultController::setPinFor(const QString& vaultFile, const QString& pin)
{
    // The open vault's owner needs no password; any other vault only with its
    // own password typed just now (the owner, D-076).
    const bool owner = ownerOf(vaultFile);
    if (settingPin || vaultFile.isEmpty() || (!owner && !typedProofOf(vaultFile)) || pin.size() < pinMinimumLength) {
        return;
    }
    settingPin = true;
    emit pinChanged();
    VaultKey own = owner ? key : proofKey;
    own.key.detach();
    const QByteArray salt = crypto::randomBytes(16);
    const crypto::KdfParameters parameters = kdfParameters;
    QByteArray pinBytes = pin.toUtf8();
    QPointer<VaultController> self(this);
    // Making the key takes a moment: never on the thread that draws (OR-5).
    (void)QtConcurrent::run(&unlockPool, [self, pinBytes, salt, parameters, own, vaultFile]() mutable {
        std::optional<QByteArray> made = crypto::deriveKey(pinBytes, salt, parameters);
        crypto::wipe(pinBytes);
        QByteArray entry;
        if (made) {
            entry = crypto::seal(own.key, *made, pinEntryLabel);
            crypto::wipe(*made);
        }
        own.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, entry, salt, parameters, vaultFile]() {
                if (!self) {
                    return;
                }
                self->settingPin = false;
                if (entry.isEmpty()) {
                    emit self->pinChanged();
                    emit self->vaultMenuProblem(tr("The PIN could not be set: not enough memory."));
                    return;
                }
                VaultsFile settings;
                const QString group = pinGroupOf(vaultFile);
                settings.remove(group);
                settings.setValue(group + QStringLiteral("/salt"), salt);
                settings.setValue(group + QStringLiteral("/derivation"), derivationOf(parameters));
                settings.setValue(group + QStringLiteral("/entry"), entry);
                ++self->pinCount;
                emit self->pinChanged();
            },
            Qt::QueuedConnection);
    });
}

void VaultController::removePinFor(const QString& vaultFile)
{
    if (vaultFile.isEmpty()) {
        return;
    }
    VaultsFile().remove(pinGroupOf(vaultFile));
    ++pinCount;
    if (vaultFile == sessionLogin) {
        crypto::wipe(sessionByPin);
        sessionByPin.clear();
    }
    emit pinChanged();
}

void VaultController::resealPin(const QString& vaultFile, const VaultKey& oldKey, const VaultKey& newKey)
{
    // A changed master password keeps the PIN (D-059).
    VaultsFile settings;
    const QString name = pinGroupOf(vaultFile) + QStringLiteral("/entry");
    const QByteArray entry = settings.value(name).toByteArray();
    if (entry.isEmpty()) {
        return;
    }
    std::optional<QByteArray> pinKey = crypto::open(oldKey.key, entry, pinEntryLabel);
    if (!pinKey) {
        return;
    }
    settings.setValue(name, crypto::seal(newKey.key, *pinKey, pinEntryLabel));
    crypto::wipe(*pinKey);
}

void VaultController::sealSession()
{
    dropSession();
    // Logged in with the primary while its key store is open, else with the
    // vault that is open (D-076).
    const QString primary = primaryVault();
    const bool throughPrimary = primaryKey.isValid() && foundList.contains(primary);
    if (!throughPrimary && currentState != State::Unlocked) {
        return;
    }
    const QString login = throughPrimary ? primary : path;
    const VaultKey& loginKey = throughPrimary ? primaryKey : key;
    Session session;
    if (currentState == State::Unlocked) {
        session.open = path;
        session.openSecret = quickSecretOf(key, keyfileSecret);
    }
    if (throughPrimary) {
        session.primarySecret = quickSecretOf(primaryKey, primaryKeyfile);
        session.saved = savedPasswords;
    }
    QByteArray packed = packSession(session);
    session.wipe();
    sessionLogin = login;
    sessionByKey = crypto::seal(loginKey.key, packed, byKeyLabel);
    VaultsFile settings;
    const QString entryName = pinGroupOf(login) + QStringLiteral("/entry");
    const QByteArray entry = settings.value(entryName).toByteArray();
    if (!entry.isEmpty()) {
        if (std::optional<QByteArray> pinKey = crypto::open(loginKey.key, entry, pinEntryLabel)) {
            sessionByPin = crypto::seal(*pinKey, packed, byPinLabel);
            crypto::wipe(*pinKey);
        } else {
            // Made with a key this vault no longer has: set it up again.
            settings.remove(pinGroupOf(login));
        }
    }
    crypto::wipe(packed);
    emit pinChanged();
}

void VaultController::dropSession()
{
    if (sessionLogin.isEmpty() && sessionByKey.isEmpty() && sessionByPin.isEmpty()) {
        return;
    }
    sessionLogin.clear();
    crypto::wipe(sessionByKey);
    sessionByKey.clear();
    crypto::wipe(sessionByPin);
    sessionByPin.clear();
    crypto::wipe(returnSecret);
    returnSecret.clear();
    emit pinChanged();
}

bool VaultController::takeBackSession(const QByteArray& opener, bool byPin)
{
    std::optional<QByteArray> packed = crypto::open(opener, byPin ? sessionByPin : sessionByKey, byPin ? byPinLabel : byKeyLabel);
    if (!packed) {
        return false;
    }
    std::optional<Session> session = unpackSession(*packed);
    crypto::wipe(*packed);
    if (!session) {
        return false;
    }
    if (auto store = quickSecretFrom(session->primarySecret)) {
        primaryKey.wipe();
        primaryKey = store->first;
        crypto::wipe(primaryKeyfile);
        primaryKeyfile = store->second;
        savedPasswords = session->saved;
        tellKept();
    }
    // The vault that was open; it comes back by itself when it is not the one
    // logged in with.
    crypto::wipe(returnSecret);
    returnSecret = session->openSecret;
    returnSecret.detach();
    if (!session->open.isEmpty() && session->open != sessionLogin && vaultList.contains(session->open)) {
        returnTo = session->open;
        emit returnChanged();
    }
    session->wipe();
    return true;
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
    const QString group = pinGroupOf(path);
    const QByteArray salt = settings.value(group + QStringLiteral("/salt")).toByteArray();
    const std::optional<crypto::KdfParameters> parameters =
        derivationFrom(settings.value(group + QStringLiteral("/derivation")).toByteArray());
    if (salt.isEmpty() || !parameters) {
        say(tr("The PIN could not be read. Type the password."), true);
        return;
    }
    setState(State::Busy);
    say(tr("Unlocking…"), false);
    const quint64 attempt = ++unlockAttempt;
    QByteArray pinBytes = pin.toUtf8();
    const crypto::KdfParameters derivation = *parameters;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, pinBytes, salt, derivation, attempt]() mutable {
        std::optional<QByteArray> made = crypto::deriveKey(pinBytes, salt, derivation);
        crypto::wipe(pinBytes);
        QMetaObject::invokeMethod(
            qApp,
            [self, made, attempt]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    if (made) {
                        crypto::wipe(*made);
                    }
                    return;
                }
                if (!made) {
                    self->setState(State::Locked);
                    self->say(tr("The PIN could not be checked: not enough memory."), true);
                    return;
                }
                const bool right = self->takeBackSession(*made, true);
                crypto::wipe(*made);
                if (!right) {
                    self->wrongPasswordTyped();
                    self->setState(State::Locked);
                    self->say(tr("That is not the PIN."), true);
                    return;
                }
                self->openTakenBack();
            },
            Qt::QueuedConnection);
    });
}

void VaultController::openTakenBack()
{
    // After the PIN: the vault that was open, with the key it had; else the one
    // logged in with, with the key store's own (D-076).
    QByteArray secret;
    if (!returnSecret.isEmpty()) {
        secret = returnSecret;
        secret.detach();
        if (!returnTo.isEmpty()) {
            const QString back = returnTo;
            setPath(back);
        }
    } else if (path == primaryVault() && primaryKey.isValid()) {
        secret = quickSecretOf(primaryKey, primaryKeyfile);
    }
    dropSession();
    auto kept = quickSecretFrom(secret);
    crypto::wipe(secret);
    if (!kept) {
        say(QString(), false);
        setState(State::Locked);
        return;
    }
    keyfileSecret = kept->second;
    openingByPin = true;
    startOpenWithKey(kept->first);
    kept->first.wipe();
    crypto::wipe(kept->second);
}

} // namespace chrono_vault
