# How the code works

A short tour of SpacePenguin's source, for finding your way around. Each file's own
comments go deeper; this says what lives where and how the parts fit.

SpacePenguin is a C++20 / Qt 6.8 desktop browser. Pages are drawn by
**QtWebEngine** (Chromium); everything around them — tabs, toolbar, menus, settings,
blocking — is Qt Widgets code in `src/`. A separate Qt Quick app for phones lives in
`mobile/`.

## The shape of it

```
main.cpp ──► ProfileServices (profiles.cpp) ──► BrowserWindow ──► WebView ──► BrowserPage
   │          one QWebEngineProfile plus its        tabs, toolbar,      a tab      policy for each
   │          managers: ad blocker, history,         menus, prompts                 navigation
   │          bookmarks, downloads, cookies…
   └─► SingleInstance: a second launch hands its addresses to the running process
```

- **One process per profile.** `main.cpp` sets Chromium up, parses the command line
  and, if SpacePenguin is already running, passes the addresses to it
  (`singleinstance.cpp`) and exits.
- **Services, not globals.** `createProfileServices()` builds one browser profile and
  the managers that belong to it. Every window of that profile shares the same
  `ProfileServices`; private windows get an off-the-record set.
- **Preferences** (`preferences.cpp`) are `QSettings` behind getters and setters.
  Every setter emits `changed()`, and every window re-applies them, so a change in
  Settings shows everywhere at once.

## Start-up (`main.cpp`)

1. `Sandbox::prepare()` — on Linux systems that block Chromium's sandbox, fall back
   instead of crashing (`sandbox.cpp`).
2. `DeviceTuning::prepare()` — small machines and Chromebooks get fewer renderer
   processes, and software drawing where there is no GPU (`devicetuning.cpp`).
3. Register the `sp://` scheme (start page, store), create `QApplication`.
4. Parse options: `--private`, `--tor`, `--app=<id>`, `--user-data-dir`, addresses.
5. Hand off to a running instance, or carry on: classroom (`classroom.cpp`), gamer
   mode's Chromium switches (`roles.cpp`), Tor (`tor.cpp`, only with `--tor`).
6. Build the profile, then a `BrowserWindow` — or only an app window for `--app`.

## The window (`browserwindow.cpp`)

The largest file. `BrowserWindow` owns:

- **Chrome:** the tab strip (`tabbar.cpp`, or `sidetabs.cpp` down the side), tab
  groups (`tabgroups.cpp`), the address bar ("omnibox") with completion, the
  bookmarks bar, the find bar, and the title bar it draws itself (`windowframe.cpp`).
- **Actions and menus:** every command is a `QAction` with its shortcut from
  `shortcuts.cpp`, shared by the ☰ menu and the menu bar.
