#!/usr/bin/env bash
# Prints the CHANGELOG.md section for one version, for its release on GitHub:
#
#   tools/release-notes.sh v1.0.0 [CHANGELOG.md]
#
# A version with no section of its own gets the Unreleased one, so a release made
# before its section was written still says what is new. Prints nothing if
# neither is there.
set -euo pipefail
version="${1#v}"
changelog="${2:-CHANGELOG.md}"
[[ -f "${changelog}" ]] || exit 0
section() {
    awk -v want="$1" '
        /^## \[/ { inside = (index($0, "## [" want "]") == 1); next }
        /^\[[^]]+\]: / { next }
        inside { print }
    ' "${changelog}" | sed -e '/./,$!d' | sed -e ':a' -e '/^\n*$/{$d;N;ba' -e '}'
}
notes="$(section "${version}")"
[[ -n "${notes}" ]] || notes="$(section Unreleased)"
printf '%s\n' "${notes}"
