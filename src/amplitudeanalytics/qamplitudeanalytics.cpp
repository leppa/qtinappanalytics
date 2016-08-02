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

#include "qamplitudeanalytics.h"

#include "jsonfunctions_p.h"
#include "mccmncfunctions_p.h"

#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QSettings>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslCertificate>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#   include <QDesktopServices>
#else
#   include <QStandardPaths>
#   include <QUrlQuery>
#endif

#include <qplatformdefs.h>

#ifdef QAMPLITUDEANALYTICS_USE_QTSYSTEMINFO
#   include <QDeviceInfo>
#   include <QNetworkInfo>
#elif defined(QAMPLITUDEANALYTICS_USE_QTMOBILITY)
#   include <QSystemInfo>
#   include <QSystemDeviceInfo>
#   include <QSystemNetworkInfo>
#elif defined(Q_OS_BLACKBERRY)
#   include <bbndk.h>
#   include <bb/device/CellularNetworkInfo>
#   include <bb/device/HardwareInfo>
#   include <bb/platform/PlatformInfo>
#endif

QAmplitudeAnalytics::QAmplitudeAnalytics(const QString &apiKey,
                                         const QString &configFilePath,
                                         QObject *parent)
    : QObject(parent)
    , m_apiKey(apiKey)
    , m_privacyEnabled(false)
    , m_sessionId(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())
    , m_lastEventId(0)
    , m_shouldSend(false)
    , m_nam(new QNetworkAccessManager())
    , m_reply(NULL)
{
    if (configFilePath.isEmpty()) {
        QString dataPath;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        dataPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#else
        // Same as AppLocalDataLocation but compatible with Qt < 5.4
        dataPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#endif
        QDir dataDir(dataPath);
        if (!dataDir.exists())
            dataDir.mkpath(QLatin1String("."));

        m_settings.reset(new QSettings(dataDir.filePath(QLatin1String("QtInAppAnalytics.ini")),
                                       QSettings::IniFormat));
    } else {
        m_settings.reset(new QSettings(configFilePath, QSettings::IniFormat));
    }
    m_settings->beginGroup(QLatin1String("AmplitudeAnalytics"));

    QFile f;
    f.setFileName(QLatin1String(":/qtamplitudeanalytics/certificates/addtrust.ca.pem"));
    f.open(QFile::ReadOnly);
    QSslCertificate cert(f.readAll());
    f.close();

    m_sslConfiguration = QSslConfiguration::defaultConfiguration();
    QList<QSslCertificate> cacerts = m_sslConfiguration.caCertificates();
    cacerts.append(cert);
    m_sslConfiguration.setCaCertificates(cacerts);
#if QT_VERSION < QT_VERSION_CHECK(4,8,0)
    // More and more servers are disabling SSLv3 due to vulnerabilities,
    // however Qt < 4.8 has only SSLv3 enabled by default. As there is no
    // QSsl::TlsV1SslV3 enum value in Qt 4.7, we switch to TLSv1 only.
    m_sslConfiguration.setProtocol(QSsl::TlsV1);
#endif

    m_appVersion = QCoreApplication::applicationVersion();

#ifdef Q_OS_LINUX
    m_device.os.platform = QLatin1String("Linux");
#elif defined(Q_OS_WIN)
# ifdef Q_OS_WIN32
    m_device.os.platform = QLatin1String("Win32");
# elif defined(Q_OS_WINCE)
    m_device.os.platform = QLatin1String("WinCE");
# elif defined(Q_OS_WINRT)
    m_device.os.platform = QLatin1String("WinRT");
# endif
#elif defined(Q_OS_SYMBIAN)
    m_device.os.platform = QLatin1String("Symbian");
    m_device.os.name = QLatin1String("S60");
#elif defined(Q_OS_QNX)
    m_device.os.platform = QLatin1String("QNX");
#endif

#ifdef MEEGO_EDITION_HARMATTAN
    m_device.os.name = QLatin1String("Meego");
#endif

#ifdef QAMPLITUDEANALYTICS_USE_QTSYSTEMINFO
    QDeviceInfo di;

    m_device.os.name = di.operatingSystemName();
    m_device.os.version = di.version(QDeviceInfo::Os);

    m_device.id = di.uniqueDeviceID();
    m_device.manufacturer = di.manufacturer();
    m_device.model = di.model();
    if (m_device.model.isEmpty())
        m_device.model = di.productName();

    QNetworkInfo ni;
    m_location.country = findCountryByMcc(ni.homeMobileCountryCode(0));

    QVector<QNetworkInfo::NetworkMode> modes;
    modes << QNetworkInfo::GsmMode
          << QNetworkInfo::CdmaMode
          << QNetworkInfo::WcdmaMode
          << QNetworkInfo::WimaxMode
          << QNetworkInfo::LteMode
          << QNetworkInfo::TdscdmaMode;
    foreach (QNetworkInfo::NetworkMode mode, modes) {
        const int count = ni.networkInterfaceCount(mode);
        for (int interface = 0; interface < count; ++interface) {
            m_device.carrier = ni.networkName(mode, interface);
            if (!m_device.carrier.isEmpty())
                break;
        }
        if (!m_device.carrier.isEmpty())
            break;
    }
    if (m_device.carrier.isEmpty())
        m_device.carrier = findCarrierByMccMnc(ni.homeMobileCountryCode(0), ni.homeMobileNetworkCode(0));
#elif defined(QAMPLITUDEANALYTICS_USE_QTMOBILITY)
    QtMobility::QSystemInfo si;
    m_device.os.version = si.version(QtMobility::QSystemInfo::Os);

    QtMobility::QSystemDeviceInfo sdi;
    m_device.id = sdi.uniqueDeviceID();
    m_device.manufacturer = sdi.manufacturer();
    m_device.model = sdi.model();

    QLocale loc(si.currentLanguage());
    m_language = QLocale::languageToString(loc.language());

    QtMobility::QSystemNetworkInfo sni;
    m_location.country = findCountryByMcc(sni.homeMobileCountryCode());
    if (m_location.country.isEmpty())
        m_location.country = findCountryByIso3166(si.currentCountryCode());

    QVector<QtMobility::QSystemNetworkInfo::NetworkMode> modes;
    modes << QtMobility::QSystemNetworkInfo::GsmMode
          << QtMobility::QSystemNetworkInfo::CdmaMode
          << QtMobility::QSystemNetworkInfo::WcdmaMode
          << QtMobility::QSystemNetworkInfo::WimaxMode
          << QtMobility::QSystemNetworkInfo::LteMode;
    foreach (QtMobility::QSystemNetworkInfo::NetworkMode mode, modes) {
        m_device.carrier = QtMobility::QSystemNetworkInfo::networkName(mode);
        if (!m_device.carrier.isEmpty())
            break;
    }
    if (m_device.carrier.isEmpty())
        m_device.carrier = findCarrierByMccMnc(sni.homeMobileCountryCode(), sni.homeMobileNetworkCode());
#elif defined(Q_OS_BLACKBERRY)
    m_device.os.name = QLatin1String("BlackBerry 10");

    bb::platform::PlatformInfo pi;
    m_device.os.version = pi.osVersion();

//    m_device.brand = QLatin1String("BlackBerry");
    m_device.manufacturer = QLatin1String("BlackBerry");
    bb::device::HardwareInfo hwi;
    m_device.model = hwi.modelName();
    if (m_device.model.isEmpty())
        m_device.model = hwi.deviceName();

    bb::device::CellularNetworkInfo cni;
#if defined(BBNDK_VERSION_AT_LEAST) && BBNDK_VERSION_AT_LEAST(10,3,0)
    if (!cni.displayName().isEmpty())
        m_device.carrier = cni.displayName();
    else
#endif
    if (!cni.name().isEmpty())
        m_device.carrier = cni.name();
    else
        m_device.carrier = findCarrierByMccMnc(cni.mobileCountryCode(), cni.mobileNetworkCode());
    m_location.country = findCountryByMcc(cni.mobileCountryCode());
#endif

    if (m_device.id.isEmpty()) {
        m_device.id = m_settings->value(QLatin1String("InstallationId")).toString();
        if (m_device.id.isEmpty()) {
            m_device.id = QUuid::createUuid().toString();
            // Strip curly braces
            m_device.id.remove(0, 1).chop(1);
            m_settings->setValue(QLatin1String("InstallationId"), m_device.id);
        }
    }

    const QLocale sysloc(QLocale::system());
    if (m_language.isEmpty() && sysloc.language() != QLocale::C)
        m_language = QLocale::languageToString(sysloc.language());
    capitalize(m_language);

    if (m_location.country.isEmpty() && sysloc.country() != QLocale::AnyCountry) {
        const QStringList lang = sysloc.name().split(QLatin1Char('_'));
        if (lang.count() > 1) {
            m_location.country = findCountryByIso3166(lang.at(1));
        }
    }

    int size = m_settings->beginReadArray(QLatin1String("QueuedEvents"));
    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        m_queue.append(m_settings->value(QLatin1String("Event")).toString());
    }
    m_settings->endArray();

    connect(m_nam.data(), SIGNAL(finished(QNetworkReply*)), this, SLOT(onNetworkReply(QNetworkReply*)));
}

