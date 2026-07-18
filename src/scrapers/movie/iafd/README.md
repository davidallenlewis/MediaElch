# IAFD Movie Scraper

Scraper for [IAFD (Internet Adult Film Database)](https://www.iafd.com) built as a fork of [MediaElch](https://github.com/Komet/MediaElch).

## Features

- Search via Startpage (no direct IAFD search traffic — avoids IP bans and Cloudflare blocks)
- URL paste: paste an IAFD movie URL directly into the search box to skip search entirely
- Metadata: title, year, runtime, director, studio, plot
- Studio written as both `<credits>` and `<writer>` for Infuse compatibility
- Full cast with headshots and role descriptions
- `(Credited: Name)` aliases extracted into a custom `<credited>` NFO tag
- 773-actor exclusion list (male performers) to keep cast lists focused on female performers
- Actors sorted: thumbed (alpha) first, then unthumbed (alpha)
- Cloudflare detection with clear error messages

## Maintaining this fork

### First-time setup

After cloning, add the upstream MediaElch remote:

```bash
git remote add upstream https://github.com/Komet/MediaElch.git
```

### Pulling in upstream MediaElch updates

When MediaElch releases a new version:

```bash
git fetch upstream
git rebase upstream/master
```

Resolve any conflicts (likely only in the integration-point files below), then:

```bash
git push
```

### Files changed outside `src/scrapers/movie/iafd/`

These are the only files modified compared to upstream MediaElch:

| File | Change |
|------|--------|
| `src/data/Actor.h` | Added `creditedAs` field |
| `src/media_center/kodi/KodiXmlWriter.cpp` | Actor sort order, `<credited>` tag, `<thumb>` always written |
| `src/media_center/kodi/MovieXmlWriter.cpp` | Writes `<writer>` tag alongside `<credits>` |
| `CMakeLists.txt` (iafd subfolder) | Build integration |
| `ScraperManager.cpp` | Scraper registration |
| `ui.qrc` | Exclusion list resource |

These are small, isolated changes. Rebase conflicts here will be infrequent and trivial to resolve.

### Build

```bash
/opt/homebrew/bin/cmake --preset debug -DCMAKE_CXX_COMPILER_LAUNCHER=""
/opt/homebrew/bin/cmake --build build/debug --target MediaElch -j$(sysctl -n hw.logicalcpu)
open build/debug/MediaElch.app
```

Quick rebuild after changes:

```bash
/opt/homebrew/bin/cmake --build build/debug --target MediaElch -j$(sysctl -n hw.logicalcpu) && open build/debug/MediaElch.app
```

> **Note:** Changing `Actor.h` triggers a full recompile of most of the codebase — this takes several minutes. Changes only inside `src/scrapers/movie/iafd/` rebuild in seconds.
