# SpacePenguin

A secure, lightweight, and usable browser written in Qt.

**Status: pre-alpha (v0.0.1).** It opens web pages in tabs and gets out of the
way. Everything below is what actually exists in the tree today — no aspirational
checkbox list.

## What it does

- **Tabs** — tab strip on top; new, close, middle-click to close, drag to reorder,
  duplicate, close others, reopen closed, session restored on restart. Links that ask
  for a new tab or window (`target=_blank`, "Open link in new tab") open as tabs
- **Address bar** — takes URLs, `about:` pages and plain-language queries; anything
  that isn't a URL becomes a search on the chosen engine (DuckDuckGo by default).
  Suggests matching bookmarks and history as you type; shows a lock, warning or info
  icon for the page's security state and a star to bookmark it
- **Navigation** — back, forward, reload, stop, home
- **Find in page** — `Ctrl+F`, with match count and next/previous
- **Zoom** — per-tab, `Ctrl`+`+` / `Ctrl`+`-` / `Ctrl`+`0`
- **Windows** — new window, new private window (`Ctrl`+`Shift`+`N`); private
  windows never write a session, cookies or cache to disk
- **Bookmarks** — `Ctrl`+`D` bookmarks the page, the star edits it; a bookmarks bar
  (`Ctrl`+`Shift`+`B`) and a searchable manager (`Ctrl`+`Shift`+`O`)
- **History** — searchable history window (`Ctrl`+`H`) with delete and clear;
  `about:history` lists recent visits. Private windows record nothing
- **Downloads** — downloads window (`Ctrl`+`J`) with progress, cancel, open and
  show in folder
- **Start page** — search box for the chosen engine and tiles for your most
  visited sites, served locally by `sp://start/`
- **Load state** — thin progress line under the toolbar, hovered link preview,
  and a shield button with the blocked-request count and blocking controls
- **Settings** — `Ctrl`+`,`: start-up, home page, search engine, theme, toolbar,
  ad blocking and paused sites, data saver, clearing history/cookies/cache,
  download folder
- **Theme** — follow system, light or dark (`View → Theme`), remembered between
  runs
- **Content blocking** — an Adblock Plus style filter list, applied through a
  request interceptor
- **Extensions** — JavaScript user scripts, installed per profile and managed
  from `Tools → Extensions…`

## Theme

`☰ → Theme` (or `View → Theme`, or Settings → Appearance) offers **Follow
system**, **Light** and **Dark**. The choice is written to `QSettings` under
`appearance/theme` and reapplied at startup.

SpacePenguin applies the mode to the application chrome with
`QStyleHints::setColorScheme` plus its own palette and stylesheet, drawn on the
Fusion style with a built-in icon set so the window looks the same on every
platform. The start page and `about:` pages follow the same choice. It does
**not** restyle websites: those that honour `prefers-color-scheme` follow the OS
setting on their own, and those that ignore it are left alone. Claiming otherwise
would be a lie in the UI.

## Extensions

QtWebEngine does not load Chrome extensions and never will without a native
Chromium build. SpacePenguin does not fake support. What it ships is
**JavaScript user scripts**, installed as `QWebEngineScript` objects on the
active profile.

Scripts live in `<app data>/extensions`. Drop a `.js` file in, or use
`Tools → Extensions…` (`Ctrl`+`Shift`+`E`) to add, enable, disable or remove
one. Metadata in a leading comment controls what a script runs on:

```js
// @name Readability helper
// @match *://example.com/*, *://example.org/*
console.log('running on', location.hostname);
```

- `// @name` — the label shown in the manager; defaults to the file name
- `// @match` — comma-separated host patterns; without it the script runs on
  every page
- The script body is wrapped in a guard that checks `location.hostname` against
  the patterns and reports failures to the page console instead of throwing

Private windows run no user scripts: the extension directory is not attached to
an off-the-record profile.

## Content blocking

`AdBlocker` is a `QWebEngineUrlRequestInterceptor` installed on every profile,
backed by an Adblock Plus format filter list. Two mechanisms work together:

- **Network rules** stop requests before they leave the browser
- **Cosmetic rules** (`##selector`, `domain.com##selector`, `#@#` to allow one
  back) are compiled into a stylesheet injected at document creation, which is
  what removes ads a network blocker cannot see: overlays, interstitials, in-page
  popups and the player's own ad slots

