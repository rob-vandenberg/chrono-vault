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

#include "json_export.h"
#include "quick_unlock.h"
#include "settings_files.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QSaveFile>
#include <QScopeGuard>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

// --- Version ------------------------------------------------------------
// vault_controller_ask.cpp 0.6.105

// --- Version history ----------------------------------------------------
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.103: Initial release (D-073)

// The standard dialog's question: a master password, a PIN or a finger for any
// vault. What was right is kept only until the dialog has done its work or was
// closed, and the vault shown is left as it is until then (D-073).

namespace chrono_vault {

namespace {
enum ProvenBy { ByPin, ByFinger };
}

void VaultController::tellAsked(const QString& text, bool isTrouble)
{
    askText = text;
    askIsTrouble = isTrouble;
    emit askChanged();
}

void VaultController::wipeProof()
{
    proofKey.wipe();
    proofKey = {};
    crypto::wipe(proofKeyfile);
    proofKeyfile.clear();
    proofTyped = false;
    crypto::wipe(proofWords);
    proofWords.clear();
}

bool VaultController::askRememberShown() const
{
    return !askedFor.isEmpty() && askedFor != primaryVault();
}

bool VaultController::askRememberPossible() const
{
    return primaryKey.isValid() || (currentState == State::Unlocked && path == primaryVault());
}

void VaultController::updateAskWait()
{
    const int left = waitLeft(askedFor);
    if (left > 0 && !askWaitTimer.isActive()) {
        askWaitTimer.start();
    } else if (left == 0) {
        askWaitTimer.stop();
    }
    if (left != askWait) {
        askWait = left;
        emit askChanged();
    }
}

bool VaultController::askPinReady() const
{
    return !askedFor.isEmpty() && pinSet() && pinLocked.contains(askedFor) && !reminderDueFor(askedFor, true);
}

bool VaultController::askFingerOn() const
{
    return quick && quickAvailable && !askedFor.isEmpty() && quickUnlockEnabled() && quick->has(askedFor)
           && !reminderDueFor(askedFor, true);
}

void VaultController::askFor(const QString& vaultFile)
{
    stopAsking();
    stopQuickUnlock(); // one question to the reader at a time
    askedFor = vaultFile;
    updateAskWait();
    emit askChanged();
}

void VaultController::stopAsking()
{
    ++askAttempt;
    askIsBusy = false;
    if (std::exchange(askingFinger, false) && quick) {
        quick->cancel();
    }
    wipeProof();
    askedFor.clear();
    askNeedsKeyfile = false;
    askText.clear();
    askIsTrouble = false;
    askWait = 0;
    askWaitTimer.stop();
    emit askChanged();
}

void VaultController::setAskKeyfile(const QUrl& keyfile)
{
    if (!askedFor.isEmpty()) {
        rememberKeyfile(askedFor, keyfile.isLocalFile() ? keyfile.toLocalFile() : keyfile.toString());
        tellAsked(QString(), false);
    }
}

void VaultController::proveWithPassword(const QString& password)
{
    updateAskWait();
    if (askedFor.isEmpty() || askIsBusy || password.isEmpty() || askWait > 0) {
        return;
    }
    if (std::exchange(askingFinger, false) && quick) {
        quick->cancel(); // typed: the finger is no longer asked
    }
    wipeProof();
    askIsBusy = true;
    tellAsked(QString(), false);
    QByteArray typed = password.toUtf8();
    const QString vaultFile = askedFor;
    const QString keyfileFile = keyfileOf(askedFor);
    const quint64 attempt = ++askAttempt;
    // The vault shown and open is checked against its own key, as it is open.
    const bool own = currentState == State::Unlocked && askedFor == path;
    const VaultKey current = own ? key : VaultKey{};
    const QByteArray ownKeyfile = own ? keyfileSecret : QByteArray();
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, vaultFile, typed, keyfileFile, attempt, own, current, ownKeyfile]() mutable {
        std::optional<QByteArray> fingerprint = QByteArray();
        OpenResult result;
        if (own) {
            fingerprint = ownKeyfile;
            QByteArray checked = current.usesKeyfile ? typed + ownKeyfile : typed;
            const auto derived = crypto::deriveKey(checked, current.salt, current.parameters);
            crypto::wipe(checked);
            result.status = derived && *derived == current.key ? OpenStatus::Opened : OpenStatus::WrongPasswordOrDamaged;
            if (result.status == OpenStatus::Opened) {
                result.key = current;
                result.key.key.detach();
            }
        } else {
            if (!keyfileFile.isEmpty()) {
                fingerprint = keyfileFingerprint(keyfileFile);
            }
            if (fingerprint) {
                result = openVaultFile(vaultFile, typed, *fingerprint);
                crypto::wipe(result.contents);
            }
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, result, fingerprint, attempt, typed]() mutable {
                if (!self || attempt != self->askAttempt) {
                    result.key.wipe();
                    crypto::wipe(typed);
                    return;
                }
                self->askIsBusy = false;
                QByteArray words = typed;
                words.detach();
                crypto::wipe(typed);
                const auto wiped = qScopeGuard([&words]() { crypto::wipe(words); });
                if (!fingerprint) {
                    self->tellAsked(tr("The keyfile could not be read. Choose it again below."), true);
                    return;
                }
                if (result.status != OpenStatus::Opened) {
                    if (result.status == OpenStatus::WrongPasswordOrDamaged) {
                        self->countWrongPassword(self->askedFor);
                        self->updateAskWait();
                        if (self->askedFor == self->path) {
                            self->updateWait();
                        }
                    }
                    if (result.status == OpenStatus::KeyfileNeeded) {
                        self->askNeedsKeyfile = true;
                    }
                    self->tellAsked(openStatusText(result.status), true);
                    return;
                }
                self->provenNow(result.key, *fingerprint, true, words);
                result.key.wipe();
            },
            Qt::QueuedConnection);
    });
}

