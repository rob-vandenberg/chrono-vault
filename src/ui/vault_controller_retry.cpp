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
#include "settings_files.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller_retry.cpp 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: Initial release: Retry sync (D-061)

// The controller's part for Retry sync (D-061): an item with a conflict read
// again from where its other values came from, the import's file or vault or
// the shared vault, and compared afresh. A source that cannot be read, or
// that no longer holds the item, leaves the conflict as it is (OR-15).

namespace chrono_vault {

namespace {

const QString exportPrefix = QStringLiteral("export:");
const QString vaultSourcePrefix = QStringLiteral("vault:");
const QByteArray sourceKeyLabel = QByteArrayLiteral("import source");
const QString windowsPrefix = QStringLiteral("windows/");

QString fileOfSource(const QString& source)
{
    return source.mid(source.indexOf(QLatin1Char(':')) + 1);
}

} // namespace

QString VaultController::windowValue(const QString& name) const
{
    return SettingsFile().value(windowsPrefix + name).toString();
}

void VaultController::keepWindowValue(const QString& name, const QString& value)
{
    SettingsFile().setValue(windowsPrefix + name, value);
}

VaultController::SourceRead VaultController::readExportSource(const QString& file, const VaultContents& snapshot)
{
    SourceRead found;
    QFile input(file);
    const bool readable = input.open(QIODevice::ReadOnly);
    if (!readable) {
        found.result.status = ImportStatus::NotTheFormat;
    } else if (input.size() > largestImportBytes) {
        found.result.status = ImportStatus::TooLarge;
    } else {
        QByteArray json = input.read(largestImportBytes + 1);
        found.result = readReferenceExport(json, snapshot);
        crypto::wipe(json);
    }
    if (found.result.status != ImportStatus::Read) {
        found.why = !readable                                           ? tr("The file could not be read.")
                    : found.result.status == ImportStatus::TooLarge     ? tr("The file is too large to be an export.")
                    : found.result.status == ImportStatus::NotTheFormat ? tr("This file is not an export of %1.").arg(referenceProgramName())
                                                                        : tr("The export is damaged. Nothing was imported.");
    }
    return found;
}

// An imported vault's key, never its password, sealed until the lock, so
// Retry sync need not ask the password again.
void VaultController::keepSourceKey(const QString& file, const VaultKey& sourceKey)
{
    QByteArray packed = quickSecretOf(sourceKey, QByteArray());
    sourceKeys.insert(file, crypto::seal(key.key, packed, sourceKeyLabel));
    crypto::wipe(packed);
}

void VaultController::retryConflicts(const QStringList& itemIds)
{
    if (currentState != State::Unlocked || itemIds.isEmpty()) {
        return;
    }
    bool shared = false;
    QStringList sources;
    QHash<QString, QStringList> idsOf;
    const auto look = [&](const QString& id, const QList<MergeNote>& notes) {
        for (const MergeNote& note : notes) {
            if (!note.imported) {
                shared = true;
            } else if (!note.source.isEmpty()) {
                if (!sources.contains(note.source)) {
                    sources.append(note.source);
                }
                if (!idsOf[note.source].contains(id)) {
                    idsOf[note.source].append(id);
                }
            }
        }
    };
    for (const QString& id : itemIds) {
        if (id.startsWith(QLatin1String("vault:"))) {
            shared = true; // the person's own categories and templates
        }
    }
    for (const Item& item : std::as_const(contents.items)) {
        const QString id = item.id.toString(QUuid::WithoutBraces);
        if (itemIds.contains(id)) {
            look(id, item.mergeNotes);
        }
    }
    if (shared) {
        retryShared();
    }
    for (const QString& source : std::as_const(sources)) {
        retryFrom(source, idsOf.value(source), QString());
    }
}

void VaultController::retryFromVault(const QString& source, const QStringList& itemIds, const QString& password)
{
    if (currentState == State::Unlocked && source.startsWith(vaultSourcePrefix) && !password.isEmpty()) {
        retryFrom(source, itemIds, password);
    }
}

// The shared vault read again: what another device settled or changed there
// is merged as at any sync.
void VaultController::retryShared()
{
    if (webDavPlaceOf(path)) {
        if (passwordNeeded) {
            emit retried(tr("Not synchronized: the server's password is needed. The conflict stays."), true);
            return;
        }
        retryWaiting = true;
        syncNow();
        return;
    }
    const QString vaultFile = path;
    const quint64 session = vaultSession;
    VaultKey kept = key;
    kept.key = QByteArray(key.key.constData(), key.key.size());
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&savePool, [self, vaultFile, session, kept]() mutable {
        OpenResult opened = openVaultFileWithKey(vaultFile, kept);
        kept.wipe();
        crypto::wipe(opened.contents);
        opened.key.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, vaultFile, session, status = opened.status, stamp = opened.stamp]() {
                if (!self || session != self->vaultSession || self->path != vaultFile ||
                    self->currentState != State::Unlocked) {
                    return;
                }
                if (status != OpenStatus::Opened) {
                    emit self->retried(tr("“%1” could not be read. The conflict stays.")
                                           .arg(QFileInfo(vaultFile).completeBaseName()),
                                       true);
                    return;
                }
                if (stamp == self->stamps->get(vaultFile)) {
                    emit self->retried(tr("Nothing changed in “%1” since: the conflict stays.")
                                           .arg(QFileInfo(vaultFile).completeBaseName()),
                                       false);
                    return;
                }
                self->checkForChangeElsewhere();
                emit self->retried(tr("“%1” was read again. A conflict still listed is the same there.")
                                       .arg(QFileInfo(vaultFile).completeBaseName()),
                                   false);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::retryFrom(const QString& source, const QStringList& itemIds, const QString& password)
{
    const QString file = fileOfSource(source);
    const VaultContents snapshot = contents;
    const quint64 session = vaultSession;
    QPointer<VaultController> self(this);
    const auto answer = [self, source, itemIds, session](SourceRead read) {
        QMetaObject::invokeMethod(
            qApp,
            [self, source, itemIds, session, read]() mutable {
                if (self && session == self->vaultSession && self->currentState == State::Unlocked) {
                    self->retryRead(source, itemIds, read);
                }
                read.key.wipe();
            },
            Qt::QueuedConnection);
    };
    if (source.startsWith(exportPrefix)) {
        (void)QtConcurrent::run(&filePool, [file, snapshot, answer]() { answer(readExportSource(file, snapshot)); });
        return;
    }
    const QString name = QFileInfo(file).completeBaseName();
    std::optional<VaultKey> kept;
    if (password.isEmpty()) {
        // The key kept since the import, when it was not locked since.
        if (auto packed = crypto::open(key.key, sourceKeys.value(file), sourceKeyLabel)) {
            if (const auto secret = quickSecretFrom(*packed)) {
                kept = secret->first;
            }
            crypto::wipe(*packed);
        }
        if (!kept) {
            emit retryPasswordNeeded(source, name, itemIds, QString());
            return;
        }
    }
    QByteArray passwordBytes = password.toUtf8();
    const QString keyfile = keyfileOf(file);
    // Deriving the key is slow: off the window's thread (OR-5).
    (void)QtConcurrent::run(&unlockPool, [file, snapshot, answer, kept, passwordBytes, keyfile]() mutable {
        SourceRead read = readVaultSource(file, passwordBytes, kept, keyfile, snapshot);
        crypto::wipe(passwordBytes);
        if (kept) {
            kept->wipe();
        }
        answer(std::move(read));
    });
}

void VaultController::retryRead(const QString& source, const QStringList& itemIds, const SourceRead& read)
{
    const QString file = fileOfSource(source);
    const QString name = source.startsWith(exportPrefix) ? QFileInfo(file).fileName() : QFileInfo(file).completeBaseName();
    if (read.passwordNeeded) {
        sourceKeys.remove(file);
        emit retryPasswordNeeded(source, name, itemIds, read.why);
        return;
    }
    if (!read.why.isEmpty()) {
        const QString why = QFileInfo::exists(file) ? read.why : tr("The file is not there.");
        emit retried(tr("“%1” could not be read: %2 The conflict stays.").arg(name, why), true);
        return;
    }
    if (source.startsWith(vaultSourcePrefix)) {
        keepSourceKey(file, read.key);
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QStringList gone;
    int same = 0;
    int differ = 0;
    bool tooLarge = false;
    bool changed = false;
    for (Item& item : contents.items) {
        const QString id = item.id.toString(QUuid::WithoutBraces);
        const auto fromHere = [&source](const MergeNote& note) { return note.imported && note.source == source; };
        if (!itemIds.contains(id) || std::none_of(item.mergeNotes.cbegin(), item.mergeNotes.cend(), fromHere)) {
            continue; // settled or deleted meanwhile
        }
        const auto theirs = std::find_if(read.result.items.cbegin(), read.result.items.cend(),
                                         [&item](const Item& one) { return one.id == item.id; });
        if (theirs == read.result.items.cend()) {
            gone.append(item.title);
            ++differ;
            continue;
        }
        qsizetype added = 0;
        for (const Attachment& one : theirs->attachments) {
            added += item.attachments.contains(one) ? 0 : one.data.size();
        }
        if (attachmentBytes() + added > largestAttachmentsTotal) {
            tooLarge = true;
            ++differ;
            continue;
        }
        // A field this side never had stays one it never had.
        QList<qint64> never;
        for (const MergeNote& note : std::as_const(item.mergeNotes)) {
            if (note.property == QLatin1String("present") && note.subject == QLatin1String("never")) {
                never.append(note.fieldUid);
            }
        }
        const Item before = item;
        item.mergeNotes.removeIf(fromHere);
        (void)noteDifferences(item, *theirs, now, true, QString(), source);
        for (MergeNote& note : item.mergeNotes) {
            if (note.property == QLatin1String("present") && never.contains(note.fieldUid)) {
                note.subject = QStringLiteral("never");
            }
            // The same difference as before was found when it was found then.
            for (const MergeNote& old : before.mergeNotes) {
                MergeNote then = note;
                then.when = old.when;
                if (then == old) {
                    note.when = old.when;
                }
            }
        }
        // Nothing new there: the item is not changed by asking.
        if (!std::is_permutation(item.mergeNotes.cbegin(), item.mergeNotes.cend(), before.mergeNotes.cbegin(),
                                 before.mergeNotes.cend()) ||
            item.fields != before.fields) {
            item.modified = now;
            changed = true;
        }
        if (item.mergeNotes.isEmpty()) {
            ++same;
        } else {
            ++differ;
        }
    }
    if (changed) {
        contentsChanged();
        saveInBackground(QString());
    }
    QString said = tr("Read again from “%1”.").arg(name);
    if (same > 0) {
        said += QLatin1Char(' ') + tr("%n item(s) no longer differ.", nullptr, same);
    }
    if (differ > 0) {
        said += QLatin1Char(' ') + tr("%n item(s) still differ.", nullptr, differ);
    }
    if (!gone.isEmpty()) {
        said += QLatin1Char(' ') + tr("No longer there, so left as it was: %1.").arg(gone.join(QStringLiteral(", ")));
    }
    if (tooLarge) {
        said += QLatin1Char(' ') + tr("The attachments of this vault together may hold at most %1.")
                                       .arg(sizeText(largestAttachmentsTotal));
    }
    emit retried(said, !gone.isEmpty() || tooLarge);
}

} // namespace chrono_vault
