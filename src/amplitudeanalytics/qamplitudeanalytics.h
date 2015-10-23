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

    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)

    Q_PROPERTY(QString appVersion READ appVersion WRITE setAppVersion NOTIFY appVersionChanged)
    Q_PROPERTY(QString userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(QVariantMap persistentUserProperties READ persistentUserProperties
                                                    WRITE setPersistentUserProperties
                                                    NOTIFY persistentUserPropertiesChanged)
    Q_PROPERTY(DeviceInfo deviceInfo READ deviceInfo WRITE setDeviceInfo NOTIFY deviceInfoChanged)
    Q_PROPERTY(LocationInfo locationInfo READ locationInfo
                                         WRITE setLocationInfo
                                         NOTIFY locationInfoChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:

    struct DeviceInfo {
        QString id;
        QString brand;
        QString manufacturer;
        QString model;
        struct OsInfo {
            QString platform;
            QString name;
            QString version;
        } os;
        QString carrier;
    };

    struct LocationInfo {
        QString country;
        QString region;
        QString city;
        QString dma;
        QVariant latitude;
        QVariant longitude;
        QString ip;
    };

    explicit QAmplitudeAnalytics(const QString &apiKey, QSettings *settings, QObject *parent = 0);

    QString apiKey() const;
    void setApiKey(const QString &apiKey);

    QString appVersion() const;
    void setAppVersion(const QString &version);

    QString userId() const;
    void setUserId(const QString &id);

    QVariantMap persistentUserProperties() const;
    void setPersistentUserProperties(const QVariantMap &properties);

    DeviceInfo deviceInfo() const;
    void setDeviceInfo(const DeviceInfo &info);

    LocationInfo locationInfo() const;
    void setLocationInfo(const LocationInfo &info);

    QString language() const;
    void setLanguage(const QString &language);

    ~QAmplitudeAnalytics();

signals:
    void apiKeyChanged();
    void userIdChanged();
    void persistentUserPropertiesChanged();
    void appVersionChanged();
    void deviceInfoChanged();
    void locationInfoChanged();
    void languageChanged();

public slots:
    void trackEvent(const QString &eventType,
                    const QVariantMap &eventProperties,
                    bool postpone = false);

    void trackEvent(const QString &eventType,
                    const QVariantMap &eventProperties,
                    const QVariantMap &userProperties,
                    const QVariant &revenue = QVariant(),
                    bool postpone = false);

private slots:
    void onNetworkReply(QNetworkReply *reply);

private:
    QString m_apiKey;

    QString m_appVersion;

    QString m_userId;
    QVariantMap m_userProperties;

    DeviceInfo m_device;
    LocationInfo m_location;
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

inline bool operator ==(const QAmplitudeAnalytics::DeviceInfo::OsInfo &first,
                        const QAmplitudeAnalytics::DeviceInfo::OsInfo &second)
{
    return first.platform == second.platform && first.name == second.name
           && first.version == second.version;
}

inline bool operator ==(const QAmplitudeAnalytics::DeviceInfo &first,
                        const QAmplitudeAnalytics::DeviceInfo &second)
{
    return first.id == second.id && first.brand == second.brand
           && first.manufacturer == second.manufacturer && first.model == second.model
           && first.os == second.os && first.carrier == second.carrier;
}

inline bool operator ==(const QAmplitudeAnalytics::LocationInfo &first,
                        const QAmplitudeAnalytics::LocationInfo &second)
{
    return first.country == second.country && first.region == second.region
           && first.city == second.city && first.dma == second.dma
           && first.latitude == second.latitude && first.longitude == second.longitude
           && first.ip == second.ip;
}

#endif // QAMPLITUDEANALYTICS_H
