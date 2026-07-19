#include "scrapers/movie/MovieMerger.h"

#include "data/movie/Movie.h"
#include "log/Log.h"

#include <QRegularExpression>
#include <chrono>

namespace mediaelch {
namespace scraper {

namespace {

using namespace std::chrono_literals;

// TODO: Option "only replace if source has value"
void copyDetailToMovie(Movie& target,
    const Movie& source,
    MovieScraperInfo detail,
    bool usePlotForOutline,
    bool ignoreDuplicateOriginalTitle)
{
    if (source.tmdbId().isValid()) {
        target.setTmdbId(source.tmdbId());
    }
    if (!source.imdbId().toString().isEmpty()) {
        target.setImdbId(source.imdbId());
    }

    switch (detail) {
    case MovieScraperInfo::Invalid: {
        qCCritical(generic) << "[MovieMerger] Cannot copy details 'invalid'";
        break;
    }
    case MovieScraperInfo::Title: {
        target.setTitle(source.title());
        if (!ignoreDuplicateOriginalTitle || source.title() != source.originalTitle()) {
            target.setOriginalTitle(source.originalTitle());
        }
        break;
    }
    case MovieScraperInfo::Tagline: {
        target.setTagline(source.tagline());
        break;
    }
    case MovieScraperInfo::Rating: {
        target.ratings().merge(source.ratings());
        break;
    }
    case MovieScraperInfo::Released: {
        if (source.released().isValid()) {
            target.setReleased(source.released());
        }
        break;
    }
    case MovieScraperInfo::Runtime: {
        if (source.runtime() > 0min) {
            target.setRuntime(source.runtime());
        }
        break;
    }
    case MovieScraperInfo::Certification: {
        target.setCertification(source.certification());
        break;
    }
    case MovieScraperInfo::Trailer: {
        target.setTrailer(source.trailer());
        break;
    }
    case MovieScraperInfo::TvShowLinks: {
        target.setTvShowLinks(source.tvShowLinks());
        break;
    }
    case MovieScraperInfo::Overview: {
        if (!source.overview().isEmpty()) {
            target.setOverview(source.overview());
        }
        break;
    }
    case MovieScraperInfo::Outline: {
        if (!source.outline().isEmpty()) {
            target.setOutline(source.outline());
        } else if (target.outline().isEmpty() && usePlotForOutline && !source.overview().isEmpty()) {
            target.setOutline(source.overview());
        }
        break;
    }
    case MovieScraperInfo::Poster: {
        const auto& sourceImages = source.constImages().posters();
        for (const Poster& poster : sourceImages) {
            target.images().addPoster(poster);
        }
        break;
    }
    case MovieScraperInfo::Backdrop: {
        const auto& sourceImages = source.constImages().backdrops();
        for (const Poster& backdrop : sourceImages) {
            target.images().addBackdrop(backdrop);
        }
        break;
    }
    case MovieScraperInfo::Actors: {
        // Preserve any manually-disambiguated actors (e.g. "Heidi (2)") before
        // replacing with scraped actors.  These entries are user edits added to
        // resolve name collisions in Infuse and must survive a re-scrape.
        //
        // Merge rule: if a scraped actor's name matches the base name of a
        // preserved actor (strip the " (N)" suffix), substitute the
        // disambiguated name so Infuse sees "Heidi (2)" instead of a plain
        // "Heidi" duplicate.  The "(N)" suffix is itself proof of intentional
        // user disambiguation, so a base-name match alone is sufficient.
        static const QRegularExpression disambigRx(QStringLiteral(R"(\s*\(\d+\)\s*$)"));

        struct PreservedActor {
            Actor actor;
            QString baseName; // name with the " (N)" suffix stripped
            bool merged = false;
        };
        QVector<PreservedActor> preserved;
        for (const Actor* a : target.actors()) {
            if (disambigRx.match(a->name).hasMatch()) {
                QString base = a->name;
                base.remove(disambigRx);
                preserved.append({*a, base.trimmed(), false});
            }
        }

        // Index preserved actors by their base name for fast lookup.
        QMap<QString, QVector<int>> byBaseName;
        for (int i = 0; i < preserved.size(); ++i) {
            byBaseName[preserved[i].baseName].append(i);
        }

        target.setActors({});
        const auto& sourceActors = source.actors();
        for (const Actor* sourceActor : sourceActors) {
            bool substituted = false;
            if (byBaseName.contains(sourceActor->name)) {
                // The presence of "(N)" is an intentional user disambiguation,
                // so a base-name match alone is sufficient to merge.
                for (int idx : byBaseName[sourceActor->name]) {
                    PreservedActor& p = preserved[idx];
                    if (!p.merged) {
                        Actor merged = *sourceActor;
                        merged.name = p.actor.name;
                        target.addActor(merged);
                        p.merged = true;
                        substituted = true;
                        break;
                    }
                }
            }
            if (!substituted) {
                target.addActor(*sourceActor);
            }
        }

        // Re-add any preserved actors that weren't merged into a scraped entry.
        QSet<QString> addedNames;
        for (const Actor* a : target.actors()) {
            addedNames.insert(a->name);
        }
        for (const PreservedActor& p : preserved) {
            if (!p.merged && !addedNames.contains(p.actor.name)) {
                target.addActor(p.actor);
            }
        }
        break;
    }
    case MovieScraperInfo::Genres: {
        const auto& genres = source.genres();
        for (const auto& genre : genres) {
            target.addGenre(genre);
        }
        break;
    }
    case MovieScraperInfo::Studios: {
        const auto& studios = source.studios();
        for (const auto& studio : studios) {
            target.addStudio(studio);
        }
        break;
    }
    case MovieScraperInfo::Countries: {
        const auto& countries = source.countries();
        for (const auto& country : countries) {
            target.addCountry(country);
        }
        break;
    }
    case MovieScraperInfo::Writer: {
        if (!source.writer().isEmpty()) {
            target.setWriter(source.writer());
        }
        break;
    }
    case MovieScraperInfo::Director: {
        if (!source.director().isEmpty()) {
            target.setDirector(source.director());
        }
        break;
    }
    case MovieScraperInfo::Tags: {
        const auto& tags = source.tags();
        for (const auto& tag : tags) {
            target.addTag(tag);
        }
        break;
    }
    case MovieScraperInfo::ExtraFanarts: {
        // no-op: Are loaded from disk only.
        break;
    }
    case MovieScraperInfo::Set: {
        target.setSet(source.set());
        break;
    }
    case MovieScraperInfo::Logo: {
        const auto& logos = source.constImages().logos();
        for (const auto& logo : logos) {
            target.images().addLogo(logo);
        }
        break;
    }
    case MovieScraperInfo::CdArt: {
        const auto& images = source.constImages().discArts();
        for (const auto& discArt : images) {
            target.images().addDiscArt(discArt);
        }
        break;
    }
    case MovieScraperInfo::ClearArt: {
        const auto& images = source.constImages().clearArts();
        for (const auto& clearArt : images) {
            target.images().addClearArt(clearArt);
        }
        break;
    }
    case MovieScraperInfo::Banner: {
        const QByteArray banner = source.constImages().image(ImageType::MovieBanner);
        if (!banner.isEmpty()) {
            target.images().setImage(ImageType::MovieBanner, banner);
        }
        break;
    }
    case MovieScraperInfo::Thumb: {
        const QByteArray thumb = source.constImages().image(ImageType::MovieThumb);
        if (!thumb.isEmpty()) {
            target.images().setImage(ImageType::MovieThumb, thumb);
        }
        break;
    }
    }
}

} // namespace

void copyDetailsToMovie(Movie& target,
    const Movie& source,
    const QSet<MovieScraperInfo>& details,
    bool usePlotForOutline,
    bool ignoreDuplicateOriginalTitle)
{
    const bool wasBlocked = target.blockSignals(true);
    for (MovieScraperInfo detail : details) {
        copyDetailToMovie(target, source, detail, usePlotForOutline, ignoreDuplicateOriginalTitle);
    }
    target.blockSignals(wasBlocked);
}

} // namespace scraper
} // namespace mediaelch
