#!/usr/bin/env bash
# Removes SpacePenguin installed with install.sh. In a terminal:
#
#   curl -fsSL https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/uninstall.sh | bash
#
# or, with the script downloaded:
#
#   bash uninstall.sh            remove it, and ask whether to delete your data too
#   bash uninstall.sh --purge    remove it and your bookmarks, history, logins and settings
#   bash uninstall.sh --yes      remove it and keep your data, without asking
#
# It is install.sh --uninstall: from the checkout when there is one, downloaded
# otherwise. A .deb, .rpm, Flatpak or Arch package is removed with its package
# manager instead; on Windows use Settings › Apps, and on macOS drag SpacePenguin
# from Applications to the Bin.
set -euo pipefail

# Run as a file: the installer beside it. Piped into bash there is no file, and
# whatever install.sh sits in the current folder is not to be trusted.
if [[ -n "${BASH_SOURCE[0]:-}" && -f "${BASH_SOURCE[0]}" ]]; then
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    if [[ -f "${here}/install.sh" ]]; then
        exec bash "${here}/install.sh" --uninstall "$@"
    fi
fi
command -v curl >/dev/null || { echo "error: curl is missing" >&2; exit 1; }
installer="$(mktemp)"
trap 'rm -f "${installer}"' EXIT
curl -fsSL --retry 3 -o "${installer}" \
    https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/install.sh
bash "${installer}" --uninstall "$@"
