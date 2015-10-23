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

inline QString toJsonString(const QVariant &value);

inline void capitalize(QString &str)
{
    if (str.isEmpty())
        return;

    str[0] = str.at(0).toUpper();
}

inline QString quoteAndEscape(const QString &string)
{
    QString result(string);
    // JSON requires \, ", \b, \f, \n, \r, \t to be escaped
    result.replace('\\', "\\\\");
    result.replace('"', "\\\"");
    result.replace('\b', "\\b");
    result.replace('\f', "\\f");
    result.replace('\n', "\\n");
    result.replace('\r', "\\r");
    result.replace('\t', "\\t");
    return result.prepend("\"").append("\"");
}

inline QString toJson(const QVariantHash &hash)
{
    QStringList json;
    for (QVariantHash::const_iterator i = hash.constBegin(); i != hash.constEnd(); ++i)
        json.append(QString("%1:%2").arg(quoteAndEscape(i.key()), toJsonString(i.value())));
    return json.join(",").prepend("{").append("}");
}

inline QString toJson(const QVariantMap &map)
{
    QStringList json;
    for (QVariantMap::const_iterator i = map.constBegin(); i != map.constEnd(); ++i)
        json.append(QString("%1:%2").arg(quoteAndEscape(i.key()), toJsonString(i.value())));
    return json.join(",").prepend("{").append("}");
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
        return quoteAndEscape(value.toDateTime().toUTC().toString("yyyy-MM-ddTHH:mm:ss.zzzZ"));
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
        return values.join(",").prepend("[").append("]");
    }
    case QVariant::StringList:
    {
        QStringList values;
        foreach (const QVariant &item, value.toStringList()) {
            values.append(toJsonString(item));
        }
        return values.join(",").prepend("[").append("]");
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
        // Not a number: ignore
        qWarning() << value << "is not a number.";
        return "0";
    }
}

#endif // JSONFUNCTIONS_P_H
