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
// vault_controller_import.cpp 0.5.78

// --- Version history ----------------------------------------------------
// v0.5.78: Every difference a conflict; an item deleted for good here comes back with a conflict (D-061)
// v0.5.77: Initial release: importing another vault; an import's duplicates skipped or made conflicts (D-060)

// The controller's part for Import… (D-060): another vault read with its
// master password, and every imported item taken in so that nothing is
// written over: an item the vault has keeps its values, and every way it
// differs waits in the Sync conflicts dialog for the person (OR-15, D-061).

namespace chrono_vault {

void VaultController::readVaultImport(const QUrl& file, const QString& password)
{
    const QString source = file.isLocalFile() ? file.toLocalFile() : file.toString();
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
    QByteArray passwordBytes = password.toUtf8();
    QPointer<VaultController> self(this);
    say(tr("Reading the vault…"), false);
    // Deriving the key is slow: off the window's thread (OR-5).
    (void)QtConcurrent::run(&unlockPool, [self, source, passwordBytes, keyfileFile, snapshot, session]() mutable {
        std::optional<QByteArray> fingerprint = QByteArray();
        if (!keyfileFile.isEmpty()) {
            fingerprint = keyfileFingerprint(keyfileFile);
        }
        OpenResult opened;
        if (fingerprint) {
            opened = openVaultFile(source, passwordBytes, *fingerprint);
        }
        crypto::wipe(passwordBytes);
        ImportResult result;
        QString why;
        if (!fingerprint) {
            why = tr("The vault's keyfile could not be read.");
        } else if (opened.status == OpenStatus::WrongPasswordOrDamaged) {
            why = tr("That is not the vault's master password, or the vault is damaged.");
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
        opened.key.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, result, why, session]() mutable {
                if (!self || session != self->vaultSession || self->currentState != State::Unlocked) {
                    return;
                }
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
        // Deleted here for good, held by the import: the person chooses.
        // Until then it is kept, counted as changed now, so a sync with a
        // device that still holds the deletion keeps it and asks, never drops
        // it silently (OR-15, D-061).
        item.modified = now;
        item.mergeNotes.clear();
        MergeNote note;
        note.property = QStringLiteral("deletedHere");
        note.when = now;
        note.imported = true;
        item.mergeNotes.append(note);
        contents.items.append(item);
        return true;
    }
    // Ours stands; every way the import differs is noted (D-061).
    const NotedDifferences noted = noteDifferences(*ours, imported, now, true, QString());
    if (noted.changed) {
        ours->modified = now;
    }
    return noted.newConflict;
}

} // namespace chrono_vault
