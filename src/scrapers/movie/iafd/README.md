# IAFD Movie Scraper

Scraper for [IAFD (Internet Adult Film Database)](https://www.iafd.com) built as a fork of [MediaElch](https://github.com/Komet/MediaElch).

## Features

- Search via Startpage (no direct IAFD search traffic — avoids IP bans and Cloudflare blocks)
- URL paste: paste an IAFD movie URL directly into the search box to skip search entirely
- Metadata: title, year, runtime, director(s), studio, scene breakdown, synopsis, IAFD URL
- Studio written as `<writer>` for Infuse studio-browsing compatibility
- Full cast with headshots and role descriptions
- `(Credited: Name)` aliases extracted into a custom `<credited>` NFO tag
- 773-actor exclusion list (male performers) to keep cast lists focused on female performers
- Actors sorted: pinned (alpha) first, then thumbed (alpha), then unthumbed (alpha)
- Actor exclusion and pinned lists are live config files — editable without recompiling
- Cloudflare detection with clear error messages

## Scraping behaviour

| Field | NFO tag | IAFD behaviour | If IAFD has no data |
|---|---|---|---|
| Title | `<title>` | Always overwrites | IAFD always has it |
| Director | `<director>` | Overwrites; supports multiple directors | Existing NFO value kept |
| Released | `<premiered>` `<year>` | Overwrites if valid date | Existing NFO value kept |
| Runtime | `<runtime>` | Overwrites if > 0 | Existing NFO value kept |
| Synopsis | `<outline>` | Overwrites with IAFD synopsis | Existing NFO value kept |
| Scenes | `<plot>` | Overwrites with scene-by-scene breakdown | Existing NFO value kept |
| Genres | `<genre>` | Adds `Compilation` when applicable | Existing NFO genres kept |
| Studios | `<studio>` | Adds IAFD studio | Existing NFO studios kept |
| Actors | `<actor>` | *Fully replaces *disambiguated `(N)` actors kept *sort by sticky then w/ and w/o thumb *eliminate men | Actors fully replaced |
| Writer | `<writer>` | Set to IAFD studio name (for Infuse studio browsing) | ⚠️ Existing value wiped |
| ID | `<id>` | IAFD movie URL | Not set |

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
| `src/media_center/kodi/KodiXmlWriter.cpp` | Actor sort order (pinned → thumbed → unthumbed), `<credited>` tag, `<thumb>` always written |
| `src/media_center/kodi/MovieXmlWriter.cpp` | Writes `<writer>` tag alongside `<credits>` |
| `CMakeLists.txt` (iafd subfolder) | Build integration |
| `ScraperManager.cpp` | Scraper registration |
| `ui.qrc` | Exclusion and pinned actor list resources (seed files only) |

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

## Actor list config files

Two plain-text config files control which actors are excluded and which are pinned to the top of the cast list:

| File | Purpose |
|------|---------|
| `IafdExcludeActors.txt` | Actors never added to the cast (773 male performers by default) |
| `IafdPinnedActors.txt` | Actors always sorted first — useful since Infuse only shows the first 15 |

One name per line. Lines starting with `#` are ignored.

Both lists are **reloaded from disk on every scrape** — no rebuild or restart needed after editing.

### Where the app reads them

```
~/Library/Application Support/kvibes/MediaElch/iafd/IafdExcludeActors.txt
~/Library/Application Support/kvibes/MediaElch/iafd/IafdPinnedActors.txt
```

The app seeds these files from the embedded resources the first time it launches. After that, only the files on disk are used.

### Keeping them in sync with the repo (recommended)

Replace the seeded copies with symlinks back to the source files so there is only one canonical file — version-tracked in git, but editable in any text editor without recompiling:

```bash
ln -sf "$(pwd)/src/scrapers/movie/iafd/IafdPinnedActors.txt" \
  ~/Library/Application\ Support/kvibes/MediaElch/iafd/IafdPinnedActors.txt

ln -sf "$(pwd)/src/scrapers/movie/iafd/IafdExcludeActors.txt" \
  ~/Library/Application\ Support/kvibes/MediaElch/iafd/IafdExcludeActors.txt
```

Run these commands once from the repo root. If you wipe `~/Library` (e.g. clean macOS reinstall), re-run them to restore the symlinks.
