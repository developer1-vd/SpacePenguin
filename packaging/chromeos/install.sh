#!/usr/bin/env bash
# Installs SpacePenguin on a Chromebook, in ChromeOS's Linux environment.
#
# Turn on Linux first: Settings › About ChromeOS › Developers › Linux development
# environment. (On a school-managed Chromebook the school decides whether that
# setting is there; if it is missing or greyed out, only the school's admin can
# allow it.) Then, in the Terminal app:
#
#   bash install.sh                      download the newest version and install it
#   bash install.sh SpacePenguin-*.AppImage   install an AppImage you already have
#   bash install.sh --uninstall          remove it again (keeps your bookmarks)
#
# SpacePenguin then appears in the ChromeOS launcher under "Linux apps".
#
# This is the Linux installer, install.sh at the top of the repository, which
# works the same on any distribution; this copy runs it, from the checkout when
# there is one and otherwise downloaded.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "${here}/../../install.sh" ]]; then
    exec bash "${here}/../../install.sh" "$@"
fi
command -v curl >/dev/null || { echo "error: curl is missing; run: sudo apt install curl" >&2; exit 1; }
installer="$(mktemp)"
trap 'rm -f "${installer}"' EXIT
curl -fsSL --retry 3 -o "${installer}" \
    https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/install.sh
bash "${installer}" "$@"
