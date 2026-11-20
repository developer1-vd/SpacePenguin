# Install with:
#   brew tap developer1-vd/spacepenguin https://github.com/developer1-vd/SpacePenguin
#   brew install --cask spacepenguin
# Written by tools/update-package-manifests.sh; run it again for a new release.
cask "spacepenguin" do
  version "1.0.0-rc.5"
  sha256 "3b83f42ecfb0bc057f4cafd6fe28f760474b329c93c1c61ad664cf198a4d96e2"

  url "https://github.com/developer1-vd/SpacePenguin/releases/download/v#{version}/SpacePenguin-#{version}-macos.dmg"
  name "SpacePenguin"
  desc "Secure, lightweight browser built on Qt and Chromium"
  homepage "https://github.com/developer1-vd/SpacePenguin"

  depends_on macos: ">= :monterey"

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
