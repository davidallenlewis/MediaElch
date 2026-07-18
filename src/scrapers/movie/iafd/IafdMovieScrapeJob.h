#pragma once

#include "scrapers/movie/MovieScrapeJob.h"
#include "scrapers/movie/iafd/IafdMovieApi.h"

namespace mediaelch {
namespace scraper {

/// Ensures the user-editable IAFD actor list config files exist on disk,
/// seeding them from the embedded resources if necessary.
void seedActorListConfigFiles();

class IafdMovieScrapeJob final : public MovieScrapeJob
{
    Q_OBJECT

public:
    explicit IafdMovieScrapeJob(IafdMovieApi& api, MovieScrapeJob::Config config, QObject* parent = nullptr);
    ~IafdMovieScrapeJob() override = default;

    void doStart() override;

private:
    void parseAndAssignInfos(const QString& html);

private:
    IafdMovieApi& m_api;
};

} // namespace scraper
} // namespace mediaelch
