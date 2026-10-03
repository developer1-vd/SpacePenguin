# SpacePenguin

A secure, lightweight, and usable browser written in Qt.

**Status: pre-alpha (v0.0.1).** It opens web pages in tabs and gets out of the
way. Everything below is what actually exists in the tree today — no aspirational
checkbox list.

## What it does

- **Tabs** — new, close, drag to reorder, session restored on restart
- **Address bar** — takes URLs and plain-language queries; anything that isn't a
  URL becomes a search on DuckDuckGo
- **Navigation** — back, forward, reload, stop, home
- **Zoom** — per-tab, `Ctrl`+`+` / `Ctrl`+`-` / `Ctrl`+`0`
- **Private mode** — `--private` runs with nothing written to disk
- **Status line** — load progress and a Secure / Not secure indicator

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

| Shortcut            | Action            |
| ------------------- | ----------------- |
| `Ctrl+T` / `Ctrl+W` | New / close tab   |
| `Ctrl+N`            | New window        |
| `Ctrl+L`            | Focus address bar |
| `F5`                | Reload            |
| `Alt+Left`/`Right`  | Back / forward    |
| `Ctrl++` / `Ctrl+-` | Zoom in / out     |
| `Ctrl+0`            | Reset zoom        |

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

- No bookmarks, history UI, downloads UI, or find-in-page
- No fullscreen (request support is disabled until we handle it correctly)
- No `file://` support, no popups, no PDF viewer, no extensions
- No private-window indicator distinct from a normal window
- Session restore saves URLs only — not scroll position or history depth

## Layout

```
src/browserwindow.*   main window: tabs, toolbar, omnibox, status bar
src/browserpage.*     QWebEnginePage subclass holding the security policy
src/startpageschemehandler.*  serves html/start.html over the sp:// scheme
src/urlresolver.*     URL-or-search resolution (unit tested)
src/main.cpp          profile setup, scheme registration, command line
html/start.html       built-in start page
tests/                unit tests
```

## Why

The mainstream browsers are heavy and increasingly hard to audit. The goal is a
browser you can read end to end: a small C++/Qt codebase, no bundled runtime
services, and a security posture you can check in a sitting.

Non-goals: ad blocking, tracking protection, sync accounts, extensions. Those are
separate problems with separate solutions.

## License

BSD 3-Clause. See [LICENSE](LICENSE).