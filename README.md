# SpacePenguin

A secure, lightweight, and usable browser written in Qt.

**Status: pre-alpha (v0.0.1).** It opens web pages in tabs and gets out of the
way. Everything below is what actually exists in the tree today — no aspirational
checkbox list.

## What it does

- **Tabs** — new, close, drag to reorder, reopen closed, session restored on restart
- **Address bar** — takes URLs, `about:` pages and plain-language queries; anything
  that isn't a URL becomes a search on DuckDuckGo
- **Navigation** — back, forward, reload, stop, home
- **Find in page** — `Ctrl+F`, with match count and next/previous
- **Zoom** — per-tab, `Ctrl`+`+` / `Ctrl`+`-` / `Ctrl`+`0`
- **Windows** — new window, new private window (`Ctrl`+`Shift`+`N`); private
  windows never write a session, cookies or cache to disk
- **Status line** — load progress plus Secure / Not secure / Bundled page /
  Internal page indicator

## Internal pages

Type any of these in the address bar. They are rendered by the browser itself,
never fetched from the network.

| Page            | What it shows                                              |
| --------------- | ---------------------------------------------------------- |
| `about:`        | Index of every internal page                               |
| `about:about`   | The same index, with descriptions                          |
| `about:version` | Version, build type, Qt version, renderer, profile mode    |
| `about:license` | SpacePenguin and Qt licensing                              |
| `about:blank`   | Handled by the rendering engine                            |
| `about:penguin` | 🐧                                                        |
| `about:teapot`  | RFC 2324                                                   |
| `about:pan`     | You have been panned                                       |

Anything else — `about:nonsense` — gets an internal "no such page" answer
instead of a network error.

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
- No telemetry, no background services, no auto-updater, no remote
  configuration. The browser does not phone home.

## Keyboard

Every binding below is implemented and wired to a visible menu entry; `F1`
opens the same list from inside the app, so it cannot drift from the code.

| Shortcut                | Action                |
| ----------------------- | --------------------- |
| `Ctrl+T`                | New tab               |
| `Ctrl+W`                | Close tab             |
| `Ctrl+Shift+T`          | Reopen closed tab     |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab  |
| `Ctrl+1` … `Ctrl+9`     | Go to tab 1–9         |
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
| `Ctrl+J`                | About                 |
| `F1`                    | Keyboard shortcuts    |

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

Run the binary from the build tree rather than installing it — QtWebEngine finds
its helper process and resources relative to the executable. If you do install
it, add a `qt.conf` that points `Prefix` at the Qt `libexec` and `lib`
directories.

### Options

```
--private               do not write history, cookies, or cache for this session
--user-data-dir <dir>   store persistent data in <dir>
```

### Tests

```sh
ctest --test-dir build --output-on-failure
```

## Known limitations

- No bookmarks, history UI, or downloads UI
- No fullscreen (request support is disabled until we handle it correctly)
- No `file://` support, no popups, no PDF viewer, no extensions
- Internal `about:` pages do not create a back/forward history entry
- Session restore saves URLs only — not scroll position or history depth
- `Ctrl+F` uses the renderer's find pass; clearing the box clears highlighting
  only for the page that was open when the search started

## Layout

```
src/browserwindow.*   main window: tabs, toolbar, find bar, omnibox, status bar
src/browserpage.*     QWebEnginePage subclass holding the security policy
src/aboutpages.*      internal about: pages, including the easter eggs
src/profiles.*        persistent vs. private profile factory
src/startpageschemehandler.*  serves html/start.html over the sp:// scheme
src/urlresolver.*     URL, about: or search resolution (unit tested)
src/main.cpp          profile setup, command line, entry point
html/start.html       built-in start page
tests/                unit tests
```

## Why

The mainstream browsers are heavy and increasingly hard to audit. The goal is a
browser you can read end to end: a small C++/Qt codebase, no bundled runtime
services, and a security posture you can check in a sitting.

Non-goals: ad blocking, tracking protection, sync accounts, extensions. Those are
separate problems with separate solutions.

## Note
Keyboard shortcuts are currently not supposed to work.
Currently, it only supports Linux/MacOS right now.
Use WSL to run on Windows for now.

## License

BSD 3-Clause. See [LICENSE](LICENSE).