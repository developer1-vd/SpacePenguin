#!/usr/bin/env bash
# Let SpacePenguin use Chromium's sandbox on Ubuntu 23.10+ and other systems that
# restrict unprivileged user namespaces with AppArmor:
#
#   sudo ./packaging/linux/install-apparmor-profile.sh            # this checkout, the
#                                                                 # AppImage, /usr/local
#   sudo ./packaging/linux/install-apparmor-profile.sh <program>  # specific binaries
#   sudo ./packaging/linux/install-apparmor-profile.sh --remove
#
# Writes /etc/apparmor.d/spacepenguin-local with one profile per program that grants
# it `userns` and nothing else, then loads it. Restart SpacePenguin afterwards.
# /usr/bin/SpacePenguin is left to the .deb and .rpm packages, whose own profile
# (/etc/apparmor.d/spacepenguin) covers it; two profiles may not claim one program.
set -euo pipefail

PROFILE=/etc/apparmor.d/spacepenguin-local
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

die() { echo "error: $*" >&2; exit 1; }

[[ $(id -u) -eq 0 ]] || die "run this with sudo"

if [[ "${1:-}" == --remove ]]; then
    if [[ -f "${PROFILE}" ]]; then
        apparmor_parser -R "${PROFILE}" 2>/dev/null || true
        rm -f "${PROFILE}"
        echo "Removed ${PROFILE}."
    fi
    exit 0
fi

command -v apparmor_parser >/dev/null || die "AppArmor is not installed, so no profile is needed"
restricted=0
if [[ -r /proc/sys/kernel/apparmor_restrict_unprivileged_userns ]] &&
   [[ "$(cat /proc/sys/kernel/apparmor_restrict_unprivileged_userns)" == 1 ]]; then
    restricted=1
fi
if [[ ${restricted} -eq 0 ]]; then
    echo "This system does not restrict user namespaces with AppArmor; Chromium's sandbox"
    echo "works without a profile. Installing one anyway, in case that changes."
fi

PROGRAMS=()
if [[ $# -gt 0 ]]; then
    for program in "$@"; do
        [[ -e "${program}" ]] || die "${program} does not exist"
        PROGRAMS+=("$(realpath "${program}")")
    done
else
    # This checkout's builds and the Qt that install-deps put in .qt/.
    shopt -s nullglob
    for program in "${ROOT}"/build*/SpacePenguin "${ROOT}"/build*/dist/bin/SpacePenguin \
                   "${ROOT}"/.qt/*/*/libexec/QtWebEngineProcess; do
        PROGRAMS+=("$(realpath "${program}")")
    done
    shopt -u nullglob
    # Hand-installed copies and AppImages, wherever their runtime unpacks them.
    PROGRAMS+=(/usr/local/bin/SpacePenguin
               "/tmp/.mount_SpaceP*/usr/bin/SpacePenguin"
               "/tmp/.mount_SpaceP*/usr/libexec/QtWebEngineProcess"
               "/tmp/appimage_extracted_*/usr/bin/SpacePenguin"
               "/tmp/appimage_extracted_*/usr/libexec/QtWebEngineProcess")
fi

tmp="$(mktemp)"
trap 'rm -f "${tmp}"' EXIT
{
    echo "# Written by SpacePenguin's install-apparmor-profile.sh on $(date -u +%Y-%m-%d)."
    echo "# Grants Chromium's sandbox the unprivileged user namespaces it needs; see"
    echo "# packaging/linux/apparmor/spacepenguin.in in the SpacePenguin repository."
    echo "abi <abi/4.0>,"
    echo "include <tunables/global>"
    index=0
    for program in "${PROGRAMS[@]}"; do
        [[ "${program}" == *'"'* ]] && die "cannot use a path containing a quote: ${program}"
        index=$((index + 1))
        echo
        echo "profile spacepenguin-local-${index} \"${program}\" flags=(unconfined) {"
        echo "  userns,"
        echo "  include if exists <local/spacepenguin>"
        echo "}"
    done
} > "${tmp}"

# Check it parses before replacing anything.
apparmor_parser -Q -K "${tmp}" >/dev/null || die "AppArmor rejected the profile (needs AppArmor 4)"
install -m 0644 "${tmp}" "${PROFILE}"
apparmor_parser -r "${PROFILE}"

echo "Installed ${PROFILE} for:"
printf '    %s\n' "${PROGRAMS[@]}"
echo "Restart SpacePenguin; about:version should now say \"Sandbox: On\"."
