#pragma once

#include "scrapers/movie/MovieSearchJob.h"
#include "scrapers/movie/iafd/IafdMovieApi.h"

#include <QVector>

namespace mediaelch {
namespace scraper {

class IafdMovieSearchJob final : public MovieSearchJob
{
    Q_OBJECT

public:
    explicit IafdMovieSearchJob(IafdMovieApi& api, MovieSearchJob::Config config, QObject* parent = nullptr);
    ~IafdMovieSearchJob() override = default;

    void doStart() override;

private:
    static QList<MovieSearchJob::Result> parseSearchStartpage(const QString& html);
    static QList<MovieSearchJob::Result> parseSearchDDG(const QString& html);
    void runSearch(const QString& query);
    void runDDGFallback(const QString& query);

private:
    IafdMovieApi& m_api;
    bool m_retried = false;
};

} // namespace scraper
} // namespace mediaelch
