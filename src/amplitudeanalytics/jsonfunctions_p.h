/*
 *  Qt In-App Analytics
 *
 *  Copyright (c) 2015, Oleksii Serdiuk <contacts[at]oleksii[dot]name>
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSONFUNCTIONS_P_H
#define JSONFUNCTIONS_P_H

#include <QLocale>
#include <QDate>
#include <QVariant>
#include <QRegExp>

inline QString toJsonString(const QVariant &value);

inline void capitalize(QString &str)
{
    if (str.isEmpty())
        return;

    str[0] = str.at(0).toUpper();
}

inline QString quoteAndEscape(const QString &string)
{
    static QRegExp rx(QLatin1String("^[+\\-]?[0-9]+(\\.[0-9]+)?$"));
    if (rx.exactMatch(string)) {
        // Seems to be a (floating point) number: doesn't need
        // to be quoted, nothing to escape - return as-is.
        return string;
    }

    QString result(string);
    // JSON requires \, ", \b, \f, \n, \r, \t to be escaped
    result.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    result.replace(QLatin1Char('"'), QLatin1String("\\\""));
    result.replace(QLatin1Char('\b'), QLatin1String("\\b"));
    result.replace(QLatin1Char('\f'), QLatin1String("\\f"));
    result.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    result.replace(QLatin1Char('\r'), QLatin1String("\\r"));
    result.replace(QLatin1Char('\t'), QLatin1String("\\t"));
    return result.prepend(QLatin1Char('"')).append(QLatin1Char('"'));
}

inline QString toJson(const QVariantHash &hash)
{
    QStringList json;
    for (QVariantHash::const_iterator i = hash.constBegin(); i != hash.constEnd(); ++i)
        json.append(quoteAndEscape(i.key()).append(QLatin1Char(':'))
                                           .append(toJsonString(i.value())));
    return json.join(QLatin1String(",")).prepend(QLatin1Char('{')).append(QLatin1Char('}'));
}

inline QString toJson(const QVariantMap &map)
{
    QStringList json;
    for (QVariantMap::const_iterator i = map.constBegin(); i != map.constEnd(); ++i)
        json.append(quoteAndEscape(i.key()).append(QLatin1Char(':'))
                                           .append(toJsonString(i.value())));
    return json.join(QLatin1String(",")).prepend(QLatin1Char('{')).append(QLatin1Char('}'));
}

inline QString toJsonString(const QVariant &value)
{
    switch (value.type()) {
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::Double:
    case QVariant::Bool:
        return value.toString();
        break;
    case QVariant::String:
    case QVariant::Char:
    case QVariant::Url:
        return quoteAndEscape(value.toString());
    case QVariant::Date:
        return quoteAndEscape(value.toDate().toString(Qt::ISODate));
    case QVariant::Time:
        return quoteAndEscape(value.toTime().toString(Qt::ISODate));
    case QVariant::DateTime:
        // Not using Qt::ISODate because we also want to save microseconds
        return quoteAndEscape(value.toDateTime().toUTC().toString(
                                  QLatin1String("yyyy-MM-ddTHH:mm:ss.zzzZ")));
    case QVariant::Locale:
        return quoteAndEscape(QLocale::languageToString(value.toLocale().language()));
    case QVariant::Map:
        return toJson(value.toMap());
    case QVariant::Hash:
        return toJson(value.toHash());
    case QVariant::List:
    {
        QStringList values;
        foreach (const QVariant &item, value.toList()) {
            values.append(toJsonString(item));
        }
        return values.join(QLatin1String(",")).prepend(QLatin1Char('[')).append(QLatin1Char(']'));
    }
    case QVariant::StringList:
    {
        QStringList values;
        foreach (const QVariant &item, value.toStringList()) {
            values.append(toJsonString(item));
        }
        return values.join(QLatin1String(",")).prepend(QLatin1Char('[')).append(QLatin1Char(']'));
    }
    default:
        // unsupported type -> return empty string
        return QString();
    }
    return QString();
}

inline QString doubleToString(const QVariant &value, int precision)
{
    switch (value.type()) {
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::String:
        return value.toString();
    case QVariant::Double:
        return QString::number(value.toDouble(), 'g', precision);
    default:
        // Not a number: return 0
        qWarning() << value << "is not a number.";
        return QLatin1String("0");
    }
}

#endif // JSONFUNCTIONS_P_H
