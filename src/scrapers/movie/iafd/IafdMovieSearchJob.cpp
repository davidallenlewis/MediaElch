#include "scrapers/movie/iafd/IafdMovieSearchJob.h"

#include "log/Log.h"
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

    // Single Startpage query: "iafd.com TITLE"
    // The domain path acts as an implicit keyword filter — no site: operator needed.
    m_api.searchForMovie(query, [this, query](QString data, ScraperError error) {
        if (!error.hasError()) {
            m_results = parseSearchStartpage(data);
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

        qCDebug(generic) << "[IAFD] Search complete:" << m_results.size() << "results";
        emitFinished();
    });
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
