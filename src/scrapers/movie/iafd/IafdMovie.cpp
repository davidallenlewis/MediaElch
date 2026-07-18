#include "scrapers/movie/iafd/IafdMovie.h"

#include "scrapers/movie/iafd/IafdMovieScrapeJob.h"
#include "scrapers/movie/iafd/IafdMovieSearchJob.h"

namespace mediaelch {
namespace scraper {

const char* const IafdMovie::ID = "iafd";

IafdMovie::IafdMovie(QObject* parent) : MovieScraper(parent)
{
    m_meta.identifier = ID;
    m_meta.name = "IAFD";
    m_meta.description = tr("Internet Adult Film Database (IAFD) is a comprehensive database for adult content.");
    m_meta.website = "https://www.iafd.com";
    m_meta.termsOfService = "https://www.iafd.com/misc.rme/page=terms.htm";
    m_meta.privacyPolicy = "https://www.iafd.com/misc.rme/page=privacy.htm";
    m_meta.help = "https://www.iafd.com";
    m_meta.supportedDetails = {
        MovieScraperInfo::Title,
        MovieScraperInfo::Released,
        MovieScraperInfo::Runtime,
        MovieScraperInfo::Overview,
        MovieScraperInfo::Director,
        MovieScraperInfo::Studios,
        MovieScraperInfo::Writer,
        MovieScraperInfo::Actors,
    };
    m_meta.supportedLanguages = {"en"};
    m_meta.defaultLocale = "en";
    m_meta.isAdult = true;
}

const MovieScraper::ScraperMeta& IafdMovie::meta() const
{
    return m_meta;
}

void IafdMovie::initialize()
{
    seedActorListConfigFiles();
}

bool IafdMovie::isInitialized() const
{
    return true;
}

MovieSearchJob* IafdMovie::search(MovieSearchJob::Config config)
{
    return new IafdMovieSearchJob(m_api, std::move(config), this);
}

MovieScrapeJob* IafdMovie::loadMovie(MovieScrapeJob::Config config)
{
    return new IafdMovieScrapeJob(m_api, std::move(config), this);
}

QSet<MovieScraperInfo> IafdMovie::scraperNativelySupports()
{
    return m_meta.supportedDetails;
}

void IafdMovie::changeLanguage(mediaelch::Locale /*locale*/)
{
    // IAFD is English-only; language changes are no-ops.
}

} // namespace scraper
} // namespace mediaelch