QString QAmplitudeAnalytics::apiKey() const
{
    return m_apiKey;
}

void QAmplitudeAnalytics::setApiKey(const QString &apiKey)
{
    if (m_apiKey == apiKey)
        return;

    m_apiKey = apiKey;
    emit apiKeyChanged();
}

QString QAmplitudeAnalytics::appVersion() const
{
    return m_appVersion;
}

void QAmplitudeAnalytics::setAppVersion(const QString &version)
{
    if (m_appVersion == version)
        return;

    m_appVersion = version;
    emit appVersionChanged();
}

QString QAmplitudeAnalytics::userId() const
{
    return m_userId;
}

void QAmplitudeAnalytics::setUserId(const QString &id)
{
    if (m_userId == id)
        return;

    m_userId = id;
    emit userId();
}

QVariantMap QAmplitudeAnalytics::persistentUserProperties() const
{
    return m_userProperties;
}

void QAmplitudeAnalytics::setPersistentUserProperties(const QVariantMap &properties)
{
    if (m_userProperties == properties)
        return;

    m_userProperties = properties;
    emit persistentUserPropertiesChanged();
}

QAmplitudeAnalytics::DeviceInfo QAmplitudeAnalytics::deviceInfo() const
{
    return m_device;
}

void QAmplitudeAnalytics::setDeviceInfo(const DeviceInfo &info)
{
    if (m_device == info)
        return;

    m_device = info;
    emit deviceInfoChanged();
}

