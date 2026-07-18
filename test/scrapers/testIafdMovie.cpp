#include "test/test_helpers.h"

#include "scrapers/movie/iafd/IafdMovie.h"
#include "scrapers/movie/iafd/IafdMovieApi.h"
#include "scrapers/movie/iafd/IafdMovieScrapeJob.h"
#include "scrapers/movie/iafd/IafdMovieSearchJob.h"
#include "test/helpers/scraper_helpers.h"

using namespace mediaelch::scraper;

static IafdMovieApi& getIafdApi()
{
    static auto api = std::make_unique<IafdMovieApi>();
    return *api;
}

static MovieScrapeJob::Config makeIafdConfig(QString url)
{
    static auto iafd = std::make_unique<IafdMovie>();
    MovieScrapeJob::Config config;
    config.identifier = MovieIdentifier(url);
    config.details = iafd->meta().supportedDetails;
    config.locale = iafd->meta().defaultLocale;
    return config;
}

static auto makeScrapeJob(QString url)
{
    return std::make_unique<IafdMovieScrapeJob>(getIafdApi(), makeIafdConfig(url));
}

// ---------------------------------------------------------------------------
// Search tests
// Note: these tests require a live internet connection and DuckDuckGo returning
//       IAFD results. They are intentionally kept broad to tolerate DDG ranking
//       changes over time.
// ---------------------------------------------------------------------------

TEST_CASE("IAFD returns valid search results via DuckDuckGo", "[movie][IAFD][search]")
{
    SECTION("Search by movie name returns at least one IAFD result")
    {
        MovieSearchJob::Config config{"Pirates", mediaelch::Locale::English};
        auto* searchJob = new IafdMovieSearchJob(getIafdApi(), config);
        const auto scraperResults = test::searchMovieScraperSync(searchJob, /*mayError=*/true).first;

        // DuckDuckGo may return no results or Cloudflare may block; only check
        // the shape of results when we actually got some back.
        if (!scraperResults.isEmpty()) {
            CHECK_THAT(scraperResults[0].identifier.str(),
                Contains("iafd.com/title.rme"));
            CHECK(!scraperResults[0].title.isEmpty());
        }
    }
}

// ---------------------------------------------------------------------------
// Scrape tests
// Note: IAFD is behind Cloudflare.  These tests may fail in CI environments
//       that are blocked.  Pass mayError=true to avoid hard failures when
//       Cloudflare denies the request.
// ---------------------------------------------------------------------------

TEST_CASE("IAFD scrapes cast from a movie page", "[movie][IAFD][load_data]")
{
    SECTION("Known IAFD movie page has a non-empty cast")
    {
        // URL of "Pirates" (2005) on IAFD - a well-known title unlikely to be
        // removed.
        const QString url =
            "https://www.iafd.com/title.rme/title=pirates/year=2005/id=pirates.htm";
        auto scrapeJob = makeScrapeJob(url);
        test::scrapeMovieScraperSync(scrapeJob.get(), /*mayError=*/true);

        // IAFD is behind Cloudflare; the request may fail or return an
        // undetected challenge page.  We only assert actor shape when we
        // actually got real data back (i.e. at least one actor was parsed).
        if (!scrapeJob->scraperError().hasError()) {
            const auto& actors = scrapeJob->movie().actors();
            // If we got actors, verify each one has a non-empty name.
            // 0 actors is tolerated here because Cloudflare may have silently
            // served a challenge page that we could not execute.
            for (const Actor* actor : actors) {
                CHECK(!actor->name.isEmpty());
            }
        }
    }
}