- **Prompts:** the bar above the page that asks questions (site permissions, saving a
  login, a student's request to unblock) — `addPrompt()`.
- **Session:** tabs and their back/forward history are saved a moment after every
  change (`scheduleSessionSave()`) and restored on start. A `session/running` flag
  left set means the last run crashed; the tabs are then restored, or offered back.
- **Roles' tools** (developer tools, Focus mode, citations…) and installing sites as
  apps — see below.

Each tab is a `WebView` (`webview.cpp`) holding a `BrowserPage` (`browserpage.cpp`).
A tab whose renderer crashes shows a page with a Reload button (`WebView::crashedPage`).
**`BrowserPage::acceptNavigationRequest` is the policy point:** it decides about every
page load. In order, it handles:

- the classroom's blocks;
- Focus mode;
- an installed app's scope;
- onion sites outside Tor;
- `about:` pages, which the window renders itself;
- user-script installs.

## Blocking

- `adblocker.cpp` — a `QWebEngineUrlRequestInterceptor` that sees every request.
  EasyList-style rules are parsed into an index keyed by a rare word of each rule, so
  a request is checked against a handful of rules, not tens of thousands. `||host^`
  rules take a faster path. It also strips tracking parameters (`utm_*`, `fbclid`…).
- `filterlists.cpp` — downloads and refreshes the subscribed lists and parses them
  off the main thread.
- `cosmeticfilters.cpp` — element-hiding rules, turned into a script that hides
  matching elements on each page.
- `youtube.cpp`, `youtubescripts.cpp`, `sponsorblock.cpp` — YouTube ads, which share
  hosts with the videos, are removed from the player's data in the page; sponsor
  segments are skipped using SponsorBlock data.

## Pages SpacePenguin draws itself

- `sp://start` and `sp://store` — `html/start.html` and `html/store.html`, served by
  `startpageschemehandler.cpp` and `store.cpp` (the store searches Greasy Fork and
  OpenUserJS for user scripts).
- `about:logins` — `html/logins.html`, served as `sp://logins` by `loginspage.cpp`.
  Its small JSON API answers only requests made from that page itself.
- `about:` pages (`about:history`, `about:version`, `about:mozilla`…) —
  `aboutpages.cpp`.
- Reader view — a script in `readerview.cpp` that redraws the page as its article.

## Data the browser keeps

| File | Keeps |
|---|---|
| `historymanager.cpp` | visited pages, for the omnibox and `about:history` |
| `bookmarkmanager.cpp` | bookmarks and folders (`bookmarksdialog.cpp` edits them) |
| `logins.cpp` | saved logins, encrypted with DPAPI on Windows |
| `importer.cpp` | bringing bookmarks and logins in from Chrome, Edge, Firefox… |
| `downloadmanager.cpp` | downloads (`downloadsdialog.cpp` shows them) |
| `cookiemanager.cpp` | the profile's cookies, listed and removed per site |
| `sitepermissions.cpp` | what each site was allowed (camera, location…) |
| `datasavermanager.cpp` | the data saver mode |

## Extensions

- `userextensions.cpp` — user scripts (Tampermonkey-style), with the `GM_` functions
  they expect, injected per site.
- `chromeextensions.cpp` — Chrome (Manifest V3) extensions through QtWebEngine's
  extension support. Needs Qt 6.10; with an older Qt it reports itself unavailable.
- `extensionsdialog.cpp` — manages both.

## Made for you: roles (`roles.cpp`)

*Settings › Made for you* picks roles; each one switches on its own tools:

- **Developer** — developer tools docked in the window (`openDevTools()` in the
  window), the JavaScript console, Inspect, page source, no cache, a custom user agent.
- **Gamer** — pointer lock without asking; other tabs muted and paused (lifecycle
  *Frozen*) during a full-screen game; GPU, frame-rate and FPS-counter switches passed
  to Chromium at start (`Roles::gamerFlags`).
- **Worker / student** — Focus mode (`Roles::isBlocked`, reminder page), work pages,
  Copy Citation.
- **Teacher and student** — the classroom, below.

### Classroom (`classroom.cpp`)

Teacher and student pair over the local network, with nothing on the internet:

- **Teacher's side:** the teacher's SpacePenguin runs a small HTTP/JSON server
  (`QTcpServer`, port 47800) and answers UDP broadcasts on port 47801 to its
  six-character class code.
- **Joining:** a student broadcasts the code to find the teacher, then polls
  `GET /v1/class` every 15 s. The reply carries the blocked sites, each with the
  teacher's reason, plus this student's exceptions and answers.
- **Requests:** a student's request to unblock a site, or to leave, is a
  `POST /v1/request`. It appears as a prompt in the teacher's window.
- **Enforcement:** `Classroom::blocks()` is called from `BrowserPage` for every page
  and frame. It also unwraps view-source, translation proxies and archives, knows a
  site's other addresses (youtu.be…), and blocks web proxies. While a student is in a
  class, Tor and leaving are locked too.

## Private, Tor and apps

- **Private windows** — an off-the-record profile; nothing is written to disk.
- **Tor windows** (`tor.cpp`) — Chromium allows one proxy per process, so Tor windows
  run in their own process (`--tor`). It finds a running Tor (9050, 9150) or starts the
  `tor` program, then routes everything through it as SOCKS5.
- **Installed apps** (`webapps.cpp`, `appwindow.cpp`) — *Install as App* reads the
  site's web app manifest and writes a launcher for the platform: a `.desktop` file on
  Linux, a Start-menu `.lnk` on Windows, an `.app` in ~/Applications on macOS. The
  launcher runs `--app=<id>`, which opens an `AppWindow`: the site alone, with
  out-of-scope links sent to the browser.

## Look and feel

- `theme.cpp` — one set of colours drives both the widget palette and the stylesheet,
  in light and dark, with several accent palettes.
- `systemscheme.cpp` — reads the desktop's dark-mode setting through the XDG portal
  (needed on GNOME).
- `icons.cpp` — every icon is painted in code, so it follows the theme at any size.
- `notifications.cpp` — site notifications, on the desktop or inside the window.
- `updatechecker.cpp` — the opt-in "new version" check against GitHub releases.
- `startuptrace.cpp` — `SPACEPENGUIN_TRACE_STARTUP=1` prints start-up timings.

## The phone app (`mobile/`)

A Qt Quick (QML) app on QtWebView, for Android and iOS. `browserbackend.cpp` gives
the QML (`mobile/qml/`) the start page, history, bookmarks, session restore and
SponsorBlock. It shares `youtubescripts.cpp` and `sponsorblock.cpp` with the desktop.

## Tests (`tests/`)

Qt Test, one executable per area; run them with `ctest --test-dir build`.

| Test | Covers |
|---|---|
| `tst_adblocker` | rule parsing and matching |
| `tst_urlresolver` | turning typed text into an address or a search |
| `tst_classroom` | runs a whole class over 127.0.0.1: join, block, ask, allow, leave |
| `tst_roles` | Focus mode, citations, manifests, search templates, onions, gamer switches |
| `tst_importer` | bookmark and login import; only about:logins may use its API |
| `tst_userextensions` | user-script installation and injection |
| `tst_aboutpages` | the `about:` pages |
| `tst_startpage` | the start page |
| `tst_shortcuts` | per-platform key bindings |
| `tst_singleinstance` | handing addresses to the running process |
| `tst_sponsorblock` | SponsorBlock lookups |
| `tst_devicetuning` | start-up tuning for small machines |

## Building and shipping

- `CMakeLists.txt` — the app, the tests, packaging (CPack .deb/.rpm), and the version.
  The version is `VERSION` in numbers, plus `SPACEPENGUIN_VERSION_NAME` for what people
  see (0.1-A).
- `install-deps.sh` / `.ps1` — install Qt and the build tools.
- `build-*.sh` / `build-windows.ps1` — the AppImage, DMG, Windows installer
  (`installer.nsi`), distribution packages, Android APK and iPhone .ipa.
- `install.sh` / `uninstall.sh` — the Linux installer: downloads the AppImage and adds
  it to the app menu.
- `packaging/` — desktop entry, icons, Flatpak manifest, Arch PKGBUILD, macOS and
  Windows bits, the Chromebook installer.
- `flake.nix`, `default.nix` — Nix builds.
- `.github/workflows/build.yml` — CI:
  - builds and tests on Windows, macOS, Linux distributions, Nix, Flatpak, Android
    and iOS, and FreeBSD in a VM;
  - publishes the nightly build from main;
  - publishes releases, from a `v*` tag or a manual run given a tag and a name.
- `docs/` — the website.
