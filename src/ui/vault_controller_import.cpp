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

#include "item_differences.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller_import.cpp 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.5.81: A source read for Import and for Retry sync alike; an item deleted for good here simply imported (D-061)
// v0.5.78: Every difference a conflict; an item deleted for good here comes back with a conflict (D-061)
// v0.5.77: Initial release: importing another vault; an import's duplicates skipped or made conflicts (D-060)

// The controller's part for Import… (D-060): another vault read with its
// master password, and every imported item taken in so that nothing is
// written over: an item the vault has keeps its values, and every way it
// differs waits in the Sync conflicts dialog for the person (OR-15, D-061).

namespace chrono_vault {

VaultController::SourceRead VaultController::readVaultSource(const QString& file, QByteArray password,
                                                             const std::optional<VaultKey>& kept,
                                                             const QString& keyfile, const VaultContents& snapshot)
{
    std::optional<QByteArray> fingerprint = QByteArray();
    if (!kept && !keyfile.isEmpty()) {
        fingerprint = keyfileFingerprint(keyfile);
    }
    OpenResult opened;
    if (kept) {
        opened = openVaultFileWithKey(file, *kept);
    } else if (fingerprint) {
        opened = openVaultFile(file, password, *fingerprint);
    }
    crypto::wipe(password);
    SourceRead found;
    ImportResult& result = found.result;
    QString& why = found.why;
    if (!fingerprint) {
        why = tr("The vault's keyfile could not be read.");
    } else if (opened.status == OpenStatus::WrongPasswordOrDamaged) {
        why = tr("That is not the vault's master password, or the vault is damaged.");
        found.passwordNeeded = true;
    } else if (opened.status == OpenStatus::KeyChanged) {
        why = tr("That vault was given another master password.");
        found.passwordNeeded = true;
    } else if (opened.status == OpenStatus::KeyfileNeeded) {
        why = tr("That vault needs its keyfile. Open it once in Chrono Vault with its keyfile, then import it.");
    } else if (opened.status == OpenStatus::NewerFormat) {
        why = tr("That vault was made by a newer Chrono Vault. Nothing was imported.");
    } else if (opened.status != OpenStatus::Opened) {
        why = tr("That file could not be opened as a vault.");
    } else {
        ContentsResult read = contentsFromJson(opened.contents);
        crypto::wipe(opened.contents);
        if (read.status != ContentsStatus::Read) {
            why = tr("That vault was made by a newer Chrono Vault, or is damaged. Nothing was imported.");
        } else {
            result.status = ImportStatus::Read;
            for (const Item& item : std::as_const(read.contents.items)) {
                result.items.append(item);
                result.attachmentCount += static_cast<int>(item.attachments.size());
            }
            // The categories and templates of its own, those this vault
            // does not have as they are: new ones are added, ones that
            // differ wait for the person (D-061).
            for (const Category& category : std::as_const(read.contents.categories)) {
                if (!snapshot.categories.contains(category)) {
                    result.categories.append(category);
                }
            }
            for (const Template& one : std::as_const(read.contents.templates)) {
                if (!snapshot.templates.contains(one)) {
                    result.templates.append(one);
                }
            }
        }
    }
    if (why.isEmpty()) {
        found.key = opened.key;
        found.key.key.detach();
    }
    opened.key.wipe();
    return found;
}

void VaultController::readVaultImport(const QUrl& file, const QString& password)
{
    QByteArray passwordBytes = password.toUtf8();
    readForImport(file.isLocalFile() ? file.toLocalFile() : file.toString(), passwordBytes, std::nullopt);
    crypto::wipe(passwordBytes);
}

void VaultController::readForImport(const QString& source, const QByteArray& password, const std::optional<VaultKey>& kept)
{
    if (currentState != State::Unlocked || source.isEmpty()) {
        return;
    }
    pendingImport.reset();
    if (QFileInfo(source) == QFileInfo(path)) {
        const QString why = tr("That is the vault the items would go into. Choose another vault.");
        emit importRead({{QStringLiteral("ok"), false}, {QStringLiteral("message"), why}});
        return;
    }
    const VaultContents snapshot = contents;
    const quint64 session = vaultSession;
    const QString keyfileFile = keyfileOf(source);
    QByteArray passwordBytes = password;
    passwordBytes.detach();
    std::optional<VaultKey> given = kept;
    if (given) {
        given->key.detach();
    }
    QPointer<VaultController> self(this);
    say(tr("Reading the vault…"), false);
    // Deriving the key is slow: off the window's thread (OR-5).
    (void)QtConcurrent::run(&unlockPool, [self, source, passwordBytes, given, keyfileFile, snapshot, session]() mutable {
        SourceRead read = readVaultSource(source, std::move(passwordBytes), given, keyfileFile, snapshot);
        if (given) {
            given->wipe();
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, read, source, session]() mutable {
                const QString why = read.why;
                ImportResult result = std::move(read.result);
                if (!self || session != self->vaultSession || self->currentState != State::Unlocked) {
                    read.key.wipe();
                    return;
                }
                if (why.isEmpty()) {
                    self->keepSourceKey(source, read.key);
                }
                read.key.wipe();
                QVariantMap summary{{QStringLiteral("ok"), why.isEmpty()}};
                if (!why.isEmpty()) {
                    summary.insert(QStringLiteral("message"), why);
                    self->say(why, true);
                    emit self->importRead(summary);
                    return;
                }
                summary.insert(QStringLiteral("items"), static_cast<int>(result.items.size()));
                summary.insert(QStringLiteral("categories"), static_cast<int>(result.categories.size()));
                summary.insert(QStringLiteral("templates"), static_cast<int>(result.templates.size()));
                summary.insert(QStringLiteral("attachments"), result.attachmentCount);
                self->pendingImport = std::move(result);
                self->pendingSource = QStringLiteral("vault:") + source;
                self->say(QString(), false);
                emit self->importRead(summary);
            },
            Qt::QueuedConnection);
    });
}

bool VaultController::takeImported(const Item& imported, const QDateTime& now, bool* added)
{
    *added = false;
    const auto ours = std::find_if(contents.items.begin(), contents.items.end(),
                                   [&imported](const Item& one) { return one.id == imported.id; });
    if (ours == contents.items.end()) {
        Item item = imported;
        const qsizetype removed = contents.deletedItems.removeIf(
            [&item](const DeletedItem& deletion) { return deletion.id == item.id; });
        if (removed == 0) {
            contents.items.append(item);
            *added = true;
            return false;
        }
        // Deleted here for good, held by the import: imported as any item the
        // vault does not have (the owner). Counted as changed now, so no sync
        // with a device that still holds the deletion drops it (OR-15).
        item.modified = now;
        item.mergeNotes.clear();
        contents.items.append(item);
        *added = true;
        return false;
    }
    // Ours stands; every way the import differs is noted (D-061).
    const NotedDifferences noted = noteDifferences(*ours, imported, now, true, QString(), pendingSource);
    if (noted.changed) {
        ours->modified = now;
    }
    return noted.newConflict;
}

} // namespace chrono_vault
