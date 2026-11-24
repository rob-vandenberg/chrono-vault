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

#pragma once

#include "vault_contents.h"

#include <QDateTime>
#include <QString>

// --- Version ------------------------------------------------------------
// item_differences.h 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: Keeping both values of a field; a field only the other side has placed as there (D-061)
// v0.5.78: Initial release (D-061)

// Every way two versions of one item differ, kept on the item as notes for
// the person to settle, and a side the person chose put in place (D-061).
// The program never chooses: it cannot know which value is right.

namespace chrono_vault {

struct NotedDifferences
{
    bool changed = false;     // the item changed: a note or a field to settle was added
    bool newConflict = false; // a note was added that was not there yet
};

// "1" or "0", as a note keeps a yes or no.
[[nodiscard]] QString flagText(bool on);

// Adds the note unless an equal one is there already; true when it is new.
bool addNote(QList<MergeNote>& notes, const MergeNote& note);

// Notes on ours every way theirs differs, with no earlier version to judge
// by (an import, or a merge with no base). Ours stands; theirs is kept in the
// notes, a field only theirs has is kept on ours as deleted until the person
// chooses (OR-15). An item, field, tag or attachment only ours has is no
// difference: an import or another device simply lacks it.
// Where theirs came from is kept with the notes (MergeNote::source).
NotedDifferences noteDifferences(Item& ours, const Item& theirs, const QDateTime& now, bool imported,
                                 const QString& device, const QString& source = {});

// A value note kept both ways: a field with the same name, kind and hide
// mark and the other value, directly after the original (D-061). False when
// the note cannot be kept both ways.
bool canKeepBoth(const Item& item, const MergeNote& note);
bool keepBothValues(Item& item, const MergeNote& note, const QDateTime& now);

// The note's other side put in place on the item. The value it replaces
// goes into the field's history; an attachment it replaces is kept beside
// it as an earlier version, so nothing is lost.
void useOtherSide(Item& item, const MergeNote& note, const QDateTime& now);

// A vault note's other side put in place: the other category or template.
void useOtherSide(VaultContents& contents, const MergeNote& note);

} // namespace chrono_vault