YouTube is the clearest example of why both are needed. It serves its ad
requests from `www.youtube.com` itself, so a network rule can only target the ad
*endpoints* — `/pagead/`, `/api/stats/ads`, `/premium_ads`,
`/youtubei/v1/player/ad_break`. The visible ad, the overlay and the pop-out
player are elements in the page, and only cosmetic filtering touches those.

### Rule syntax supported

| Form | Meaning |
| ---- | ------- |
| `host` or `ads.example.com/path` | matches anywhere in the address |
| `\|\|host^` | host and all its subdomains |
| `\|start` / `end\|` | anchored at the start or end of the address |
| `^` | separator: any character that is not part of a host name |
| `*` | wildcard |
| `/path?` | end of address |
| `@@` prefix | exception, always wins |
| `$domain=a.com\|b.com`, `$domain=~a.com` | limit to, or exclude, top-level sites |
| `$third-party` / `$3p` | only cross-site requests |
| `$script`, `$image`, `$xhr`, `$document`, `$popup`, … | resource type |
| `$match-case` | case sensitive |
| `##selector`, `a.com##selector`, `#@#selector` | cosmetic hide / exception |

Not supported: `$redirect` and `$removeparam` rewrites, scriptlet rules
(`##+js(...)`), generic-hiding exceptions, and per-site exception editing.

`$generichide` and `$elemhide` rules are ignored for network decisions. That
matters in practice: EasyList ships `@@||www.youtube.com^$generichide`, and
treating it as a network exception silently disables that site's own rules.

### Filter lists

A fresh install writes a small starter list to
`<app data>/adblock/filters.txt` so blocking is visible immediately. It is
deliberately minimal — the ad hosts and YouTube ad endpoints, written for this
project.

For real coverage, point SpacePenguin at a maintained list:

```sh
spacepenguin --filter-list ~/lists/easylist.txt
```

EasyList is GPLv3, the same license as SpacePenguin, so you can redistribute a
build that vendors it under GPLv3 terms. It is not committed here because the list
changes several times a week and a fresh clone should stay small.

Measured against EasyList, SpacePenguin parses 53,859 network rules and 24,009
cosmetic rules, and 13,650 cosmetic selectors apply to `youtube.com`.

A full list means a large injected stylesheet on every page. That costs some
memory and a little load time; if you notice it, use a smaller list.

### Pop-ups

Actual pop-up windows are already refused by the security policy
(`JavascriptCanOpenWindows` is off), and `$popup` rules are honoured for
anything a page tries to load as a document. In-page pop-ups — interstitials,
"pop-out" players, overlay dialogs — are cosmetic, so they need `##` rules from
your list.

## Internal pages

Type any of these in the address bar. They are rendered by the browser itself,
never fetched from the network.

| Page            | What it shows                                              |
| --------------- | ---------------------------------------------------------- |
| `about:`          | Index of every internal page                               |
| `about:about`     | The same index, with descriptions                          |
| `about:version`   | Version, build, Qt, platform, theme, profile, blocking rules, extension count |
| `about:license`   | SpacePenguin and Qt licensing                              |
| `about:blank`     | Handled by the rendering engine                            |
| `about:penguin`   | 🐧                                                        |
| `about:teapot`    | RFC 2324                                                   |
| `about:pan`       | You have been panned                                       |
| `about:force`     | Use the Force                                              |
| `about:deathstar` | That's No Moon                                             |
| `about:yoda`      | Wise, You Are                                              |
| `about:hyperdrive`| Punch It                                                   |
| `about:mozilla`   | Book of Mozilla (verses)                                   |
| `about:firefox`   | Phoenix Rising                                             |
| `about:servo`     | Fearless Concurrency                                       |
| `about:rust`      | Empowering Everyone                                        |
| `about:downloads` | Download manager                                           |
| `about:history`   | Browsing history                                           |
| `about:extensions`| User scripts and extensions                                |
| `about:settings`  | Open the settings dialog                                   |
| `about:tatooine`  | Tatooine                                                   |
| `about:hoth`      | Hoth                                                       |
| `about:dagobah`   | Dagobah                                                    |
| `about:endor`     | Endor                                                      |
| `about:naboo`     | Naboo                                                      |
| `about:coruscant` | Coruscant                                                  |

Any `about:` page not in the list above gets an internal "no such page" answer instead of a network error.


