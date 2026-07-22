#pragma once

#include "network/NetworkManager.h"
#include "scrapers/ScraperError.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <functional>

namespace mediaelch {
namespace scraper {

/// \brief Network/API layer for IAFD (Internet Adult Film Database).
/// \details IAFD is behind Cloudflare and has no public API.
///          Movie search is performed via Startpage (Google proxy) and
///          movie pages are fetched directly with a browser-like user agent.
class IafdMovieApi : public QObject
{
    Q_OBJECT

public:
    explicit IafdMovieApi(QObject* parent = nullptr);
    ~IafdMovieApi() override = default;

public:
    using ApiCallback = std::function<void(QString, ScraperError)>;

    /// \param referer Optional Referer header value. Pass empty string for none.
    void sendGetRequest(const QUrl& url, const QString& referer, ApiCallback callback);

    /// \brief Search IAFD via Startpage using keyword query.
    void searchForMovie(const QString& query, ApiCallback callback);

    /// \brief Load the IAFD movie page at \p url (full URL used as identifier).
    void loadMovie(const QString& url, ApiCallback callback);

    static QUrl makeStartpageSearchUrl(const QString& searchStr);
    static QUrl makeMovieUrl(const QString& id);

private:
    mediaelch::network::NetworkManager m_network;
};

} // namespace scraper
} // namespace mediaelch
