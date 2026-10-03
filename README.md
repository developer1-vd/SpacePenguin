# SpacePenguin

A secure, lightweight, and usable browser written in Qt.

## Status

Early prototype. The repository currently holds the project charter and build
ignores; the browser itself has not landed yet. The first milestone is a
single window that opens a URL, with tabs and history following after.

## What we're aiming for

SpacePenguin exists because the mainstream browsers are heavy and increasingly
hard to audit. The goal is a browser you can actually read end to end:

- **Auditable** — a small C++/Qt codebase with no build-time code generation you
  can't trace back to a source file.
- **Lightweight** — Qt Widgets and QtWebEngine, no bundled runtime services,
  no telemetry, no background updater.
- **Secure by default** — no remote content unless the user asked for it, least
  privilege for web-facing APIs, and a security posture you can read in a
  sitting.
- **Usable** — tabs, history, bookmarks, downloads and keyboard navigation done
  properly, not as an afterthought.

Non-goals: ad blocking, tracking protection, sync accounts, and extensions.
Those are separate problems with separate solutions.

## Requirements

- Qt 6.5 or newer with **QtWebEngine** (`qtwebengine6-dev` / `qtwebengine`)
- A C++20 compiler (GCC 13+, Clang 16+, or MSVC 2022)
- CMake 3.21+ *or* qmake from the same Qt installation

## Building

No build files are committed yet. Once they are:

```sh
# CMake (preferred)
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.2/gcc_64
cmake --build build
./build/spacepenguin

# or qmake
qmake6 && make && ./spacepenguin
```

Run the binary from the build directory rather than installing it; QtWebEngine
locates its bundled resources relative to the executable and a prefix install
needs an explicit `qt.conf`.

## Layout

```
src/        browser sources (window, tab, omnibox, profile)
src/ui/     Qt Designer forms and resources
tests/      unit and UI tests
```

## License

BSD 3-Clause. See [LICENSE](LICENSE).