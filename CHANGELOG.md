# Changelog

What changed in each SpacePenguin release, newest first. Security fixes are
marked **Security**; see [SECURITY.md](SECURITY.md) for which releases get them.
The format follows [Keep a Changelog](https://keepachangelog.com/), and versions
follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- AES-256 encryption: Settings › Privacy & Security can export your saved logins and settings (bookmarks included) as an encrypted backup (AES-256-GCM, with a key from PBKDF2-SHA256 at 600,000 rounds) and restore one on another computer. The work is done by Chromium's WebCrypto, so SpacePenguin carries no cryptography of its own.
- A primary password for saved logins: once set, they cannot be seen, filled in or exported until it is given, once per start. Only a PBKDF2 verifier of it is kept.
- A VPN switch in the toolbar: everything SpacePenguin loads, its own updates and filter lists included, goes through Tor (free, no account) or through a SOCKS5 or HTTP server of your VPN service or your own (Settings › Privacy & Security). Names are resolved by the proxy and WebRTC cannot go around it. If the VPN cannot start, SpacePenguin connects to nothing until you choose. Changing it restarts SpacePenguin with the same tabs.
- A translator: Translate Page (Ctrl+Shift+Y, View menu) and Translate Selection (right-click), with Google Translate, Bing Translator, DeepL, Yandex Translate or a LibreTranslate server of your choosing, which translates the page in place. SpacePenguin offers to translate pages that are not in your languages (Settings › General turns that off). Nothing is sent until you ask; LibreTranslate is not used from Tor windows, where it would not go through Tor.
- Languages in Settings › General: SpacePenguin's own language (and so how dates and numbers are written), the languages websites are asked for, in order (Accept-Language and navigator.languages), and the spelling dictionaries, instead of only the system's. Any language and region can be added.
- Read Aloud (text to speech): View › Read Aloud or Ctrl+Alt+S reads the selected text, or else the page's article, in the page's language, with the system's own voices: macOS speech, Windows speech, or speech-dispatcher or eSpeak on Linux. Nothing leaves the computer. Right-click selected text to read just that; the speed is in Settings › General.
- Picture-in-Picture: a page's video plays in a small window that stays over the others (View › Picture-in-Picture, Ctrl+Alt+P, or right-click a video). The tab keeps its place and the video keeps playing; Escape, closing the window or *Bring It Back* returns it.
- Mouse gestures: hold the right mouse button and draw on a page. ← back, → forward, ↑ to the top, ↓ to the bottom, ↑↓ reload, ↓← new tab, ↓→ close tab, ↑← reopen the closed tab. A right click without drawing still opens the menu. Settings › General turns them off.
- A Homebrew cask for macOS (`brew install --cask spacepenguin` from the SpacePenguin tap) and winget manifests for Windows, both written for each release by `tools/update-package-manifests.sh`.

### Fixed

- SpacePenguin could crash on the way out: a window that was still open, or only scheduled to close, could keep its pages past the browser profile they belong to. All windows now go before the profile.
- On Linux, playing a video (YouTube, or a tab reopened at start-up) could still close the browser, Gamer mode or not: Chromium handed decoded frames to QtWebEngine in a form it cannot take on Linux. Videos are now decoded without the GPU on Linux.

## [1.0.0-rc.3] - 2026-11-16

The third release candidate for 1.0.

### Added

- Yandex, Amazon and eBay as search engines, in Settings › General and the menu's Search With.

### Changed

- The new-tab button sits right after the last tab instead of at the far end of the tab row.

### Fixed

- Videos (YouTube and others) no longer crash the browser on Linux with Gamer mode's GPU boost on: it no longer turns on zero-copy there, which Chromium cannot do on Linux.
- The ad blocker could block the page you opened, such as a DuckDuckGo search, which then showed ERR_ACCESS_DENIED. It misread which kind of request each one was, so rules meant for scripts or images could match the page itself. Now a page is only blocked by a rule for the whole site or one marked $document.
- The AppImage could crash the first time it checked a site's certificate on distributions with a different NSS version than the build machine, such as Fedora. It now uses the system's NSS instead of bundling part of it.
- The Linux install script installed 0.1-A instead of the newest release candidate, and then failed to add the menu entry. It now installs the newest release, release candidates included, and finds the menu entry under either app name.

## [1.0.0-rc.2] - 2026-11-15

The second release candidate for 1.0.

### Added
- **Printing**: Print… (Ctrl+P) and Save Page as PDF…, in the menu and the File menu.
- **HTTPS-only mode** (Settings › Privacy & Security, off by default): plain http
  sites are loaded over https, with a warning before one that has no secure
  version. Addresses on your own computer or network are left alone.
- **Spell checking** in text boxes, in your own languages, with suggestions on
  right-click. The AppImage now carries English dictionaries.
- **Screen reader names** for every button and list, so screen readers no longer
  announce icon buttons as just "button".
- **Translations**: SpacePenguin can now be translated; see CONTRIBUTING.md.
- A version number in the settings, so later releases can change how settings are
  stored without breaking existing profiles.

### Fixed
- Messages with a count read correctly in English ("1 bookmark", not "1 bookmark(s)").
- Starting SpacePenguin with an address no longer leaves an empty tab beside it.
- History no longer counts each restored tab as a new visit on every start.
- Duplicate opens the copy next to the original, in its tab group, with its history.
- The website's Windows and macOS download rows are laid out properly again.

## [1.0.0-rc.1] - 2026-11-15

The first release candidate for 1.0.

### Added
- Saved logins kept in the system's keyring: the Secret Service on Linux and
  the BSDs, and the macOS Keychain.
- A Flatpak for x86-64 and ARM64, and SpacePenguin's own Flatpak repository,
  so `flatpak update` keeps it current.
- Classroom mode's messages between the teacher's and students' browsers are
  signed with the class code, and a recorded one cannot be replayed.
- SECURITY.md, explaining how to report a vulnerability.

### Fixed
- Typing an address with a port and a path (`localhost:3000/app`) opens it
  instead of a blank page; local addresses open over http.
- No crash when quitting with Quit.

### Security
- Websites can no longer show SpacePenguin's own pages, saved logins among them,
  in a frame.
- The store's settings can only be changed by the store page itself.
- A login is saved and filled in only for the site it belongs to.
- Only a class's blocked page can ask the teacher to unblock a site.

## [0.1-A] - 2026-11-01

The first public build: ad and tracker blocking (YouTube's too), SponsorBlock,
private and Tor windows, side tabs and tab groups, reader view, user scripts and
their store, sites installed as apps, saved logins (about:logins), crash
recovery, roles for developers, gamers, students, workers and teachers, and
packages for Windows, macOS, Linux, the BSDs, Chromebooks, Android and iPhone.

[Unreleased]: https://github.com/developer1-vd/SpacePenguin/compare/v1.0.0-rc.3...HEAD
[1.0.0-rc.3]: https://github.com/developer1-vd/SpacePenguin/compare/v1.0.0-rc.2...v1.0.0-rc.3
[1.0.0-rc.2]: https://github.com/developer1-vd/SpacePenguin/compare/v1.0.0-rc.1...v1.0.0-rc.2
[1.0.0-rc.1]: https://github.com/developer1-vd/SpacePenguin/compare/v0.1-A...v1.0.0-rc.1
[0.1-A]: https://github.com/developer1-vd/SpacePenguin/releases/tag/v0.1-A