QAmplitudeAnalytics::LocationInfo QAmplitudeAnalytics::locationInfo() const
{
    return m_location;
}

void QAmplitudeAnalytics::setLocationInfo(const LocationInfo &info)
{
    if (m_location == info)
        return;

    m_location = info;
    emit locationInfoChanged();
}

QString QAmplitudeAnalytics::language() const
{
    return m_language;
}

void QAmplitudeAnalytics::setLanguage(const QString &language)
{
    if (m_language == language)
        return;

    m_language = language;
    emit languageChanged();
}

bool QAmplitudeAnalytics::isPrivacyEnabled() const
{
    return m_privacyEnabled;
}

void QAmplitudeAnalytics::setPrivacyEnabled(bool enabled)
{
    if (m_privacyEnabled == enabled)
        return;

    m_privacyEnabled = enabled;
    emit privacyEnabledChanged();
}

QAmplitudeAnalytics::~QAmplitudeAnalytics()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }

    saveToSettings();
    m_settings->endGroup();
}

void QAmplitudeAnalytics::trackEvent(const QString &eventType,
                                     const QVariantMap &eventProperties,
                                     bool postpone)
{
    trackEvent(eventType, eventProperties, QVariantMap(), QVariant(), postpone);
}

void QAmplitudeAnalytics::trackEvent(const QString &eventType,
                                     const QVariantMap &eventProperties,
                                     const QVariantMap &userProperties,
                                     const QVariant &revenue,
                                     bool postpone)
{
    QVariantHash event;
    fillCommonProperties(event, userProperties);
    event.insert(QLatin1String("event_type"), eventType);
    event.insert(QLatin1String("time"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    event.insert(QLatin1String("event_properties"), eventProperties);

    if (!m_privacyEnabled) {
        if (m_location.latitude.isValid())
            event.insert(QLatin1String("location_lat"), doubleToString(m_location.latitude, 15));
        if (m_location.longitude.isValid())
            event.insert(QLatin1String("location_lng"), doubleToString(m_location.longitude, 15));
        if (!m_location.ip.isEmpty())
            event.insert(QLatin1String("ip"), m_location.ip);
    }

    if (revenue.isValid()) {
        event.insert(QLatin1String("revenue"), doubleToString(revenue, 2));
    }

    event.insert(QLatin1String("event_id"), ++m_lastEventId);
    event.insert(QLatin1String("session_id"), m_sessionId);
    QString uuid = QUuid::createUuid().toString();
    // Strip curly braces
    uuid.remove(0, 1).chop(1);
    event.insert(QLatin1String("insert_id"), uuid);
    m_queue.append(toJson(event));
    saveToSettings();

    if (postpone) {
        return;
    }

    sendQueuedEvents();
}

void QAmplitudeAnalytics::identifyUser(const QVariantMap &userProperties,
                                       const QVariant paying,
                                       const QString &startVersion)
{
    QVariantHash identification;
    fillCommonProperties(identification, userProperties);

    if (paying.isValid())
        identification.insert(QLatin1String("paying"), paying);
    if (!startVersion.isEmpty())
        identification.insert(QLatin1String("start_version"), startVersion);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QUrl query;
#else
    QUrlQuery query;
#endif
    query.addQueryItem(QLatin1String("api_key"), m_apiKey);
    query.addQueryItem(QLatin1String("identification"), toJson(identification));

    QNetworkRequest request(QUrl(QLatin1String("https://api.amplitude.com/identify")));
    request.setSslConfiguration(m_sslConfiguration);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QLatin1String("application/x-www-form-urlencoded;charset=UTF-8"));
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    const QByteArray data(query.encodedQuery());
#else
    const QByteArray data(query.toString(QUrl::FullyEncoded).toUtf8());
#endif
    m_nam->post(request, data);
}

void QAmplitudeAnalytics::sendQueuedEvents()
{
    if (m_queue.isEmpty())
        return;

    if (m_reply || !m_pending.isEmpty()) {
        // We already have request pending - mark that we
        // should send again and wait until it finishes
        m_shouldSend = true;
        return;
    }

    m_shouldSend = false;
    m_pending = m_queue;
    m_queue.clear();

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QUrl query;
#else
    QUrlQuery query;
#endif
    query.addQueryItem(QLatin1String("api_key"), m_apiKey);
    query.addQueryItem(QLatin1String("event"), m_pending.join(QLatin1String(","))
                                                        .prepend(QLatin1Char('['))
                                                        .append(QLatin1Char(']')));

    QNetworkRequest request(QUrl(QLatin1String("https://api.amplitude.com/httpapi")));
    request.setSslConfiguration(m_sslConfiguration);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QLatin1String("application/x-www-form-urlencoded;charset=UTF-8"));
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    const QByteArray data(query.encodedQuery());
#else
    const QByteArray data(query.toString(QUrl::FullyEncoded).toUtf8());
#endif
    m_reply = m_nam->post(request, data);
}

