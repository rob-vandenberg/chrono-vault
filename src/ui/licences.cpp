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
#include "licences.h"

#include "crypto.h"
#include "version.h"

#include <QFile>
#include <QtGlobal>

// --- Version ------------------------------------------------------------
// licences.cpp 0.0.8

// --- Version history ----------------------------------------------------
// v0.0.8: Initial release (D-021)

namespace chrono_vault {

namespace {

QString textOf(const QString& resource)
{
    QFile file(resource);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

QList<Licence> shippedLicences()
{
    const QString qt = QString::fromLatin1(qVersion());
    const QString openSsl = QString::fromLatin1(crypto::openSslVersion());
    QList<Licence> licences{
        {Licences::tr("Chrono Vault %1: GNU Affero General Public License, version 3").arg(programVersion()),
         textOf(QStringLiteral(":/licences/AGPL-3.0.txt"))},
        {Licences::tr("Qt %1: GNU Lesser General Public License, version 3").arg(qt),
         textOf(QStringLiteral(":/licences/LGPL-3.0.txt"))},
        {Licences::tr("Qt %1: GNU General Public License, version 3").arg(qt),
         textOf(QStringLiteral(":/licences/GPL-3.0.txt"))},
        {Licences::tr("Qt %1: GNU General Public License, version 2").arg(qt),
         textOf(QStringLiteral(":/licences/GPL-2.0.txt"))},
        {Licences::tr("OpenSSL %1: Apache License, version 2.0").arg(openSsl),
         textOf(QStringLiteral(":/licences/Apache-2.0.txt"))},
        {Licences::tr("Noto Sans and Noto Sans Mono: SIL Open Font License, version 1.1"),
         textOf(QStringLiteral(":/licences/OFL-1.1.txt"))},
    };
#ifdef Q_OS_WIN
    // Shipped beside the program on Windows; Microsoft gives no text to ship.
    licences.append({Licences::tr("Microsoft Visual C++ runtime, redistributed under Microsoft's terms"), {}});
#endif
    return licences;
}

QString Licences::text()
{
    QString html;
    bool first = true;
    for (const Licence& licence : shippedLicences()) {
        if (!first) {
            html += QStringLiteral("<hr/>");
        }
        first = false;
        html += QStringLiteral("<p><b>%1</b></p>").arg(licence.covers.toHtmlEscaped());
        if (!licence.text.isEmpty()) {
            // Line by line as the text has it: in Qt's rich text a plain line
            // break would start a new paragraph, with space around it.
            QString lines = licence.text.toHtmlEscaped();
            lines.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            lines.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
            html += QStringLiteral("<p style=\"white-space: pre-wrap;\">%1</p>").arg(lines);
        }
    }
    return html;
}

} // namespace chrono_vault
