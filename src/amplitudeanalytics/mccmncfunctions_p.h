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

#ifndef MCCMNCFUNCTIONS_P_H
#define MCCMNCFUNCTIONS_P_H

#include <QFile>

inline QString findCountryByIso3166(const QString &iso)
{
    if (iso.isEmpty())
        return QString();

    QFile f;
    f.setFileName(QLatin1String(":/qtamplitudeanalytics/csv/iso3166-country-codes.csv"));
    if (!f.open(QFile::Text | QFile::ReadOnly))
        return QString();

    const QString cc = iso.toUpper();
    while (!f.atEnd()) {
        QStringList row = QString::fromUtf8(f.readLine()).split(QLatin1Char(','));
        if (row.at(0) == cc)
            return row.at(1).trimmed();
    }

    return QString();
}

inline QString findCountryByMcc(const QString &mcc)
{
    if (mcc.isEmpty())
        return QString();

    QFile f;
    f.setFileName(QLatin1String(":/qtamplitudeanalytics/csv/mcc-mnc-codes.csv"));
    if (!f.open(QFile::Text | QFile::ReadOnly))
        return QString();

    while (!f.atEnd()) {
        QStringList row = QString::fromLatin1(f.readLine()).split(QLatin1Char(','));
        if (row.at(0) == mcc) {
            return row.at(5).trimmed();
        }
    }

    return QString();
}

inline QString findCarrierByMccMnc(const QString &mcc, const QString &mnc)
{
    if (mcc.isEmpty() || mnc.isEmpty())
        return QString();

    QFile f;
    f.setFileName(QLatin1String("://mcc-mnc-codes.csv"));
    if (!f.open(QFile::Text | QFile::ReadOnly))
        return QString();
    while (!f.atEnd()) {
        QStringList row = QString::fromLatin1(f.readLine()).split(QLatin1Char(','));
        if (row.at(0) == mcc && row.at(2) == mnc) {
            return row.at(7).trimmed();
        }
    }

    return QString();
}

#endif // MCCMNCFUNCTIONS_P_H