## Security posture

These are deliberate choices, not defaults we inherited:

- Top-level navigation is restricted to `http`, `https`, `sp`, `qrc`, `about` and
  `data`. `file://` and `javascript:` top-level loads are refused.
- The built-in start page is served by our own `sp://` scheme handler
  (`src/startpageschemehandler.cpp`) rather than `qrc:` — Chromium's network
  stack cannot read Qt resources, so a `qrc:` start page silently fails to load.
- `LocalContentCanAccessRemoteUrls` and `LocalContentCanAccessFileUrls` are off,
  so a page from the local disk or an in-app page can't reach the network.
- `AllowRunningInsecureContent` is off — no mixed content, ever.
- `JavascriptCanOpenWindows` is off until we implement popup windows properly.
  Links the user opens in a new tab or window become tabs; script pop-ups and
  dialogs are still refused.
- No telemetry, no background services, no auto-updater, no remote
  configuration. The browser does not phone home.

## Keyboard

Every binding below is implemented and wired to a menu entry (the ☰ menu, or the
menu bar); `F1` (`⌘?` on macOS) opens the same list from inside the app, so it
cannot drift from the code. All bindings live in one table, `src/shortcuts.cpp`,
and a test checks that no two commands share a key on the platform it runs on.

macOS uses `⌘` where Windows and Linux use `Ctrl`. Where a binding would collide
with the desktop itself — `⌘H` hides the app, `⌘Tab` switches apps, `F11` shows
the desktop — macOS gets the binding Safari and Chrome use there instead.

| Action                | Windows / Linux                 | macOS                         |
| --------------------- | ------------------------------- | ----------------------------- |
| New tab               | `Ctrl+T`                        | `⌘T`                          |
| Close tab             | `Ctrl+W`                        | `⌘W`                          |
| Reopen closed tab     | `Ctrl+Shift+T`                  | `⇧⌘T`                         |
| Next / previous tab   | `Ctrl+Tab` / `Ctrl+Shift+Tab`, `Ctrl+PgDn` / `Ctrl+PgUp` | `⌃Tab` / `⌃⇧Tab`, `⌥⌘→` / `⌥⌘←`, `⇧⌘]` / `⇧⌘[` |
| Go to tab 1–8 / last  | `Ctrl+1` … `Ctrl+8` / `Ctrl+9`  | `⌘1` … `⌘8` / `⌘9`            |
| New window            | `Ctrl+N`                        | `⌘N`                          |
| New private window    | `Ctrl+Shift+N`                  | `⇧⌘N`                         |
| Close window          | `Ctrl+Shift+W`                  | `⇧⌘W`                         |
| Quit                  | `Ctrl+Q`                        | `⌘Q`                          |
| Back / forward        | `Alt+Left` / `Alt+Right`        | `⌘[` / `⌘]`                   |
| Reload                | `F5`, `Ctrl+R`                  | `⌘R`                          |
| Stop loading          | `Esc` (while a load is in flight) | `Esc`                       |
| Home                  | `Alt+Home`                      | `⇧⌘H`                         |
| Focus address bar     | `Ctrl+L`, `Alt+D`, `F6`         | `⌘L`                          |
| Find in page          | `Ctrl+F`                        | `⌘F`                          |
| Zoom in / out / reset | `Ctrl++` / `Ctrl+-` / `Ctrl+0`  | `⌘+` / `⌘-` / `⌘0`            |
| Full screen           | `F11`                           | `⌃⌘F`                         |
| Bookmark this page    | `Ctrl+D`                        | `⌘D`                          |
| Show bookmarks bar    | `Ctrl+Shift+B`                  | `⇧⌘B`                         |
| Bookmarks             | `Ctrl+Shift+O`                  | `⌥⌘B`                         |
| History               | `Ctrl+H`                        | `⌘Y`                          |
| Downloads             | `Ctrl+J`                        | `⌥⌘L`                         |
| Extensions            | `Ctrl+Shift+E`                  | `⇧⌘E`                         |
| Settings              | `Ctrl+,`                        | `⌘,`                          |
| Keyboard shortcuts    | `F1`                            | `⌘?`                          |

The start page and the `about:` pages show the bindings for the platform you are
on.

## Platforms

SpacePenguin is one CMake project for all three desktops. CI
(`.github/workflows/build.yml`) builds it, runs every test and produces a package
on each of them for every push.

