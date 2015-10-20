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

#include "qamplitudeanalytics.h"

#include <QFile>
#include <QDateTime>
#include <QUuid>
#include <QSettings>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslCertificate>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#   include <QUrlQuery>
#endif

#ifdef QAMPLITUDEANALYTICS_USE_QTSYSTEMINFO
#   include <QDeviceInfo>
#   include <QNetworkInfo>
#elif defined(QAMPLITUDEANALYTICS_USE_QTMOBILITY)
#   include <QSystemInfo>
#   include <QSystemDeviceInfo>
#   include <QSystemNetworkInfo>
#endif

namespace {
    inline void capitalize(QString &str);
    inline QString quoteAndEscape(const QString &string);
    inline QString toJsonString(const QVariant &value);
    inline QString toJson(const QVariantMap &map);
    inline QString findCountryByIso3166(const QString &iso);
    inline QString findCountryByMcc(const QString &mcc);
    inline QString findCarrierByMccMnc(const QString &mcc, const QString &mnc);
}

QAmplitudeAnalytics::QAmplitudeAnalytics(const QString &apiKey, QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_apiKey(apiKey)
    , m_sessionId(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())
    , m_lastEventId(0)
    , m_settings(settings)
    , m_nam(new QNetworkAccessManager(this))
    , m_reply(NULL)
{
    QFile f;
    f.setFileName(":/qtamplitudeanalytics/certificates/addtrust.ca.pem");
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

#ifdef Q_OS_LINUX
    m_os.platform = QLatin1String("Linux");
#elif defined(Q_OS_WIN)
# ifdef Q_OS_WIN32
    m_os.platform = QLatin1String("Win32");
# elif defined(Q_OS_WINCE)
    m_os.platform = QLatin1String("WinCE");
# elif defined(Q_OS_WINRT)
    m_os.platform = QLatin1String("WinRT");
# endif
#elif defined(Q_OS_SYMBIAN)
    m_os.platform = QLatin1String("Symbian");
    m_os.name = QLatin1String("S60");
#elif defined(Q_OS_QNX)
    m_os.platform = QLatin1String("QNX");
#endif

#ifdef MEEGO_EDITION_HARMATTAN
    m_os.name = QLatin1String("Meego");
#endif

#ifdef QAMPLITUDEANALYTICS_USE_QTSYSTEMINFO
    QDeviceInfo di;

    m_os.name = di.operatingSystemName();
    m_os.version = di.version(QDeviceInfo::Os);

    m_device.id = di.uniqueDeviceID();
    m_device.manufacturer = di.manufacturer();
    m_device.model = di.model();
    if (m_device.model.isEmpty())
        m_device.model = di.productName();

    QNetworkInfo ni;
    m_country = findCountryByMcc(ni.homeMobileCountryCode(0));

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
            m_carrier = ni.networkName(mode, interface);
            if (!m_carrier.isEmpty())
                break;
        }
        if (!m_carrier.isEmpty())
            break;
    }
    if (m_carrier.isEmpty())
        m_carrier = findCarrierByMccMnc(ni.homeMobileCountryCode(0), ni.homeMobileNetworkCode(0));
#elif defined(QAMPLITUDEANALYTICS_USE_QTMOBILITY)
    QtMobility::QSystemInfo si;
    m_os.version = si.version(QtMobility::QSystemInfo::Os);

    QtMobility::QSystemDeviceInfo sdi;
    m_device.id = sdi.uniqueDeviceID();
    m_device.manufacturer = sdi.manufacturer();
    m_device.model = sdi.model();

    QLocale loc(si.currentLanguage());
    m_language = QLocale::languageToString(loc.language());

    QtMobility::QSystemNetworkInfo sni;
    m_country = findCountryByMcc(sni.homeMobileCountryCode());
    if (m_country.isEmpty())
        m_country = findCountryByIso3166(si.currentCountryCode());

    QVector<QtMobility::QSystemNetworkInfo::NetworkMode> modes;
    modes << QtMobility::QSystemNetworkInfo::GsmMode
          << QtMobility::QSystemNetworkInfo::CdmaMode
          << QtMobility::QSystemNetworkInfo::WcdmaMode
          << QtMobility::QSystemNetworkInfo::WimaxMode
          << QtMobility::QSystemNetworkInfo::LteMode;
    foreach (QtMobility::QSystemNetworkInfo::NetworkMode mode, modes) {
        m_carrier = QtMobility::QSystemNetworkInfo::networkName(mode);
        if (!m_carrier.isEmpty())
            break;
    }
    if (m_carrier.isEmpty())
        m_carrier = findCarrierByMccMnc(sni.homeMobileCountryCode(), sni.homeMobileNetworkCode());
#endif

    if (m_device.id.isEmpty()) {
        m_device.id = m_settings->value("Analytics/InstallationId").toString();
        if (m_device.id.isEmpty()) {
            m_device.id = QUuid::createUuid().toString();
            // Strip curly braces
            m_device.id.remove(0, 1).chop(1);
            m_settings->setValue("Analytics/InstallationId", m_device.id);
        }
    }

    const QLocale sysloc(QLocale::system());
    if (m_language.isEmpty() && sysloc.language() != QLocale::C)
        m_language = QLocale::languageToString(sysloc.language());
    capitalize(m_language);

    if (m_country.isEmpty() && sysloc.country() != QLocale::AnyCountry) {
        const QStringList lang = sysloc.name().split("_");
        if (lang.count() > 1) {
            m_country = findCountryByIso3166(lang.at(1));
        }
    }

    int size = m_settings->beginReadArray("Analytics/Amplitude");
    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        m_queue.append(m_settings->value("event").toString());
    }
    m_settings->endArray();

    connect(m_nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(onNetworkReply(QNetworkReply*)));
}

