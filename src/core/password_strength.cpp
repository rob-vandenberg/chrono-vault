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

#include "password_strength.h"

#include <QList>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

// --- Version ------------------------------------------------------------
// password_strength.cpp 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

namespace chrono_vault {

namespace {

// What people choose most: written here from what everybody knows, English
// and Dutch.
const QSet<QString>& commonWords()
{
    static const QSet<QString> words = [] {
        const QString all = QStringLiteral(
            "password passwort wachtwoord passw0rd pass welcome welkom admin administrator letmein login master root "
            "guest user test testing secret geheim hello hallo iloveyou lovely love princess prinses sunshine dragon "
            "monkey football voetbal baseball soccer superman batman spiderman pokemon starwars freedom whatever trustno "
            "shadow killer hunter ranger buster soldier cheese chocolate flower bloem summer winter spring autumn zomer "
            "herfst lente monday tuesday wednesday thursday friday saturday sunday maandag dinsdag woensdag donderdag "
            "vrijdag zaterdag zondag january february march april june july august september october november december "
            "januari februari maart juni juli augustus oktober computer internet google facebook apple windows microsoft "
            "samsung android iphone linux ubuntu server router network netwerk default changeme temp temporary company "
            "bedrijf office kantoor family familie mother mama moeder father papa vader baby angel engel schatje liefde "
            "lieverd poes hond cat dog charlie michael jennifer jessica daniel thomas anna emma sophie peter robert "
            "maria lisa laura andrew joshua matthew george amsterdam rotterdam holland nederland ajax feyenoord london "
            "paris berlin america england qwerty azerty qwertz abc xyz money geld bank credit orange banana pepper silver "
            "golden diamond purple blue black white red green yellow rood blauw groen tiger lion eagle wolf bear phoenix "
            "ninja matrix mustang ferrari porsche harley yamaha music muziek guitar rock metal jordan arsenal liverpool "
            "chelsea barcelona madrid god jesus church heaven hell star moon sky sun zon maan home huis school");
        const QStringList list = all.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        return QSet<QString>(list.cbegin(), list.cend());
    }();
    return words;
}

// Letters written as look-alikes, back as letters.
QString plainLetters(const QString& text)
{
    QString plain = text.toLower();
    for (QChar& one : plain) {
        switch (one.unicode()) {
        case '@':
        case '4': one = QLatin1Char('a'); break;
        case '3': one = QLatin1Char('e'); break;
        case '1':
        case '!': one = QLatin1Char('i'); break;
        case '0': one = QLatin1Char('o'); break;
        case '$':
        case '5': one = QLatin1Char('s'); break;
        case '7': one = QLatin1Char('t'); break;
        default: break;
        }
    }
    return plain;
}

double poolBits(const QString& password)
{
    bool lower = false;
    bool upper = false;
    bool digit = false;
    bool symbol = false;
    bool other = false;
    for (const QChar one : password) {
        if (one.unicode() > 126 || one.unicode() < 32) {
            other = true;
        } else if (one.isLower()) {
            lower = true;
        } else if (one.isUpper()) {
            upper = true;
        } else if (one.isDigit()) {
            digit = true;
        } else {
            symbol = true;
        }
    }
    const int pool = (lower ? 26 : 0) + (upper ? 26 : 0) + (digit ? 10 : 0) + (symbol ? 33 : 0) + (other ? 100 : 0);
    return std::log2(std::max(pool, 2));
}

// The length of the run from here on: the same character, the next one each
// time, or the next key of a keyboard row.
qsizetype sequenceLength(const QString& lower, qsizetype from)
{
    static const QStringList rows{QStringLiteral("qwertyuiop"), QStringLiteral("asdfghjkl"), QStringLiteral("zxcvbnm"),
                                  QStringLiteral("azertyuiop"), QStringLiteral("qsdfghjklm"), QStringLiteral("wxcvbn"),
                                  QStringLiteral("qwertzuiop"), QStringLiteral("yxcvbnm"), QStringLiteral("1234567890"),
                                  QStringLiteral("!@#$%^&*()"), QStringLiteral("1qaz2wsx3edc4rfv5tgb6yhn7ujm")};
    qsizetype longest = 1;
    for (const int step : {0, 1, -1}) {
        qsizetype length = 1;
        while (from + length < lower.size()
               && lower.at(from + length).unicode() == lower.at(from + length - 1).unicode() + step) {
            ++length;
        }
        longest = std::max(longest, length);
    }
    for (const QString& row : rows) {
        const qsizetype place = row.indexOf(lower.at(from));
        if (place < 0) {
            continue;
        }
        for (const int step : {1, -1}) {
            qsizetype length = 1;
            while (from + length < lower.size() && place + step * length >= 0 && place + step * length < row.size()
                   && row.at(place + step * length) == lower.at(from + length)) {
                ++length;
            }
            longest = std::max(longest, length);
        }
    }
    return longest;
}

// A run of letters that reads as a word: in one case or with a capital first,
// and with vowels where a word has them.
bool readsAsWord(const QString& text)
{
    qsizetype vowels = 0;
    qsizetype consonants = 0;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar one = text.at(index);
        if (!one.isLetter()) {
            return false;
        }
        if (index > 1 && one.isUpper() != text.at(index - 1).isUpper()) {
            return false;
        }
        if (QStringLiteral("aeiouyàáâäèéêëìíîïòóôöùúûü").contains(one.toLower())) {
            ++vowels;
            consonants = 0;
        } else if (++consonants > 4) {
            return false;
        }
    }
    return text.size() >= 4 && vowels * 4 >= text.size();
}

bool isNumber(const QString& text)
{
    return std::all_of(text.cbegin(), text.cend(), [](QChar one) { return one >= QLatin1Char('0') && one <= QLatin1Char('9'); });
}

bool isDate(const QString& digits)
{
    const auto day = [](int value) { return value >= 1 && value <= 31; };
    const auto month = [](int value) { return value >= 1 && value <= 12; };
    const auto year = [](int value) { return value >= 1900 && value <= 2099; };
    const int first = digits.left(2).toInt();
    const int second = digits.mid(2, 2).toInt();
    if (digits.size() == 4) {
        return year(digits.toInt());
    }
    if (digits.size() == 6) {
        const int third = digits.mid(4, 2).toInt();
        return (day(first) && month(second)) || (month(first) && day(second)) || (month(second) && day(third));
    }
    if (digits.size() == 8) {
        return ((day(first) && month(second)) || (month(first) && day(second))) && year(digits.right(4).toInt())
               ? true
               : year(digits.left(4).toInt()) && month(digits.mid(4, 2).toInt()) && day(digits.right(2).toInt());
    }
    return false;
}

double bitsOf(const QString& password, const QSet<QString>& personal)
{
    const qsizetype size = password.size();
    const double perCharacter = poolBits(password);
    const QString lower = password.toLower();
    const QString plain = plainLetters(password);
    QList<double> cost(size + 1, std::numeric_limits<double>::max());
    cost[0] = 0;
    const auto offer = [&cost](qsizetype end, double bits) { cost[end] = std::min(cost.at(end), bits); };
    for (qsizetype from = 0; from < size; ++from) {
        const double before = cost.at(from);
        offer(from + 1, before + perCharacter);
        if (const qsizetype run = sequenceLength(lower, from); run >= 3) {
            for (qsizetype length = 3; length <= run; ++length) {
                offer(from + length, before + 7.0 + std::log2(double(length)));
            }
        }
        for (qsizetype length = 3; from + length <= size && length <= 24; ++length) {
            const QString part = password.mid(from, length);
            const QString partLower = lower.mid(from, length);
            const QString partPlain = plain.mid(from, length);
            // A changed letter or a capital costs the guesser a bit or two.
            const double dressed = (part != partLower ? 1.0 : 0.0) + (partPlain != partLower ? 1.0 : 0.0);
            if (personal.contains(partLower) || personal.contains(partPlain)) {
                offer(from + length, before + 5.0 + dressed);
            } else if (commonWords().contains(partLower) || commonWords().contains(partPlain)) {
                offer(from + length, before + 8.0 + dressed);
            } else if (readsAsWord(part)) {
                offer(from + length, before + 2.5 * double(length) + dressed);
            }
            if ((length == 4 || length == 6 || length == 8) && isNumber(part) && isDate(part)) {
                offer(from + length, before + (length == 4 ? 7.5 : 15.0));
            }
        }
    }
    return cost.at(size);
}

} // namespace

double passwordBits(const QString& password, const QStringList& personal)
{
    if (password.isEmpty()) {
        return 0;
    }
    // Past this a longer password is only stronger still.
    const QString looked = password.left(128);
    QSet<QString> own;
    for (const QString& text : personal) {
        QString part;
        const auto keep = [&own, &part]() {
            if (part.size() >= 3) {
                own.insert(part);
            }
            part.clear();
        };
        for (const QChar one : text.toLower()) {
            if (one.isLetterOrNumber()) {
                part.append(one);
            } else {
                keep();
            }
        }
        keep();
    }
    double bits = bitsOf(looked, own);
    // A block repeated counts once.
    for (qsizetype block = 1; block * 2 <= looked.size(); ++block) {
        if (looked.size() % block == 0 && looked.left(block).repeated(looked.size() / block) == looked) {
            bits = std::min(bits, bitsOf(looked.left(block), own) + std::log2(double(looked.size() / block)));
            break;
        }
    }
    return bits;
}

} // namespace chrono_vault
