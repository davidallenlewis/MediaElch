#include "scrapers/movie/iafd/IafdMovieSearchJob.h"

#include "log/Log.h"
#include "scrapers/ScraperError.h"
#include "scrapers/movie/MovieIdentifier.h"

#include <QObject>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrl>

#include <algorithm>

namespace mediaelch {
namespace scraper {

IafdMovieSearchJob::IafdMovieSearchJob(IafdMovieApi& api,
    MovieSearchJob::Config config,
    QObject* parent) :
    MovieSearchJob(std::move(config), parent), m_api{api}
{
}

void IafdMovieSearchJob::doStart()
{
    // If the user pastes an IAFD movie URL directly into the search box, skip
    // search entirely and return it as an immediate single result.
    const QString query = config().query.trimmed();
    if (query.contains(QStringLiteral("iafd.com/title.rme"), Qt::CaseInsensitive)) {
        MovieSearchJob::Result result;
        result.identifier = MovieIdentifier(query);
        static const QRegularExpression titleSlugRx(R"re(/title=([^/]+)/)re");
        const auto m = titleSlugRx.match(query);
        if (m.hasMatch()) {
            result.title = QString(m.captured(1)).replace(QLatin1Char('-'), QLatin1Char(' '));
        } else {
            result.title = QStringLiteral("IAFD Movie (URL)");
        }
        m_results << result;
        emitFinished();
        return;
    }

    runSearch(query);
}

void IafdMovieSearchJob::runSearch(const QString& query)
{
    // Single Startpage query: "iafd.com TITLE"
    // The domain path acts as an implicit keyword filter — no site: operator needed.
    m_api.searchForMovie(query, [this, query](QString data, ScraperError error) {
        if (!error.hasError()) {
            m_results = parseSearchStartpage(data);
        }

        // Startpage selectively server-renders results. When the raw response
        // contains no title.rme links it's a JS-only shell — fall back to DDG HTML.
        if (m_results.isEmpty() && !m_retried) {
            int titleRmeCount = 0;
            int pos = 0;
            while ((pos = data.indexOf(QLatin1String("title.rme"), pos, Qt::CaseInsensitive)) != -1) {
                ++titleRmeCount;
                ++pos;
            }
            qCInfo(generic) << "[IAFD] Startpage returned 0 results for query:" << query
                             << "| title.rme occurrences:" << titleRmeCount
                             << "| response length:" << data.size();
            if (titleRmeCount == 0) {
                qCInfo(generic) << "[IAFD] Startpage returned JS-only shell — falling back to DDG HTML";
                m_retried = true;
                runDDGFallback(query);
                return;
            }
        } else if (!m_results.isEmpty()) {
            qCInfo(generic) << "[IAFD] Startpage parsed" << m_results.size()
                             << "results for query:" << query;
            for (const auto& r : asConst(m_results)) {
                qCInfo(generic) << "[IAFD]  -" << r.title << "|" << r.identifier.str();
            }
        }

        // Stable-sort by relevance: exact title match first, then prefix, then
        // contains, then everything else in original order.
        const QString queryLower = query.toLower();
        auto score = [&queryLower](const MovieSearchJob::Result& r) {
            const QString t = r.title.toLower();
            if (t == queryLower) return 3;
            if (t.startsWith(queryLower)) return 2;
            if (t.contains(queryLower)) return 1;
            return 0;
        };
        std::stable_sort(m_results.begin(), m_results.end(),
            [&score](const MovieSearchJob::Result& a, const MovieSearchJob::Result& b) {
                return score(a) > score(b);
            });

        // Only retry (via DDG) when we got zero results — if any results came
        // back from Startpage, return them even if none score against the query.
        const bool anyMatch = !m_results.isEmpty();
        if (!anyMatch && !m_retried) {
            m_retried = true;
            qCInfo(generic) << "[IAFD] Startpage returned results but none matched — retrying via DDG for:" << query;
            m_results.clear();
            runDDGFallback(query);
            return;
        }

        qCDebug(generic) << "[IAFD] Search complete:" << m_results.size() << "results";
        emitFinished();
    });
}

void IafdMovieSearchJob::runDDGFallback(const QString& query)
{
    qCInfo(generic) << "[IAFD] Trying DDG HTML fallback for:" << query;
    m_api.searchForMovieDDG(query, [this, query](QString data, ScraperError error) {
        if (!error.hasError()) {
            m_results = parseSearchDDG(data);
        }
        qCInfo(generic) << "[IAFD] DDG returned" << m_results.size() << "results for query:" << query;
        int ddgTitleRme = 0;
        int pos = 0;
        while ((pos = data.indexOf(QLatin1String("title.rme"), pos, Qt::CaseInsensitive)) != -1) {
            ++ddgTitleRme;
            ++pos;
        }
        qCInfo(generic) << "[IAFD] DDG response 'title.rme' occurrences:" << ddgTitleRme
                         << "| response length:" << data.size();
        if (ddgTitleRme > 0 && m_results.isEmpty()) {
            // Links are present but regex didn't match — log context around first hit
            const int hitPos = data.indexOf(QLatin1String("title.rme"), 0, Qt::CaseInsensitive);
            const int start = qMax(0, hitPos - 200);
            const int len = qMin(500, data.size() - start);
            qCInfo(generic) << "[IAFD] DDG context around first title.rme:" << data.mid(start, len);
        }
        if (m_results.isEmpty()) {
            const QString iafdUrl =
                QStringLiteral("https://www.iafd.com/results.asp?searchtype=comprehensive&searchstring=")
                + QString::fromUtf8(QUrl::toPercentEncoding(query))
                      .replace(QLatin1Char(' '), QLatin1Char('+'));
            ScraperError hint;
            hint.error = ScraperError::Type::ApiError;
            hint.message =
                tr("No results \u2014 Try modifying your search or <a href=\"%1\">Search IAFD</a> and paste the URL")
                    .arg(iafdUrl);
            setScraperError(hint);
        }
        for (const auto& r : asConst(m_results)) {
            qCInfo(generic) << "[IAFD]  -" << r.title << "|" << r.identifier.str();
        }
        const QString queryLower = query.toLower();
        auto score = [&queryLower](const MovieSearchJob::Result& r) {
            const QString t = r.title.toLower();
            if (t == queryLower) return 3;
            if (t.startsWith(queryLower)) return 2;
            if (t.contains(queryLower)) return 1;
            return 0;
        };
        std::stable_sort(m_results.begin(), m_results.end(),
            [&score](const MovieSearchJob::Result& a, const MovieSearchJob::Result& b) {
                return score(a) > score(b);
            });
        qCDebug(generic) << "[IAFD] Search complete:" << m_results.size() << "results";
        emitFinished();
    });
}

QList<MovieSearchJob::Result> IafdMovieSearchJob::parseSearchDDG(const QString& html)
{
    QList<MovieSearchJob::Result> results;
    // DDG Lite wraps result hrefs in redirect URLs of the form:
    //   //duckduckgo.com/l/?uddg=https%3A%2F%2Fwww.iafd.com%2Ftitle.rme%2F...
    // Match those redirect links and URL-decode the uddg parameter.
    // Also match any direct iafd.com/title.rme links as a fallback.
    static const QRegularExpression redirectRx(
        R"re(href="[^"]*[?&]uddg=([^&"]+)[^"]*"[^>]*>(.*?)</a>)re",
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression directRx(
        R"re(href="(https?://(?:www\.)?iafd\.com/title\.rme/[^"]+)"[^>]*>(.*?)</a>)re",
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tagRx(R"re(<[^>]+>)re");

    QSet<QString> seen;

    auto addResult = [&](const QString& url, const QString& rawTitle) {
        if (!url.contains(QStringLiteral("iafd.com/title.rme"), Qt::CaseInsensitive)) {
            return;
        }
        if (seen.contains(url)) {
            return;
        }
        seen.insert(url);

        QString title = rawTitle;
        title.remove(tagRx);
        title = QTextDocumentFragment::fromHtml(title).toPlainText().trimmed();
        const int suffixIdx = title.indexOf(QStringLiteral(" - iafd.com"), 0, Qt::CaseInsensitive);
        if (suffixIdx > 0) {
            title = title.left(suffixIdx).trimmed();
        }
        if (title.isEmpty()) {
            return;
        }
        QStringList words = title.split(QLatin1Char(' '));
        for (QString& word : words) {
            if (!word.isEmpty()) {
                word[0] = word[0].toUpper();
            }
        }
        title = words.join(QLatin1Char(' '));

        MovieSearchJob::Result result;
        result.identifier = MovieIdentifier(url);
        result.title = title;
        results << result;
    };

    // Redirect links
    QRegularExpressionMatchIterator it = redirectRx.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString url = QUrl::fromPercentEncoding(m.captured(1).toUtf8());
        addResult(url, m.captured(2));
    }

    // Direct links (fallback, in case DDG Lite ever uses them)
    it = directRx.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        addResult(m.captured(1), m.captured(2));
    }

    return results;
}

QList<MovieSearchJob::Result> IafdMovieSearchJob::parseSearchStartpage(const QString& html)
{
    QList<MovieSearchJob::Result> results;
    // Startpage result structure: each result contains an IAFD href followed
    // by an h2/h3 heading in the form "title - iafd.com - internet adult film database".
    static const QRegularExpression rx(
        R"re(href="(https://www\.iafd\.com/title\.rme/[^"]+)".*?<h[23][^>]*>(.*?)</h[23]>)re",
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tagRx(R"re(<[^>]+>)re");

    QSet<QString> seen;
    QRegularExpressionMatchIterator it = rx.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString url = m.captured(1);
        if (seen.contains(url)) {
            continue;
        }
        seen.insert(url);

        QString rawTitle = m.captured(2);
        rawTitle.remove(tagRx);
        QString title =
            QTextDocumentFragment::fromHtml(rawTitle).toPlainText().trimmed();
        const int suffixIdx =
            title.indexOf(QStringLiteral(" - iafd.com"), 0, Qt::CaseInsensitive);
        if (suffixIdx > 0) {
            title = title.left(suffixIdx).trimmed();
        }
        if (title.isEmpty()) {
            continue;
        }
        QStringList words = title.split(QLatin1Char(' '));
        for (QString& word : words) {
            if (!word.isEmpty()) {
                word[0] = word[0].toUpper();
            }
        }
        title = words.join(QLatin1Char(' '));

        MovieSearchJob::Result result;
        result.identifier = MovieIdentifier(url);
        result.title = title;
        results << result;
    }
    return results;
}

} // namespace scraper
} // namespace mediaelch
