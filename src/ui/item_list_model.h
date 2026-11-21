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

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// item_list_model.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: The item's icon
// v0.0.3: Initial release

namespace chrono_vault {

// The items of the unlocked vault as the list on the left shows them: sorted
// by title, narrowed by the search text.
class ItemListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The controller owns the list")

public:
    enum Role { IdRole = Qt::UserRole + 1, TitleRole, SubtitleRole, IsNoteRole, IconRole };

    explicit ItemListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(const QList<Item>& items);
    void setSearchText(const QString& text);
    void clear();

    // The row of the item with this id, or -1.
    [[nodiscard]] Q_INVOKABLE int rowOf(const QString& id) const;

private:
    void rebuild();

    QList<Item> allItems;
    QList<Item> shownItems;
    QString search;
};

} // namespace chrono_vault