QAmplitudeAnalytics::~QAmplitudeAnalytics()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }

    saveToSettings();
}

void QAmplitudeAnalytics::trackEvent(const QString &eventType,
                                    const QVariantMap &eventProperties,
//                                    const QVariantMap &userProperties,
                                    bool delay)
{
    QVariantMap event;
    event.insert("device_id", m_device.id);
    event.insert("event_type", eventType);
    event.insert("time", QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    event.insert("event_properties", eventProperties);
//    event.insert("user_properties", userProperties);

    event.insert("app_version", QCoreApplication::applicationVersion());
    if (!m_os.platform.isEmpty())
        event.insert("platform", m_os.platform);
    if (!m_os.name.isEmpty())
        event.insert("os_name", m_os.name);
    if (!m_os.version.isEmpty())
        event.insert("os_version", m_os.version);
    if (!m_device.brand.isEmpty())
        event.insert("device_brand", m_device.brand);
    if (!m_device.manufacturer.isEmpty())
        event.insert("device_manufacturer", m_device.manufacturer);
    if (!m_device.model.isEmpty())
        event.insert("device_model", m_device.model);
    if (!m_carrier.isEmpty())
        event.insert("carrier", m_carrier);
    if (!m_country.isEmpty())
        event.insert("country", m_country);
    event.insert("language", m_language);
//    event.insert("revenue", "");

    event.insert("event_id", ++m_lastEventId);
    event.insert("session_id", m_sessionId);
    QString uuid = QUuid::createUuid().toString();
    // Strip curly braces
    uuid.remove(0, 1).chop(1);
    event.insert("insert_id", uuid);
    m_queue.append(toJson(event));
    saveToSettings();

    if (m_reply || delay) {
        return;
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QUrl query;
#else
    QUrlQuery query;
#endif
    query.addQueryItem("api_key", m_apiKey);
    query.addQueryItem("event", m_queue.join(",").prepend("[").append("]"));
    m_pending = m_queue;
    m_queue.clear();

    QNetworkRequest request(QUrl("https://api.amplitude.com/httpapi"));
    request.setSslConfiguration(m_sslConfiguration);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded;charset=UTF-8");
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    const QByteArray data(query.encodedQuery());
#else
    const QByteArray data(query.toString(QUrl::FullyEncoded).toUtf8());
#endif
    m_reply = m_nam->post(request, data);
}

void QAmplitudeAnalytics::onNetworkReply(QNetworkReply *reply)
{
    if (reply != m_reply) {
        // Reply not for the latest request. Ignore it.
        reply->deleteLater();
        return;
    }
    m_reply = NULL;

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        // Operation was canceled by us, ignore this error.
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_pending.append(m_queue);
        m_queue = m_pending;
        qWarning() << reply->errorString();
        return;
    }

    saveToSettings();
}

void QAmplitudeAnalytics::saveToSettings()
{
    if (!m_queue.isEmpty()) {
        m_settings->beginWriteArray("Analytics/Amplitude");
        for (int i = 0; i < m_queue.count(); ++i) {
            m_settings->setArrayIndex(i);
            m_settings->setValue("event", m_queue.at(i));
        }
        m_settings->endArray();
    } else {
        m_settings->remove("Analytics/Amplitude");
    }
}

namespace {
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
        case QVariant::List:
        {
            QStringList values;
            foreach (const QVariant &item, value.toList()) {
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

    inline QString toJson(const QVariantMap &map)
    {
        QStringList json;
        for (QVariantMap::const_iterator i = map.constBegin(); i != map.constEnd(); ++i)
            json.append(QString("%1:%2").arg(quoteAndEscape(i.key()), toJsonString(i.value())));
        return json.join(",").prepend("{").append("}");
    }

    inline QString findCountryByIso3166(const QString &iso)
    {
        if (iso.isEmpty())
            return QString();

        QFile f;
        f.setFileName(":/qtamplitudeanalytics/csv/iso3166-country-codes.csv");
        if (!f.open(QFile::Text | QFile::ReadOnly))
            return QString();

        const QString cc = iso.toUpper();
        while (!f.atEnd()) {
            QStringList row = QString::fromUtf8(f.readLine()).split(',');
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
        f.setFileName(":/qtamplitudeanalytics/csv/mcc-mnc-codes.csv");
        if (!f.open(QFile::Text | QFile::ReadOnly))
            return QString();

        while (!f.atEnd()) {
            QStringList row = QString::fromLatin1(f.readLine()).split(',');
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
        f.setFileName("://mcc-mnc-codes.csv");
        if (!f.open(QFile::Text | QFile::ReadOnly))
            return QString();
        while (!f.atEnd()) {
            QStringList row = QString::fromLatin1(f.readLine()).split(',');
            if (row.at(0) == mcc && row.at(2) == mnc) {
                return row.at(7).trimmed();
            }
        }

        return QString();
    }
}