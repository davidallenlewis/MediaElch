<div align="center">
	<img alt="MediaElch Logo" src="data/img/MediaElch.png" />
</div>

[![codecov](https://codecov.io/gh/Komet/MediaElch/branch/master/graph/badge.svg)](https://codecov.io/gh/Komet/MediaElch)
[![Coverity](https://img.shields.io/coverity/scan/19171.svg)](https://scan.coverity.com/projects/komet-mediaelch)
![Jenkins](https://jenkins.ameyering.de/buildStatus/icon?job=bugwelle%2FMediaElch%2Fmaster)

# MediaElch

MediaElch is a MediaManager for Kodi. Information about Movies, TV Shows,
Concerts and Music are stored as `NFO` files.
Fanarts are downloaded automatically from fanart.tv.

- Documentation: <https://mediaelch.github.io/mediaelch-doc/about.html>
- Blog: <https://mediaelch.github.io/mediaelch-blog/posts/>
- Source Code: <https://github.com/Komet/MediaElch>

![MediaElch Movies](https://mediaelch.github.io/mediaelch-doc/_images/movie-main.png)


## Download

Please visit <https://mediaelch.github.io/mediaelch-doc/download.html>


## Documentation

Documentation is available at <https://mediaelch.github.io/mediaelch-doc/about.html>.


### Build Instructions

For build instructions, see: https://mediaelch.github.io/mediaelch-doc/contributing/build/index.html


### Developer Documentation

For testing MediaElch, have a look at our developer/contributor
documentation in [`docs/README.md`](docs/README.md).


## Support

We're active in the [Kodi forum](https://forum.kodi.tv/showthread.php?tid=136333)
and on [GitHub](https://github.com/Komet/MediaElch).
If you want to report a bug, please see
["Bug Reports"](https://mediaelch.github.io/mediaelch-doc/contributing/bug-reports.html).

---

## Fork Modifications

This is a personal fork. The following files have been modified from upstream and
will need to be re-applied when rebasing onto a new upstream release:

### IAFD Scraper (new files — not in upstream)
- `src/scrapers/movie/iafd/` — entire directory (IafdMovieScraper, IafdMovieApi, IafdMovieSearchJob, IafdMovieScrapeJob, IafdMovieParser)

### NFO / XML Writers
- `src/media_center/kodi/MovieXmlWriter.cpp` — tab indentation (`setAutoFormattingIndent(-1)`), `<writer>` tag output
- `src/media_center/kodi/MovieXmlReader.cpp` — reads `<writer>` tag as fallback for `<credits>`
- `src/media_center/kodi/TvShowXmlWriter.cpp` — tab indentation
- `src/media_center/kodi/EpisodeXmlWriter.cpp` — tab indentation
- `src/media_center/kodi/ConcertXmlWriter.cpp` — tab indentation
- `src/media_center/kodi/AlbumXmlWriter.cpp` — tab indentation
- `src/media_center/kodi/ArtistXmlWriter.cpp` — tab indentation

### Movie Detail UI
- `src/ui/movies/MovieWidget.ui` — removed Logo/ClearArt/DiscArt/Banner art page; poster/fanart/thumb widened to 360px
- `src/ui/movies/MovieWidget.cpp` — removed art page toggle logic and logo/clearArt/cdArt/banner widget references; `setFixedSize(360)` for poster/backdrop/thumb
- `src/ui/movies/MovieWidget.h` — removed `onArtPageOne()`/`onArtPageTwo()` slot declarations