void QAmplitudeAnalytics::clearQueuedEvents()
{
    m_shouldSend = false;
    m_queue.clear();
}

void QAmplitudeAnalytics::onNetworkReply(QNetworkReply *reply)
{
    if (reply != m_reply) {
        // Reply not for the current request - ignore it (we
        // shouldn't have two requests running in parallel)
        reply->deleteLater();
        return;
    }
    m_reply = NULL;

    if (reply->error() != QNetworkReply::NoError) {
        // Sending failed - return pending events back to the queue
        m_pending.append(m_queue);
        m_queue = m_pending;
        qWarning() << reply->errorString();
    }
    m_pending.clear();
    reply->deleteLater();

    saveToSettings();

    if (m_shouldSend) {
        sendQueuedEvents();
    }
}

void QAmplitudeAnalytics::fillCommonProperties(QVariantHash &hashMap,
                                               const QVariantMap &userProperties) const
{
    if (!m_userId.isEmpty())
        hashMap.insert(QLatin1String("user_id"), m_userId);
    if (!m_device.id.isEmpty())
        hashMap.insert(QLatin1String("device_id"), m_device.id);

    if (!userProperties.isEmpty())
        hashMap.insert(QLatin1String("user_properties"), userProperties);
    else if (!m_userProperties.isEmpty())
        hashMap.insert(QLatin1String("user_properties"), m_userProperties);

    if (!m_appVersion.isEmpty())
        hashMap.insert(QLatin1String("app_version"), m_appVersion);
    if (!m_device.os.platform.isEmpty())
        hashMap.insert(QLatin1String("platform"), m_device.os.platform);
    if (!m_device.os.name.isEmpty())
        hashMap.insert(QLatin1String("os_name"), m_device.os.name);
    if (!m_device.os.version.isEmpty())
        hashMap.insert(QLatin1String("os_version"), m_device.os.version);
    if (!m_device.brand.isEmpty())
        hashMap.insert(QLatin1String("device_brand"), m_device.brand);
    if (!m_device.manufacturer.isEmpty())
        hashMap.insert(QLatin1String("device_manufacturer"), m_device.manufacturer);
    if (!m_device.model.isEmpty())
        hashMap.insert(QLatin1String("device_model"), m_device.model);

    if (!m_privacyEnabled) {
        if (!m_device.carrier.isEmpty())
            hashMap.insert(QLatin1String("carrier"), m_device.carrier);
        if (!m_location.country.isEmpty())
            hashMap.insert(QLatin1String("country"), m_location.country);
        if (!m_location.region.isEmpty())
            hashMap.insert(QLatin1String("region"), m_location.region);
        if (!m_location.city.isEmpty())
            hashMap.insert(QLatin1String("city"), m_location.city);
        if (!m_location.dma.isEmpty())
            hashMap.insert(QLatin1String("dma"), m_location.dma);
        if (!m_language.isEmpty())
            hashMap.insert(QLatin1String("language"), m_language);
    }
}

void QAmplitudeAnalytics::saveToSettings()
{
    if (!m_queue.isEmpty()) {
        m_settings->beginWriteArray(QLatin1String("QueuedEvents"));
        for (int i = 0; i < m_queue.count(); ++i) {
            m_settings->setArrayIndex(i);
            m_settings->setValue(QLatin1String("Event"), m_queue.at(i));
        }
        m_settings->endArray();
    } else {
        m_settings->remove(QLatin1String("QueuedEvents"));
    }
}
