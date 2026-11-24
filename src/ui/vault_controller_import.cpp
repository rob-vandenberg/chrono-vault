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
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller_import.cpp 0.5.77

// --- Version history ----------------------------------------------------
// v0.5.77: Initial release: importing another vault; an import's duplicates skipped or made conflicts (D-060)

// The controller's part for Import… (D-060): another vault read with its
// master password, and every imported item taken in so that nothing is
// written over: an item the vault has keeps its values, and what differs
// waits in the Sync conflicts dialog (OR-15).

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
                // The categories and templates of its own the items need,
                // those this vault does not have yet.
                for (const Category& category : std::as_const(read.contents.categories)) {
                    if (std::none_of(snapshot.categories.cbegin(), snapshot.categories.cend(),
                                     [&category](const Category& one) { return one.id == category.id; })) {
                        result.categories.append(category);
                    }
                }
                for (const Template& one : std::as_const(read.contents.templates)) {
                    if (std::none_of(snapshot.templates.cbegin(), snapshot.templates.cend(),
                                     [&one](const Template& other) { return other.id == one.id; })) {
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
        // An item this vault deleted for good comes back: its deletion is
        // forgotten, and it counts as changed now, so a sync with a device
        // that still holds the deletion keeps it and asks, never drops it
        // silently (OR-15).
        const qsizetype removed = contents.deletedItems.removeIf(
            [&item](const DeletedItem& deletion) { return deletion.id == item.id; });
        if (removed > 0) {
            item.modified = now;
        }
        contents.items.append(item);
        *added = true;
        return false;
    }
    Item& item = *ours;
    bool changed = false;
    bool conflict = false;
    const auto differs = [&](qint64 fieldUid, const QString& property, const QString& theirs) {
        const MergeNote note{MergeNote::Kind::ChangedOnBoth, fieldUid, property, theirs, now, true};
        const bool noted = std::any_of(item.mergeNotes.cbegin(), item.mergeNotes.cend(), [&](const MergeNote& one) {
            return one.fieldUid == fieldUid && one.property == property && one.otherValue == theirs;
        });
        if (!noted) {
            item.mergeNotes.append(note);
            changed = true;
            conflict = true; // a new conflict; one left from an earlier import is not counted again
        }
    };
    const auto text = [&](QString Item::*member, const char* name) {
        if (item.*member != imported.*member) {
            differs(0, QString::fromLatin1(name), imported.*member);
        }
    };
    text(&Item::title, "title");
    text(&Item::subtitle, "subtitle");
    text(&Item::note, "note");
    text(&Item::website, "website");
    for (const Field& field : imported.fields) {
        if (field.deleted) {
            continue;
        }
        const auto mine = std::find_if(item.fields.cbegin(), item.fields.cend(),
                                       [&field](const Field& one) { return one.uid == field.uid; });
        if (mine == item.fields.cend()) {
            item.fields.append(field); // a field only the import has: nothing to choose between
            changed = true;
        } else {
            if (mine->value != field.value) {
                differs(field.uid, QString(), field.value);
            }
            // Earlier values only the import holds join the field's history.
            Field& kept = item.fields[std::distance(item.fields.cbegin(), mine)];
            for (const FieldChange& change : field.history) {
                if (!kept.history.contains(change)) {
                    kept.history.append(change);
                    changed = true;
                }
            }
            std::sort(kept.history.begin(), kept.history.end(),
                      [](const FieldChange& a, const FieldChange& b) { return a.changed < b.changed; });
        }
    }
    // What only the import holds is added: nothing to choose between. A
    // different category, Trash or Archive stays as the vault has it, as a
    // sync settles them (D-054 point 4).
    for (const QString& tag : imported.tags) {
        if (!item.tags.contains(tag)) {
            item.tags.append(tag);
            changed = true;
        }
    }
    for (const Attachment& attachment : imported.attachments) {
        if (std::none_of(item.attachments.cbegin(), item.attachments.cend(),
                         [&attachment](const Attachment& one) { return one.id == attachment.id; })) {
            item.attachments.append(attachment);
            changed = true;
        }
    }
    if (changed) {
        item.modified = now;
    }
    return conflict;
}

} // namespace chrono_vault
