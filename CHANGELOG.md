# Changelog

What changed in each SpacePenguin release, newest first. Security fixes are
marked **Security**; see [SECURITY.md](SECURITY.md) for which releases get them.
The format follows [Keep a Changelog](https://keepachangelog.com/), and versions
follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

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

[Unreleased]: https://github.com/developer1-vd/SpacePenguin/compare/v1.0.0-rc.1...HEAD
[1.0.0-rc.1]: https://github.com/developer1-vd/SpacePenguin/compare/v0.1-A...v1.0.0-rc.1
[0.1-A]: https://github.com/developer1-vd/SpacePenguin/releases/tag/v0.1-A
