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
menu bar if you turn it on); `F1` opens the same list from inside the app, so it
cannot drift from the code.

| Shortcut                | Action                |
| ----------------------- | --------------------- |
| `Ctrl+T`                | New tab               |
| `Ctrl+W`                | Close tab             |
| `Ctrl+Shift+T`          | Reopen closed tab     |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab  |
| `Ctrl+1` … `Ctrl+8`     | Go to tab 1–8         |
| `Ctrl+9`                | Go to the last tab    |
| `Ctrl+N`                | New window            |
| `Ctrl+Shift+N`          | New private window    |
| `Ctrl+Shift+W`          | Close window          |
| `Ctrl+Q`                | Quit                  |
| `Alt+Left` / `Alt+Right`| Back / forward        |
| `F5`                    | Reload                |
| `Esc`                   | Stop loading (while a load is in flight) |
| `Alt+Home`              | Home                  |
| `Ctrl+L`                | Focus address bar     |
| `Ctrl+F`                | Find in page          |
| `Ctrl++` / `Ctrl+-`     | Zoom in / out         |
| `Ctrl+0`                | Reset zoom            |
| `F11`                   | Full screen           |
| `Ctrl+D`                | Bookmark this page    |
| `Ctrl+Shift+B`          | Show bookmarks bar    |
| `Ctrl+Shift+O`          | Bookmarks             |
| `Ctrl+H`                | History               |
| `Ctrl+J`                | Downloads             |
| `Ctrl+Shift+E`          | Extensions            |
| `Ctrl+,`                | Settings              |
| `F1`                    | Keyboard shortcuts    |

`Ctrl` is `Cmd` on macOS. We keep the bindings identical on every platform
rather than following each desktop's local convention, with two deliberate
exceptions: macOS gets standard application menu roles for Quit and About, and
`Cmd+W` closes the tab rather than the window.

## Platforms

| Platform | Status | Notes |
| -------- | ------ | ----- |
| Linux (X11, Wayland) | Built and verified | `cmake --install` ships a generated `qt.conf` |
| Windows | Builds from the same CMake project | MSVC `/W4`, GUI subsystem so no console window |
| macOS | Builds as `SpacePenguin.app` | `Info.plist` declares the `http`/`https` handler |

Platform integration handled in the build:

- `cmake/qt.conf.in` is configured from the detected Qt layout, so an installed
  Linux or Windows copy finds `QtWebEngineProcess`, plugins and translations
  without environment variables
- `cmake/Info.plist.in` sets the bundle identifier, high-resolution support and
  automatic graphics switching, and advertises the app as a web-page handler
- Windows builds set `WIN32_EXECUTABLE`, so no console window appears behind the
  browser
- `about:version` reports the running Qt platform plugin (`xcb`, `wayland`,
  `windows`, `cocoa`)

Platform data locations come from `QStandardPaths`, so the profile lands in
`~/.local/share/SpacePenguin` on Linux, `~/Library/Application Support` on
macOS, and `%LOCALAPPDATA%` on Windows.

## Building

Requires Qt 6.8 or newer with QtWebEngine, and a C++20 compiler.

```sh
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.2/gcc_64
cmake --build build
./build/spacepenguin
```

On Nix:

```sh
nix-shell -p 'qt6.qtbase' 'qt6.qtdeclarative' 'qt6.qtwebengine' cmake gnumake libglvnd --run \
  'cmake -B build && cmake --build build && ./build/spacepenguin'
```

On macOS the output is an app bundle, so run
`open build/SpacePenguin.app`; to ship it, deploy the frameworks with
`macdeployqt build/SpacePenguin.app` before code signing.

Run the binary from the build tree rather than installing it — QtWebEngine finds
its helper process and resources relative to the executable. An install
(`cmake --install build --prefix <dir>`) brings the generated `qt.conf` along, so
the helper process is still found from the new location.

### Options

```
--private               do not write history, cookies, or cache for this session
--user-data-dir <dir>   store persistent data in <dir>
--filter-list <file>    load ad blocking rules from <file>
[urls...]               open each address or search in a new tab
```

### Tests

```sh
ctest --test-dir build --output-on-failure
```

## Packaging

### Linux AppImage

```bash
chmod +x build-appimage.sh
./build-appimage.sh 0.1.0
```

Requires: `linuxdeployqt`, `appimagetool`, Qt6 development packages.

Output: `SpacePenguin-0.1.0-x86_64.AppImage`

### macOS DMG

```bash
chmod +x build-dmg.sh
./build-dmg.sh 0.1.0
```

Must run on macOS. Requires: Xcode command line tools.

Output: `SpacePenguin-0.1.0-macos.dmg`

### Windows Installer (NSIS)

```bash
makensis installer.nsi
```

Requires: NSIS 3.x, Qt6 Windows binaries.

Output: `SpacePenguin-0.1.0-windows-x64.exe`

### Build All Platforms

```bash
chmod +x build-all.sh
./build-all.sh 0.1.0
```

Or build for specific platform:

```bash
./build-all.sh 0.1.0 linux
./build-all.sh 0.1.0 macos
./build-all.sh 0.1.0 windows
```

### CI/CD

See `.github/workflows/build.yml` for GitHub Actions workflow that builds all three formats and creates a GitHub Release on tag push.

### Files

- `build-appimage.sh` — Linux AppImage builder
- `build-dmg.sh` — macOS DMG builder  
- `installer.nsi` — Windows NSIS installer script
- `build-all.sh` — Wrapper to build all platforms
- `BUILD_SCRIPTS.md` — Detailed build documentation

Five suites run: `urlresolver`, `aboutpages`, `adblocker`, `userextensions` and
`startpage`. The first two are plain logic; the next two cover rule matching,
filter parsing, script metadata, installation and enable/disable state; the last
checks that start page data is injected without letting page titles escape it.

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
- Only Linux is verified end to end; the Windows and macOS build paths are
  written but untested on real hardware
- No code signing, notarisation (macOS), MSIX packaging (Windows) or desktop
  entry / icon assets

## Layout

```
src/browserwindow.*   main window: tab strip, toolbar, omnibox, bookmarks bar, find bar, menus
src/browserpage.*     QWebEnginePage subclass holding the security policy
src/webview.*         per-tab view: load state, links opened as new tabs
src/icons.*           painted icon set that follows the theme
src/preferences.*     QSettings-backed preferences shared by every window
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
cmake/                Info.plist and qt.conf templates for macOS and desktop
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