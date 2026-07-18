#include "scrapers/movie/iafd/IafdMovieSearchJob.h"

#include "scrapers/movie/MovieIdentifier.h"

#include <QObject>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrl>

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
    // DuckDuckGo entirely and return it as an immediate single result.
    // This is the most reliable path and avoids all bot-detection issues.
    const QString query = config().query.trimmed();
    if (query.contains(QStringLiteral("iafd.com/title.rme"), Qt::CaseInsensitive)) {
        MovieSearchJob::Result result;
        result.identifier = MovieIdentifier(query);
        // Try slug format first: /title=movie-name/ → "movie name"
        static const QRegularExpression titleSlugRx(R"re(/title=([^/]+)/)re");
        const auto m = titleSlugRx.match(query);
        if (m.hasMatch()) {
            result.title = QString(m.captured(1)).replace(QLatin1Char('-'), QLatin1Char(' '));
        } else {
            // UUID format /id=UUID or unknown — use a placeholder; title is
            // shown properly once the page is scraped.
            result.title = QStringLiteral("IAFD Movie (URL)");
        }
        m_results << result;
        emitFinished();
        return;
    }

    m_api.searchForMovie(query, [this](QString data, ScraperError error) {
        if (error.hasError()) {
            setScraperError(error);
        } else {
            parseSearch(data);
        }
        emitFinished();
    });
}

void IafdMovieSearchJob::parseSearch(const QString& html)
{
    // Startpage result structure: each result contains an IAFD href followed
    // by an h2/h3 heading in the form "title - iafd.com - internet adult film database".
    // We match URL then the nearest heading after it.
    static const QRegularExpression rx(
        R"re(href="(https://www\.iafd\.com/title\.rme/id=[0-9a-f-]+)".*?<h[23][^>]*>(.*?)</h[23]>)re",
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

        // Strip HTML tags then unescape HTML entities.
        QString rawTitle = m.captured(2);
        rawTitle.remove(tagRx);
        QString title =
            QTextDocumentFragment::fromHtml(rawTitle).toPlainText().trimmed();
        // Remove the " - iafd.com - internet adult film database" suffix.
        const int suffixIdx =
            title.indexOf(QStringLiteral(" - iafd.com"), 0, Qt::CaseInsensitive);
        if (suffixIdx > 0) {
            title = title.left(suffixIdx).trimmed();
        }
        if (title.isEmpty()) {
            continue;
        }
        // Startpage/IAFD returns titles in all-lowercase; apply title-casing.
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
        m_results << result;
    }
}

} // namespace scraper
} // namespace mediaelch