void VaultController::proveWithPin(const QString& pin)
{
    updateAskWait();
    if (!askPinReady() || askIsBusy || pin.isEmpty() || askWait > 0) {
        return;
    }
    const std::optional<PinMeans> means = pinMeans();
    if (!means) {
        tellAsked(tr("The PIN could not be read. Type the master password."), true);
        return;
    }
    if (std::exchange(askingFinger, false) && quick) {
        quick->cancel();
    }
    wipeProof();
    askIsBusy = true;
    tellAsked(QString(), false);
    const QByteArray sealed = pinLocked.value(askedFor);
    const PinMeans with = *means;
    const quint64 attempt = ++askAttempt;
    QByteArray pinBytes = pin.toUtf8();
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, pinBytes, with, sealed, attempt]() mutable {
        std::optional<QByteArray> made = crypto::deriveKey(pinBytes, with.salt, with.parameters);
        crypto::wipe(pinBytes);
        std::optional<QByteArray> packed;
        if (made) {
            packed = crypto::open(*made, sealed, pinLockedLabel());
        }
        const bool tried = made.has_value();
        if (made) {
            crypto::wipe(*made);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, tried, packed, attempt]() mutable {
                auto kept = packed ? quickSecretFrom(*packed) : std::nullopt;
                if (packed) {
                    crypto::wipe(*packed);
                }
                if (!self || attempt != self->askAttempt) {
                    if (kept) {
                        kept->first.wipe();
                        crypto::wipe(kept->second);
                    }
                    return;
                }
                if (!kept) {
                    self->askIsBusy = false;
                    if (tried) {
                        self->countWrongPassword(self->askedFor);
                        self->updateAskWait();
                        if (self->askedFor == self->path) {
                            self->updateWait();
                        }
                    }
                    self->tellAsked(tried ? tr("That is not the PIN.")
                                          : tr("The PIN could not be checked: not enough memory."),
                                    true);
                    return;
                }
                self->checkProven(kept->first, kept->second, ByPin);
                kept->first.wipe();
                crypto::wipe(kept->second);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::proveWithFinger()
{
    if (!askFingerOn() || askIsBusy || askingFinger || retrieving || settingUp) {
        return;
    }
    askingFinger = true;
    tellAsked(tr("Waiting for your %1…").arg(quickUnlockName()), false);
    quick->retrieve(askedFor);
}

void VaultController::askedFingerAnswered(int result, const QByteArray& secret)
{
    const auto answer = static_cast<QuickUnlock::Result>(result);
    auto kept = answer == QuickUnlock::Result::Done ? quickSecretFrom(secret) : std::nullopt;
    if (kept) {
        askIsBusy = true;
        checkProven(kept->first, kept->second, ByFinger);
        kept->first.wipe();
        crypto::wipe(kept->second);
        return;
    }
    switch (answer) {
    case QuickUnlock::Result::Cancelled: tellAsked(QString(), false); break;
    case QuickUnlock::Result::Failed:
        tellAsked(tr("That finger was not recognized. Try again, or use the master password."), true);
        break;
    default:
        tellAsked(quick->problem().isEmpty()
                      ? tr("%1 is not available now. Use the master password.").arg(quickUnlockName())
                      : tr("%1 Use the master password.").arg(quick->problem()),
                  true);
        break;
    }
}

// A key kept for the PIN or the finger: right only when the vault still opens
// with it.
void VaultController::checkProven(const VaultKey& candidate, const QByteArray& keyfile, int by)
{
    const QString vaultFile = askedFor;
    const quint64 attempt = ++askAttempt;
    VaultKey tried = candidate;
    tried.key.detach();
    QByteArray triedKeyfile = keyfile;
    triedKeyfile.detach();
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&savePool, [self, vaultFile, tried, triedKeyfile, attempt, by]() mutable {
        OpenResult result = openVaultFileWithKey(vaultFile, tried);
        crypto::wipe(result.contents);
        result.key.wipe();
        const OpenStatus status = result.status;
        QMetaObject::invokeMethod(
            qApp,
            [self, status, tried, triedKeyfile, attempt, by]() mutable {
                if (!self || attempt != self->askAttempt) {
                    tried.wipe();
                    crypto::wipe(triedKeyfile);
                    return;
                }
                self->askIsBusy = false;
                if (status == OpenStatus::Opened) {
                    self->provenNow(tried, triedKeyfile, false);
                } else if (status == OpenStatus::KeyChanged) {
                    // The master password was changed on another computer: what
                    // was kept for it no longer fits (D-038, D-059).
                    if (by == ByPin) {
                        self->pinLocked.remove(self->askedFor);
                        VaultsFile().remove(self->pinEntryKey(self->askedFor));
                        emit self->pinChanged();
                    } else if (self->quick) {
                        self->quick->remove(self->askedFor);
                        emit self->quickUnlockChanged();
                    }
                    self->tellAsked(tr("The master password of this vault was changed on another computer. Type it."), true);
                } else {
                    self->tellAsked(openStatusText(status), true);
                }
                tried.wipe();
                crypto::wipe(triedKeyfile);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::provenNow(const VaultKey& right, const QByteArray& keyfile, bool typed, const QByteArray& words)
{
    forgetWrongPasswords(askedFor);
    updateAskWait();
    if (askedFor == path) {
        updateWait();
    }
    proofKey = right;
    proofKey.key.detach();
    proofKeyfile = keyfile;
    proofKeyfile.detach();
    proofTyped = typed;
    proofWords = words;
    proofWords.detach();
    askNeedsKeyfile = false;
    tellAsked(QString(), false);
    emit proven(askedFor);
}

bool VaultController::openProven(bool remember)
{
    const QString vaultFile = askedFor;
    if (vaultFile.isEmpty() || !proofKey.isValid() || currentState == State::Busy) {
        return false;
    }
    if (currentState == State::Unlocked && vaultFile == path) {
        return true; // it is open already
    }
    if (moveRunning && movingVault == vaultFile) {
        tellAsked(tr("This vault is being moved. Unlock it once the move is done."), true);
        return false;
    }
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        tellAsked(tr("Wait until your last change is saved, then try again."), true);
        return false;
    }
    VaultKey with = proofKey;
    with.key.detach();
    const bool typed = proofTyped;
    QByteArray keyfile = proofKeyfile;
    keyfile.detach();
    if (path != vaultFile) {
        // As switching to it, which closes the one shown (D-030).
        if (currentState == State::Unlocked && path == primaryVault()) {
            rememberPrimary();
        }
        forgetUnlockedVault();
        ++unlockAttempt;
        addVault(vaultFile);
        leavingFor(vaultFile);
        setPath(vaultFile);
        needKeyfile = false;
        emit keyfileChanged();
    }
    keyfileSecret = keyfile;
    openingByTyped = typed;
    startOpenWithKey(with);
    // "Remember my credentials": written into the primary with its key (D-074).
    if (remember && typed && !proofWords.isEmpty() && vaultFile != primaryVault() && primaryKey.isValid()) {
        updateSavedPassword(vaultFile, QString::fromUtf8(proofWords), true);
    }
    with.wipe();
    crypto::wipe(keyfile);
    return true;
}

void VaultController::exportProven(const QUrl& file)
{
    const QString target = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (currentState != State::Unlocked || target.isEmpty() || askedFor != path || !proofKey.isValid()
        || proofKey.key != key.key) {
        emit exportDone(false);
        return;
    }
    const VaultContents snapshot = contents;
    const quint64 session = vaultSession;
    say(tr("Exporting…"), false);
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&filePool, [self, target, snapshot, session]() mutable {
        QByteArray json = writeReferenceExport(snapshot);
        QSaveFile output(target);
        // Readable by the person alone: it holds every password unencrypted.
        const bool written = output.open(QIODevice::WriteOnly)
                             && output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
                             && output.write(json) == json.size() && output.commit();
        crypto::wipe(json);
        QMetaObject::invokeMethod(
            qApp,
            [self, written, target, session]() {
                if (!self || session != self->vaultSession) {
                    return;
                }
                self->say(written ? tr("Exported to “%1”. It holds every password unencrypted: keep it safe and delete "
                                       "it when done.")
                                        .arg(QFileInfo(target).fileName())
                                  : tr("The export could not be written: the folder cannot be written to, or the disk "
                                       "is full."),
                          !written);
                emit self->exportDone(written);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::importProven()
{
    if (!proofKey.isValid()) {
        return;
    }
    VaultKey with = proofKey;
    with.key.detach();
    readForImport(askedFor, QByteArray(), with);
    with.wipe();
}

} // namespace chrono_vault
