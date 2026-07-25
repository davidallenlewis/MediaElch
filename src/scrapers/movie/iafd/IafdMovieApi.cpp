#include "scrapers/movie/iafd/IafdMovieApi.h"

#include "log/Log.h"
#include "network/NetworkRequest.h"
#include "utils/Meta.h"

#include <QNetworkReply>
#include <QObject>
#include <QTimer>

namespace mediaelch {
namespace scraper {

namespace {

/// \brief Returns true if \p data looks like a Cloudflare browser-challenge page.
/// \details IAFD is behind Cloudflare. When the JS challenge fires the server
///          may return any 2xx/4xx/5xx with a challenge body that cannot be scraped.
///          Body markers are checked first (work across all status codes); header+status
///          and a castbox heuristic are used as fallbacks.
bool isCloudflareChallengeResponse(const QString& data, const QNetworkReply& reply)
{
    // Body-level markers present in every Cloudflare challenge variant.
    if (data.contains(QStringLiteral("_cf_chl_opt"), Qt::CaseSensitive)
        || data.contains(QStringLiteral("cf_chl_prog"), Qt::CaseSensitive)
        || data.contains(QStringLiteral("cf-browser-verification"), Qt::CaseSensitive)
        || data.contains(QStringLiteral("jschl_vc"), Qt::CaseSensitive)
        || (data.contains(QStringLiteral("Just a moment"), Qt::CaseSensitive)
            && data.contains(QStringLiteral("cloudflare"), Qt::CaseInsensitive))) {
        return true;
    }

    const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool hasCfRay = !reply.rawHeader("cf-ray").isEmpty();

    // 403/503 with cf-ray header is always a Cloudflare block.
    if (hasCfRay && (status == 403 || status == 503)) {
        return true;
    }

    // IAFD movie pages always contain a "castbox" div.
    // IAFD search result pages always contain "/title.rme/" links.
    // A 200 from iafd.com with cf-ray but neither marker means Cloudflare
    // served an interstitial — treat it as a challenge.
    // Also accept search result pages (results.asp) which contain search-result anchors.
    const QString host = reply.url().host();
    const QString urlStr = reply.url().toString();
    if (hasCfRay && status == 200
        && host.contains(QStringLiteral("iafd.com"), Qt::CaseInsensitive)
        && !data.contains(QStringLiteral("castbox"), Qt::CaseSensitive)
        && !data.contains(QStringLiteral("/title.rme/"), Qt::CaseSensitive)
        && !urlStr.contains(QStringLiteral("results.asp"), Qt::CaseSensitive)) {
        return true;
    }

    return false;
}

} // namespace

IafdMovieApi::IafdMovieApi(QObject* parent) : QObject(parent)
{
}

void IafdMovieApi::sendGetRequest(const QUrl& url, const QString& referer, IafdMovieApi::ApiCallback callback)
{
    QNetworkRequest request = mediaelch::network::requestWithDefaults(url);
    // Use a Firefox user-agent (Cloudflare passive fingerprint checks UA).
    mediaelch::network::useFirefoxUserAgent(request);
    // Send a complete browser-like header set.  Cloudflare's passive checks
    // inspect Accept, Accept-Language and Sec-Fetch-* in addition to
    // User-Agent.  Note: we intentionally omit Accept-Encoding so that Qt's
    // network layer receives uncompressed content without us having to inflate
    // it manually.
    request.setRawHeader("Accept",
        "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.5");
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Upgrade-Insecure-Requests", "1");
    request.setRawHeader("Sec-Fetch-Dest", "document");
    request.setRawHeader("Sec-Fetch-Mode", "navigate");
    if (referer.isEmpty()) {
        request.setRawHeader("Sec-Fetch-Site", "none");
    } else {
        request.setRawHeader("Referer", referer.toUtf8());
        request.setRawHeader("Sec-Fetch-Site", "same-origin");
    }
    request.setRawHeader("Sec-Fetch-User", "?1");

    // Google shows a cookie-consent interstitial unless SOCS=CAI is present.
    if (url.host().contains(QStringLiteral("google.com"), Qt::CaseInsensitive)) {
        request.setRawHeader("Cookie", "SOCS=CAI");
    }

    if (m_network.cache().hasValidElement(request)) {
        // Do not immediately run the callback because classes higher up may
        // set up a Qt connection while the network request is running.
        QTimer::singleShot(0, this, [cb = std::move(callback), element = m_network.cache().getElement(request)]() {
            cb(element, {});
        });
        return;
    }

    QNetworkReply* reply = m_network.getWithWatcher(request);
    connect(reply, &QNetworkReply::finished, this, [reply, cb = std::move(callback), request, this]() {
        auto dls = makeDeleteLaterScope(reply);

        // Always read the body — Cloudflare returns its challenge HTML even on
        // 403/503 responses, so we must inspect the body before deciding what
        // error (if any) to surface.
        const QString data = QString::fromUtf8(reply->readAll());

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(generic) << "[IafdMovieApi] Network Error:" << reply->errorString() << "for URL"
                               << reply->url();
        }

        // Detect Cloudflare challenge before doing anything else.
        // Qt does not have a JS runtime, so we cannot solve the challenge
        // programmatically.  Surface a clear error so the user knows why.
        //
        // Strategy: body markers catch 200-with-challenge responses;
        // any 4xx/5xx from iafd.com directly is also treated as a Cloudflare block
        // (search goes through DDG, so this only fires for direct movie-page loads).
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool isIafdHost =
            reply->url().host().contains(QStringLiteral("iafd.com"), Qt::CaseInsensitive);
        if (isCloudflareChallengeResponse(data, *reply)
            || (isIafdHost && httpStatus >= 400 && httpStatus < 600)) {
            qCWarning(generic) << "[IafdMovieApi] Cloudflare/server block received for URL"
                               << reply->url() << "(HTTP" << httpStatus << ")";
            ScraperError cfError;
            cfError.error = ScraperError::Type::ApiError;
            cfError.message = QObject::tr("Request blocked by Cloudflare.");
            cfError.technical =
                QStringLiteral("HTTP %1 from iafd.com (cf-ray: %2)")
                    .arg(httpStatus)
                    .arg(QString::fromLatin1(reply->rawHeader("cf-ray")));
            cb(data, cfError);
            return;
        }

        if (!data.isEmpty()) {
            m_network.cache().addElement(request, data);
        }

        ScraperError error = makeScraperError(data, *reply, {});
        cb(data, error);
    });
}

void IafdMovieApi::searchForMovie(const QString& query, IafdMovieApi::ApiCallback callback)
{
    sendGetRequest(makeStartpageSearchUrl(query), {}, std::move(callback));
}

void IafdMovieApi::loadMovie(const QString& url, IafdMovieApi::ApiCallback callback)
{
    sendGetRequest(makeMovieUrl(url), {}, std::move(callback));
}

QUrl IafdMovieApi::makeStartpageSearchUrl(const QString& searchStr)
{
    // Startpage (Google proxy) — keyword search using domain as implicit filter.
    // Using bare "iafd.com TITLE" (without /title.rme or site:) gives better coverage.
    QUrl url(QStringLiteral("https://www.startpage.com/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"),
        QStringLiteral("www.iafd.com ") + searchStr);
    q.addQueryItem(QStringLiteral("cat"), QStringLiteral("web"));
    q.addQueryItem(QStringLiteral("safe"), QStringLiteral("off"));
    url.setQuery(q);
    return url;
}

QUrl IafdMovieApi::makeMovieUrl(const QString& id)
{
    // The identifier is the full IAFD movie page URL.
    return QUrl(id);
}

} // namespace scraper
} // namespace mediaelch
