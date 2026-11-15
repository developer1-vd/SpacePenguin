# Contributing to SpacePenguin

Thanks for your interest in making SpacePenguin better. This document covers
how to build, test, and contribute changes.

## Getting started

Requirements: Qt 6.5 or later (Qt 6.11 is used in development), CMake 3.16+,
a C++20 compiler, and Make.

### Building from source

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(tools/build-jobs.sh)"   # as many jobs as memory allows
```

The binary is `build/SpacePenguin`.

On NixOS or when Qt is provided by nix, use the nix-shell shell:

```sh
nix-shell -p qt6.qtbase qt6.qtdeclarative qt6.qtwebengine cmake gnumake libglvnd \
  --run 'cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build'
```

### Running tests

```sh
cd build && ctest --output-on-failure
```

Four test suites run: `urlresolver`, `aboutpages`, `adblocker`, and
`userextensions`. The first two test pure logic; the last two cover rule
matching, filter parsing, script metadata, installation, and enable/disable
state.

## Project layout

```
src/    — C++ implementation and headers
  browserwindow.*        main window, tabs, toolbar, omnibox, status bar
  browserpage.*          QWebEnginePage subclass with the security policy
  aboutpages.*           internal about: pages, including easter eggs
  profiles.*             profile factory and service wiring
  theme.*                System / Light / Dark appearance setting
  adblocker.*            QWebEngineUrlRequestInterceptor, filter parsing
  cosmeticfilters.*      builds injected element-hiding stylesheets
  userextensions.*       user script discovery and script installation
  extensionsdialog.*     extension manager UI
  settingsdialog.*       settings dialog UI
  downloadmanager.*      download handling
  bookmarkmanager.*      bookmark model and persistence
  cookiemanager.*        cookie management
  datasavermanager.*     data-saver mode handling
  historymanager.*       history recording and storage
  startpageschemehandler.* serves the built-in start page over sp://
  urlresolver.*          URL, about:, and search resolution (unit tested)
  main.cpp                entry point, profile setup, command line
html/start.html          built-in start page (static)
filters/default.txt      starter ad-blocking filter list
cmake/                   Info.plist and qt.conf templates
tests/                   unit tests
docs/                    GitHub Pages site
```

## Coding conventions

- **C++20**. Qt 6 is the only GUI framework. No other GUI dependencies.
- **No comments** unless explicitly requested. Code should be self-documenting.
- **Header hygiene**: `#pragma once`, forward-declare where possible, and keep
  headers small. Implementation details belong in `.cpp` files.
- **Error handling**: fail closed. A broken ad-block rule or a malformed user
  script should not prevent the application from starting or a page from
  loading.
- **Security first**: default to deny. If you add a new permission, capability,
  or URL scheme, document why it is needed and what attack it would fail if it
  were removed.
- **Tests**: new logic should have a test in `tests/`. If you add a new
  `about:` page, add a row to the `knownPagesRender` data-driven test in
  `tests/tst_aboutpages.cpp`.

## How to make a change

1. Fork and clone the repository.
2. Pick an issue from the issue tracker, or open one to discuss what you want
   to build. Small fixes (typos, a broken shortcut, a missing `about:` page)
   can go straight to a pull request.
3. Make your change in a feature branch.
4. Run the existing tests: `ctest --test-dir build`. Add or update tests as
   needed.
5. If your change affects user-facing behavior, update the relevant section of
   `README.md`, and add a line to `CHANGELOG.md` under "Unreleased".
6. Open a pull request against `main`.

## Translating SpacePenguin

The browser's words come from `tr()` in the code; each language is a file in
`translations/`, `spacepenguin_<language>.ts`, built into the app, which
picks the one for the system's language. Qt Linguist (part of Qt's tools)
edits them. To add a language, German for example:

```bash
cmake -B build -DSPACEPENGUIN_NEW_LANGUAGES=de
cmake --build build --target update_translations   # writes translations/spacepenguin_de.ts
linguist translations/spacepenguin_de.ts            # translate, and save
cmake -B build -DSPACEPENGUIN_NEW_LANGUAGES=        # it is found on its own from now on
```

Rebuild and start SpacePenguin with the system in that language
(`LANGUAGE=de ./build/SpacePenguin` on Linux) to see it. To bring every
language up to date after strings change in the code, run the
`update_translations` target again. `spacepenguin_en.ts` holds only English
plural forms ("1 bookmark", "3 bookmarks"); when you add a `tr()` with `%n`,
fill in its two forms there. The built-in pages (`html/`) are not translated
yet.

## Pull request checklist

- [ ] Code builds without warnings (`-Wall -Wextra` are enforced).
- [ ] All tests pass (`ctest --test-dir build`).
- [ ] New logic is covered by a test in `tests/`.
- [ ] `README.md` is updated if the behavior change is user-visible.
- [ ] No secrets, keys, or credentials are introduced.

## Reporting issues

Before opening an issue, check the [Known limitations](README.md#known-limitations)
section in the README to see if the behavior is already documented as expected.

When reporting a bug, include:

- The platform and Qt version.
- Steps to reproduce.
- The expected vs. actual behavior.
- For crashes, run under `gdb` and paste the backtrace.

## License

All contributions are licensed under the GNU General Public License v3. By
submitting a pull request, you agree that your changes may be distributed under
those terms.
