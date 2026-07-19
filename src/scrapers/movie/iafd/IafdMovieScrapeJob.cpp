#include "scrapers/movie/iafd/IafdMovieScrapeJob.h"

#include "data/movie/Movie.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextDocumentFragment>
#include <QTextStream>
#include <chrono>

namespace mediaelch {
namespace scraper {

/// Returns the path to the user-editable config file, seeding it from the
/// embedded resource the first time it is called.
static QString actorListConfigPath(const QString& filename, const QString& resourcePath)
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/iafd");
    QDir{}.mkpath(dir);
    const QString path = dir + QLatin1Char('/') + filename;
    if (!QFile::exists(path)) {
        // Seed from embedded resource on first run.
        QFile src(resourcePath);
        if (src.open(QIODevice::ReadOnly)) {
            QFile dst(path);
            if (dst.open(QIODevice::WriteOnly)) {
                dst.write(src.readAll());
            }
        }
    }
    return path;
}

void seedActorListConfigFiles()
{
    actorListConfigPath(QStringLiteral("IafdExcludeActors.txt"),
        QStringLiteral(":/src/scrapers/movie/iafd/IafdExcludeActors.txt"));
    actorListConfigPath(QStringLiteral("IafdPinnedActors.txt"),
        QStringLiteral(":/src/scrapers/movie/iafd/IafdPinnedActors.txt"));
}

static QSet<QString> loadActorList(const QString& filePath)
{
    QSet<QString> names;
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
                names.insert(line);
            }
        }
    }
    return names;
}

static QSet<QString> excludedActors()
{
    static const QString path = actorListConfigPath(QStringLiteral("IafdExcludeActors.txt"),
        QStringLiteral(":/src/scrapers/movie/iafd/IafdExcludeActors.txt"));
    return loadActorList(path);
}

static QSet<QString> pinnedActors()
{
    static const QString path = actorListConfigPath(QStringLiteral("IafdPinnedActors.txt"),
        QStringLiteral(":/src/scrapers/movie/iafd/IafdPinnedActors.txt"));
    return loadActorList(path);
}

IafdMovieScrapeJob::IafdMovieScrapeJob(IafdMovieApi& api, MovieScrapeJob::Config config, QObject* parent) :
    MovieScrapeJob(std::move(config), parent), m_api{api}
{
}

void IafdMovieScrapeJob::doStart()
{
    m_api.loadMovie(config().identifier.str(), [this](QString data, ScraperError error) {
        if (!error.hasError()) {
            parseAndAssignInfos(data);
        } else {
            setScraperError(error);
        }
        emitFinished();
    });
}

