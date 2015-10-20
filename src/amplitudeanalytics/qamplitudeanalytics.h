/*
 *  Qt In-App Analytics
 *
 *  Copyright (c) 2015, Oleksii Serdiuk
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

#ifndef QAMPLITUDEANALYTICS_H
#define QAMPLITUDEANALYTICS_H

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QSslConfiguration>

class QSettings;
class QNetworkAccessManager;
class QNetworkReply;
class QAmplitudeAnalytics: public QObject
{
    Q_OBJECT

public:
    struct OsInfo {
        QString platform;
        QString name;
        QString version;
    };

    struct DeviceInfo {
        QString id;
        QString brand;
        QString manufacturer;
        QString model;
    };

    explicit QAmplitudeAnalytics(const QString &apiKey, QSettings *settings, QObject *parent = 0);
    ~QAmplitudeAnalytics();

signals:

public slots:
    void trackEvent(const QString &eventType,
                    const QVariantMap &eventProperties,
//                    const QVariantMap &userProperties = QVariantMap(),
                    bool delay = false);

private slots:
    void onNetworkReply(QNetworkReply *reply);

private:
    QString m_apiKey;

    DeviceInfo m_device;
    OsInfo m_os;
    QString m_carrier;
    QString m_country;
    QString m_language;

    qint64 m_sessionId;
    quint32 m_lastEventId;

    QStringList m_queue;
    QStringList m_pending;
    QSettings *m_settings;

    QSslConfiguration m_sslConfiguration;
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;

    void saveToSettings();
};

#endif // QAMPLITUDEANALYTICS_H
