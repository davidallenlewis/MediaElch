#pragma once

#include "scrapers/movie/MovieScrapeJob.h"
#include "scrapers/movie/iafd/IafdMovieApi.h"

namespace mediaelch {
namespace scraper {

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
