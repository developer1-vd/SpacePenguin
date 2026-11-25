# Install with:
#   brew tap developer1-vd/spacepenguin
#   brew trust developer1-vd/spacepenguin
#   brew install --cask spacepenguin
# Written by tools/update-package-manifests.sh in developer1-vd/SpacePenguin;
# copied here for each release.
cask "spacepenguin" do
  version "1.0.0-rc.7"
  sha256 "cc07a8b057d2b78763154d4142557caf7aa68b68e9674c9bbc30e8005cf044d5"

  url "https://github.com/developer1-vd/SpacePenguin/releases/download/v#{version}/SpacePenguin-#{version}-macos.dmg"
  name "SpacePenguin"
  desc "Secure, lightweight browser built on Qt and Chromium"
  homepage "https://github.com/developer1-vd/SpacePenguin"

  # Release candidates count too: GitHub marks them pre-releases, which livecheck
  # skips by default.
  livecheck do
    url :url
    regex(/^v?(\d+(?:\.\d+)+(?:-rc\.\d+)?)$/i)
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
