#!/usr/bin/env bash
# Writes the Homebrew cask and the winget manifests for a published release, with
# the download addresses and SHA-256 sums GitHub reports for its files.
#
#   tools/update-package-manifests.sh 1.0.0-rc.3
#
# Writes Casks/spacepenguin.rb and packaging/winget/<version>/. Run it after the
# release workflow has published (or re-published) that version, then commit.
# The cask is what people install: copy it to Casks/ in developer1-vd/homebrew-spacepenguin.
# The winget folder is what goes to microsoft/winget-pkgs (see packaging/winget/README.md).
set -euo pipefail

VERSION="${1:?usage: $0 <version, e.g. 1.0.0-rc.3>}"
VERSION="${VERSION#v}"
REPO="developer1-vd/SpacePenguin"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PACKAGE_ID="SpacePenguin.SpacePenguin"

release_json="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/tags/v${VERSION}")"

# The SHA-256 of a release file, from the digest GitHub keeps for every upload.
sha_of() {
    python3 -c '
import json, sys
name = sys.argv[1]
for asset in json.load(sys.stdin)["assets"]:
    if asset["name"] == name and (asset.get("digest") or "").startswith("sha256:"):
        print(asset["digest"][7:])
        sys.exit(0)
sys.exit("not in the release: " + name)' "$1" <<<"${release_json}"
}
release_date="$(python3 -c 'import json,sys; print(json.load(sys.stdin)["published_at"][:10])' <<<"${release_json}")"

dmg="SpacePenguin-${VERSION}-macos.dmg"
setup="SpacePenguin-${VERSION}-windows-x64-setup.exe"
dmg_sha="$(sha_of "${dmg}")"
setup_sha="$(sha_of "${setup}" | tr '[:lower:]' '[:upper:]')"
download="https://github.com/${REPO}/releases/download/v${VERSION}"

mkdir -p "${ROOT}/Casks"
cat >"${ROOT}/Casks/spacepenguin.rb" <<EOF
# Install with:
#   brew tap developer1-vd/spacepenguin
#   brew trust developer1-vd/spacepenguin
#   brew install --cask spacepenguin
# Written by tools/update-package-manifests.sh in developer1-vd/SpacePenguin;
# copied here for each release.
cask "spacepenguin" do
  version "${VERSION}"
  sha256 "${dmg_sha}"

  url "https://github.com/${REPO}/releases/download/v#{version}/SpacePenguin-#{version}-macos.dmg"
  name "SpacePenguin"
  desc "Secure, lightweight browser built on Qt and Chromium"
  homepage "https://github.com/${REPO}"

  # Release candidates count too: GitHub marks them pre-releases, which livecheck
  # skips by default.
  livecheck do
    url :url
    regex(/^v?(\\d+(?:\\.\\d+)+(?:-rc\\.\\d+)?)\$/i)
    strategy :github_releases do |json, regex|
      json.map do |release|
        next if release["draft"]

        match = release["tag_name"]&.match(regex)
        next if match.blank?

        match[1]
      end
    end
  end

  depends_on macos: :monterey

  app "SpacePenguin.app"

  zap trash: [
    "~/Library/Application Support/SpacePenguin",
    "~/Library/Caches/SpacePenguin",
    "~/Library/Preferences/invalid.spacepenguin.SpacePenguin.plist",
    "~/Library/Saved Application State/org.spacepenguin.SpacePenguin.savedState",
  ]

  caveats <<~EOS
    SpacePenguin is not notarised by Apple yet, so macOS stops the first start.
    Open it once, then go to System Settings > Privacy & Security and click
    "Open Anyway".
  EOS
end
EOF

out="${ROOT}/packaging/winget/${VERSION}"
rm -rf "${out}"
mkdir -p "${out}"
schema="# yaml-language-server: \$schema=https://aka.ms/winget-manifest"
cat >"${out}/${PACKAGE_ID}.yaml" <<EOF
${schema}.version.1.6.0.schema.json
PackageIdentifier: ${PACKAGE_ID}
PackageVersion: ${VERSION}
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
EOF
cat >"${out}/${PACKAGE_ID}.installer.yaml" <<EOF
${schema}.installer.1.6.0.schema.json
PackageIdentifier: ${PACKAGE_ID}
PackageVersion: ${VERSION}
InstallerType: nullsoft
UpgradeBehavior: install
ProductCode: SpacePenguin
ReleaseDate: ${release_date}
Commands:
- SpacePenguin
Protocols:
- http
- https
FileExtensions:
- htm
- html
- xhtml
AppsAndFeaturesEntries:
- DisplayName: SpacePenguin
  Publisher: SpacePenguin
  DisplayVersion: ${VERSION}
  ProductCode: SpacePenguin
Installers:
- Architecture: x64
  Scope: user
  InstallerUrl: ${download}/${setup}
  InstallerSha256: ${setup_sha}
  InstallerSwitches:
    Custom: /CurrentUser
- Architecture: x64
  Scope: machine
  InstallerUrl: ${download}/${setup}
  InstallerSha256: ${setup_sha}
  InstallerSwitches:
    Custom: /AllUsers
ManifestType: installer
ManifestVersion: 1.6.0
EOF
cat >"${out}/${PACKAGE_ID}.locale.en-US.yaml" <<EOF
${schema}.defaultLocale.1.6.0.schema.json
PackageIdentifier: ${PACKAGE_ID}
PackageVersion: ${VERSION}
PackageLocale: en-US
Publisher: SpacePenguin
PublisherUrl: https://github.com/developer1-vd
PublisherSupportUrl: https://github.com/${REPO}/issues
PackageName: SpacePenguin
PackageUrl: https://github.com/${REPO}
License: GPL-3.0
LicenseUrl: https://github.com/${REPO}/blob/main/LICENSE
ShortDescription: A secure, lightweight browser built on Qt and Chromium.
Description: SpacePenguin is a fast, private web browser with a built-in ad blocker, tracking protection, HTTPS-only mode and modes for students, teachers, developers and gamers.
Moniker: spacepenguin
Tags:
- browser
- chromium
- privacy
- qt
- web-browser
ReleaseNotesUrl: https://github.com/${REPO}/releases/tag/v${VERSION}
ManifestType: defaultLocale
ManifestVersion: 1.6.0
EOF

echo "Wrote Casks/spacepenguin.rb and packaging/winget/${VERSION}/ for ${VERSION}."