void IafdMovieScrapeJob::parseAndAssignInfos(const QString& html)
{
    using namespace std::chrono;

    // --- Title and Year from H1 (e.g. "Great Movie Name (2021)") ---
    {
        static const QRegularExpression h1Rx(
            R"re(<h1[^>]*>(.+?)\((\d{4})\)[^<]*</h1>)re",
            QRegularExpression::DotMatchesEverythingOption);
        const auto m = h1Rx.match(html);
        if (m.hasMatch()) {
            const QString title =
                QTextDocumentFragment::fromHtml(m.captured(1).trimmed()).toPlainText().trimmed();
            if (!title.isEmpty()) {
                m_movie->setTitle(title);
            }
            const int year = m.captured(2).toInt();
            if (year > 1800) {
                m_movie->setReleased(QDate(year, 1, 1));
            }
        }
    }

    // --- Biodata helper: extract plain text from the <p class="biodata"> that
    //     immediately follows a <p class="bioheading"> with a given label. ---
    auto getBiodata = [&](const QString& heading) -> QString {
        const QRegularExpression bioRx(
            QStringLiteral(R"re(<p class="bioheading">)re") + QRegularExpression::escape(heading)
                + QStringLiteral(R"re([^<]*</p>\s*<p class="biodata">(.*?)</p>)re"),
            QRegularExpression::DotMatchesEverythingOption);
        const auto m = bioRx.match(html);
        if (!m.hasMatch()) return {};
        const QString value = QTextDocumentFragment::fromHtml(m.captured(1)).toPlainText().trimmed();
        if (value.compare(QStringLiteral("No Data"), Qt::CaseInsensitive) == 0) return {};
        return value;
    };

    // --- Runtime (Minutes) ---
    {
        const QString mins = getBiodata(QStringLiteral("Minutes"));
        bool ok = false;
        const int n = mins.toInt(&ok);
        if (ok && n > 0) {
            m_movie->setRuntime(std::chrono::minutes(n));
        }
    }

    // --- Director ---
    {
        const QString director = getBiodata(QStringLiteral("Director"));
        if (!director.isEmpty()) {
            m_movie->setDirector(director);
        }
    }

    // --- Studio → also copied into Writer for Infuse compatibility ---
    {
        const QString studio = getBiodata(QStringLiteral("Studio"));
        if (!studio.isEmpty()) {
            m_movie->addStudio(studio);
            m_movie->setWriter(studio);
        }
    }

    // --- Plot (synopsis div → padded-panel → li text only) ---
    {
        static const QRegularExpression synopsisRx(
            R"re(<div[^>]+id="synopsis"[^>]*>.*?<ul>(.*?)</ul>)re",
            QRegularExpression::DotMatchesEverythingOption);
        const auto m = synopsisRx.match(html);
        if (m.hasMatch()) {
            QString raw =
                QTextDocumentFragment::fromHtml(m.captured(1)).toPlainText().trimmed();
            // Each line is a complete block (scene heading or full sentence) —
            // join them as sentences, adding a period when the line has none.
            const QStringList lines = raw.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                Qt::SkipEmptyParts);
            static const QString termPunct = QStringLiteral(".!?:;,");
            QString overview;
            for (const QString& rawLine : lines) {
                const QString line = rawLine.trimmed();
                if (line.isEmpty()) continue;
                if (!overview.isEmpty()) overview += QLatin1Char(' ');
                overview += line;
                if (!termPunct.contains(line.back())) {
                    overview += QLatin1Char('.');
                }
            }
            if (!overview.isEmpty()) {
                m_movie->setOverview(overview);
            }
        }
    }

    // --- Cast ---
    // Each performer is in their own <div class="castbox"><p>...</p></div>.
    // Scoped to castbox divs to avoid picking up director /person.rme links
    // in the biodata section.
    // Group 1: person href, 2: thumb URL, 3: name
    static const QRegularExpression castboxRx(
        R"re(<div[^>]+class="castbox"[^>]*>(.*?)</div\s*>)re",
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression actorRx(
        R"re(<a\s+href="(/person\.rme[^"]*)"[^>]*>\s*(?:<img[^>]+src="([^"]*)"[^>]*/?>)?\s*(?:<br\s*/?>\s*)?([^<\r\n]+?)\s*</a>)re",
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression creditedRx(
        R"re(\(Credited:\s*([^)]+)\))re",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stripTagsRx(QStringLiteral("<[^>]*>"));
    static const QRegularExpression collapseSpaceRx(QStringLiteral("\\s+"));

    // Load both lists fresh from disk once per scrape so edits take effect
    // without restarting the app.
    const QSet<QString> excluded = excludedActors();
    const QSet<QString> pinned = pinnedActors();

    QRegularExpressionMatchIterator castboxIt = castboxRx.globalMatch(html);
    while (castboxIt.hasNext()) {
        const QString boxContent = castboxIt.next().captured(1);

        const auto m = actorRx.match(boxContent);
        if (!m.hasMatch()) {
            continue;
        }

        const QString name =
            QTextDocumentFragment::fromHtml(m.captured(3).trimmed()).toPlainText().trimmed();
        if (name.isEmpty()) {
            continue;
        }
        if (excluded.contains(name)) {
            continue;
        }

        Actor actor;
        actor.name = name;

        const QString thumbSrc = m.captured(2).trimmed();
        if (!thumbSrc.isEmpty()
            && !thumbSrc.contains(QStringLiteral("nophoto"), Qt::CaseInsensitive)) {
            actor.thumb = thumbSrc.startsWith(QStringLiteral("//"))
                              ? QStringLiteral("https:") + thumbSrc
                              : thumbSrc;
        }

        // Everything after </a> in this castbox: strip tags, unescape &nbsp;,
        // extract (Credited: Name) separately, remainder is the role.
        QString afterAnchor = boxContent.mid(m.capturedEnd());
        afterAnchor.remove(stripTagsRx);
        afterAnchor.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
        afterAnchor = afterAnchor.trimmed().replace(collapseSpaceRx, QStringLiteral(" "));

        const auto creditedM = creditedRx.match(afterAnchor);
        if (creditedM.hasMatch()) {
            actor.creditedAs = creditedM.captured(1).trimmed();
            afterAnchor.remove(creditedM.capturedStart(), creditedM.capturedLength());
            afterAnchor = afterAnchor.trimmed().replace(collapseSpaceRx, QStringLiteral(" "));
        }

        if (!afterAnchor.isEmpty()) {
            actor.role = afterAnchor;
        }

        actor.order = pinned.contains(name) ? -1 : 0;

        m_movie->addActor(actor);
    }

    // Pinned ordering only matters when there are more actors than Infuse
    // displays (15). With 15 or fewer, all actors are visible anyway.
    if (m_movie->actors().size() <= 15) {
        for (Actor* a : m_movie->actors()) {
            if (a->order < 0) {
                a->order = 0;
            }
        }
    }
}

} // namespace scraper
} // namespace mediaelch
