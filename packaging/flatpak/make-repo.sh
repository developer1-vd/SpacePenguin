#!/usr/bin/env bash
# Builds SpacePenguin's own Flatpak repository from release bundles, signed, ready
# to publish under https://developer1-vd.github.io/SpacePenguin/flatpak/:
#
#   packaging/flatpak/make-repo.sh <output-folder> <gpg-key-id> SpacePenguin-*.flatpak
#
# The key must be in the GPG keyring already (CI imports it from the
# FLATPAK_GPG_PRIVATE_KEY secret). The output holds:
#   repo/                         the OSTree repository Flatpak reads
#   spacepenguin.flatpakrepo      adds the repository:  flatpak remote-add --user --if-not-exists spacepenguin <url>
#   spacepenguin.flatpakref       installs the app, adding the repository and the
#                                 Flathub runtime it needs:  flatpak install --user <url>
#   spacepenguin-repo.gpg         the public key
#
# Only the bundles given are in it: each release replaces the last, so the site
# stays small.
set -euo pipefail

BASE_URL="${SPACEPENGUIN_FLATPAK_URL:-https://developer1-vd.github.io/SpacePenguin/flatpak}"
APP_ID="io.github.developer1_vd.SpacePenguin"

[[ $# -ge 3 ]] || { echo "usage: $0 <output-folder> <gpg-key-id> <bundle>..." >&2; exit 2; }
OUT="$1"; KEY="$2"; shift 2

rm -rf "${OUT}"
mkdir -p "${OUT}"
ostree init --mode=archive-z2 --repo="${OUT}/repo"
for bundle in "$@"; do
    echo "==> Importing ${bundle}"
    flatpak build-import-bundle --gpg-sign="${KEY}" "${OUT}/repo" "${bundle}"
done
flatpak build-update-repo --gpg-sign="${KEY}" --title="SpacePenguin" \
    --default-branch=master --prune "${OUT}/repo"

gpg --export "${KEY}" > "${OUT}/spacepenguin-repo.gpg"
key_base64="$(base64 -w0 "${OUT}/spacepenguin-repo.gpg")"

cat > "${OUT}/spacepenguin.flatpakrepo" <<EOF
[Flatpak Repo]
Title=SpacePenguin
Url=${BASE_URL}/repo/
Homepage=https://developer1-vd.github.io/SpacePenguin/
Comment=The SpacePenguin web browser, from its developers
Icon=https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/packaging/icons/spacepenguin-128.png
GPGKey=${key_base64}
EOF

cat > "${OUT}/spacepenguin.flatpakref" <<EOF
[Flatpak Ref]
Name=${APP_ID}
Branch=master
Title=SpacePenguin
Url=${BASE_URL}/repo/
SuggestRemoteName=spacepenguin
Homepage=https://developer1-vd.github.io/SpacePenguin/
Icon=https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/packaging/icons/spacepenguin-128.png
RuntimeRepo=https://dl.flathub.org/repo/flathub.flatpakrepo
IsRuntime=false
GPGKey=${key_base64}
EOF

echo "==> ${OUT}: $(du -sh "${OUT}" | cut -f1)"
ostree refs --repo="${OUT}/repo"