| Platform | Built with | Package | Notes |
| -------- | ---------- | ------- | ----- |
| Linux (X11, Wayland) | GCC, Qt 6.8 | AppImage | desktop entry, AppStream metadata and icons installed under `share/` |
| Windows 10/11 x64 | MSVC 2022, Qt 6.8 | NSIS installer and portable zip | icon and version info embedded; no console window |
| macOS 12+ (Apple silicon and Intel with a universal Qt) | Apple Clang, Qt 6.8 | DMG | `SpacePenguin.app` with icon, registered for `http`/`https` |

What is handled per platform:

- **Look** — every platform uses the Fusion style with SpacePenguin's own palette,
  stylesheet and painted icons, so the window is the same everywhere and follows
  the system light/dark setting (including the dark title bar on Windows 11)
- **Menus** — on macOS the menu bar is the system's, with Quit, About and Settings
  in the application menu; on Windows and Linux it is off by default and
  everything is in the ☰ menu
- **One process per profile** — Chromium locks a profile to one process, so a
  second launch (a link clicked in another app, a second click on the launcher,
  the "New Private Window" jump-list or dock action) hands its pages to the
  running browser over a local socket (a named pipe on Windows) and exits.
  macOS delivers links opened from Finder or sent to the default browser as
  events; those open in the front window too
- **Default browser** — the Linux desktop entry, the Windows installer (through
  Settings › Apps › Default apps, never silently) and the macOS `Info.plist` all
  register SpacePenguin as a handler for `http`, `https` and HTML files
- **Deployment** — on Windows and macOS `cmake --install` runs `windeployqt` /
  `macdeployqt`, so the install prefix is a self-contained app. On Linux the
  install uses the system's Qt through a generated `qt.conf`, and the AppImage
  bundles Qt with `linuxdeploy`
- **Source encoding** — MSVC builds with `/utf-8`, so strings such as "…" and
  "—" come out right
- `about:version` reports the running Qt platform plugin (`xcb`, `wayland`,
  `windows`, `cocoa`)

Platform data locations come from `QStandardPaths`, so the profile lands in
`~/.local/share/SpacePenguin` on Linux, `~/Library/Application Support` on
macOS, and `%APPDATA%` on Windows.

## Building

Requires Qt 6.8 or newer with QtWebEngine, CMake 3.21 or newer, and a C++20
compiler: GCC 11+ or Clang 14+ on Linux, MSVC 2022 on Windows, Xcode 14+ on macOS.

```sh
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<gcc_64|msvc2022_64|macos>
cmake --build build --config Release
```

Run it from the build tree:

| Platform | Command |
| -------- | ------- |
| Linux    | `./build/SpacePenguin` |
| Windows  | `build\Release\SpacePenguin.exe` (Visual Studio generator) or `build\SpacePenguin.exe` (Ninja), with Qt's `bin` on `PATH` |
| macOS    | `open build/SpacePenguin.app` |

On Nix:

```sh
nix-shell -p 'qt6.qtbase' 'qt6.qtdeclarative' 'qt6.qtwebengine' cmake gnumake libglvnd --run \
  'cmake -B build && cmake --build build && ./build/SpacePenguin'
```

To get a copy you can move elsewhere, install it:
`cmake --install build --config Release --prefix <absolute dir>` (Qt's deploy step
rejects a relative prefix). On Windows and macOS that
deploys Qt into `<dir>`; on Linux it installs the binary, a `qt.conf` pointing at
the Qt it was built with, the desktop entry, metadata and icons.

### Options

```
--private               do not write history, cookies, or cache for this session
--user-data-dir <dir>   store persistent data in <dir>
--filter-list <file>    load ad blocking rules from <file>
[urls...]               open each address or search in a new tab
```

### Tests

```sh
ctest --test-dir build -C Release --output-on-failure
```

Set `QT_QPA_PLATFORM=offscreen` to run them without a display. Seven suites run:
`urlresolver`, `aboutpages`, `adblocker`, `userextensions`, `startpage`,
`shortcuts` and `singleinstance`. The first two are plain logic; the next two
cover rule matching, filter parsing, script metadata, installation and
enable/disable state; `startpage` checks that start page data is injected without
letting page titles escape it; `shortcuts` checks the platform's key table for
clashes; `singleinstance` checks that a second launch hands its pages over.

