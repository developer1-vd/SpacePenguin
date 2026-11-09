# Security Policy

SpacePenguin is a web browser: it runs code from every site you visit, keeps your
saved logins and decides what pages may do. Security reports are welcome and are
handled before any other work.

## Supported versions

| Version | Security fixes |
| ------- | -------------- |
| The newest release (1.0.x once 1.0.0 is out) | Yes |
| The newest release candidate, while there is one | Yes, until the release it leads to |
| The nightly build of `main` | Fixed in the next nightly |
| Anything older (0.1.x, 0.1-A, pre-alpha builds) | No: please update |

Pages are drawn by QtWebEngine (Chromium). A flaw in Chromium itself is fixed by
moving to a Qt release that carries the fix; SpacePenguin follows Qt's patch
releases for that.

## Reporting a vulnerability

**Please do not open a public issue for a security problem.**

Report it privately on GitHub: open the repository's **Security** tab and choose
**Report a vulnerability** (or go to
<https://github.com/developer1-vd/SpacePenguin/security/advisories/new>). Only the
maintainers see the report.

Please include:

- what the problem is and what an attacker could do with it;
- the SpacePenguin version (`about:version` or `SpacePenguin --version`), the
  operating system, and how it was installed (AppImage, .deb, installer, APK…);
- steps or a page that shows it. A small proof of concept helps; please don't
  test against other people's accounts or data.

## What happens next

- **Within 3 days:** a reply saying the report arrived.
- **Within 10 days:** whether it is accepted as a vulnerability, how serious it is
  judged to be, and a rough plan.
- **Accepted:** the fix is made in a private advisory, released (a new release, or
  a re-built one for the newest release) and then the advisory is published with
  credit to you, unless you would rather not be named. A fix normally ships within
  30 days; a problem that is being exploited is fixed as soon as possible.
- **Declined:** you get the reasons. If you disagree, reply on the report.

Please keep the details to yourself until the fix is out or 90 days have passed,
whichever is first.

## In scope

The desktop browser, the phone app, the build and release scripts and the
GitHub workflows in this repository: for example saved logins leaking, a website
reaching `sp://` or `about:` pages' APIs, escaping the ad blocker's or the
classroom's rules in a way that harms the user, the Tor windows leaking traffic
outside Tor, or a release artifact that could be tampered with.

Known limits that are documented are not vulnerabilities by themselves, for
example: the phone app's history-free tabs sharing cookies with other tabs, and
the classroom being a beta feature for a trusted school network (see the README).