## Packaging

Each package is built on its own platform. `./build-all.sh <version>` picks the
right script for the machine it runs on; CI builds all three and, for a `v*` tag,
attaches them to a GitHub release.

| Platform | Command | Output | Needs |
| -------- | ------- | ------ | ----- |
| Linux | `./build-appimage.sh 0.1.0` | `SpacePenguin-0.1.0-x86_64.AppImage` | Qt 6.8 with QtWebEngine; `linuxdeploy` and its Qt plugin are downloaded on first use |
| macOS | `./build-dmg.sh 0.1.0` | `SpacePenguin-0.1.0-macos.dmg` | Xcode command line tools; set `CODESIGN_IDENTITY` to sign with a Developer ID |
| Windows | `powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Version 0.1.0` | `SpacePenguin-0.1.0-windows-x64-setup.exe` and `…-windows-x64.zip` | MSVC 2022 shell, Qt on `PATH`, NSIS 3 for the installer |

See `BUILD_SCRIPTS.md` for what each script does.

## Known limitations

- Bookmarks are a flat list; the backend's folders are not shown
- No page-requested fullscreen (video players cannot go full screen); `F11`
  makes the whole window full screen
- No `file://` support, no popups, no PDF viewer
- No Chrome extension support — QtWebEngine cannot load them, so extensions are
  user scripts instead (`// @name`, `// @match`)
- Blocking covers network rules and cosmetic element hiding, but not
  `$redirect`/`$removeparam` rewrites or `##+js(...)` scriptlets, so a few
  sites still show "ad" placeholders or count blocked requests
- A very large filter list means a large stylesheet injected into every page
- The theme applies to application chrome and internal pages only; website
  colours follow each site's own CSS
- Internal `about:` pages do not create a back/forward history entry
- Session restore saves URLs only — not scroll position or history depth
- `Ctrl+F` uses the renderer's find pass, one tab at a time
- Packages are not code signed. Unsigned Windows installers trigger SmartScreen,
  and an ad-hoc signed macOS app needs right-click › Open the first time;
  `build-dmg.sh` signs and is ready to notarise when given a Developer ID
- No MSIX (Windows), Flatpak or distribution packages (Linux)

## Layout

```
src/browserwindow.*   main window: tab strip, toolbar, omnibox, bookmarks bar, find bar, menus
src/browserpage.*     QWebEnginePage subclass holding the security policy
src/webview.*         per-tab view: load state, links opened as new tabs
src/icons.*           painted icon set that follows the theme
src/preferences.*     QSettings-backed preferences shared by every window
src/shortcuts.*       per-platform keyboard bindings
src/singleinstance.*  hands a second launch's pages to the running browser
src/settingsdialog.*  settings window
src/historydialog.*   history window
src/downloadsdialog.* downloads window
src/bookmarksdialog.* bookmark manager and bookmark editor
src/aboutpages.*      internal about: pages, including the easter eggs
src/profiles.*        profile factory, plus blocking and user-script install
src/theme.*           System / Light / Dark setting, palette and stylesheet
src/adblocker.*       QWebEngineUrlRequestInterceptor, filter and cosmetic parsing
src/cosmeticfilters.* builds the injected element-hiding stylesheet
src/userextensions.*  user script discovery and QWebEngineScript installation
src/extensionsdialog.*  extension manager UI
src/startpageschemehandler.*  serves html/start.html over sp://, with search and top sites
src/urlresolver.*     URL, about: or search resolution (unit tested)
src/main.cpp          profile setup, command line, entry point
html/start.html       built-in start page
filters/default.txt   built-in starter filter list
cmake/                Info.plist and qt.conf templates
packaging/            icons, Linux desktop entry and metadata, Windows resource, macOS icon and entitlements
tests/                unit tests
docs/                 GitHub Pages site
```

## Why

The mainstream browsers are heavy and increasingly hard to audit. The goal is a
browser you can read end to end: a small C++/Qt codebase, no bundled runtime
services, and a security posture you can check in a sitting.

Tracking protection, sync accounts and a full extension ecosystem are separate
problems with separate solutions. Tracking protection is partly here already:
the content blocker can be pointed at an EasyList-format list.

The project site lives in `docs/` and is published with GitHub Pages.

## License

GNU General Public License v3. See [LICENSE](LICENSE).