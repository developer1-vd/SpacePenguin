#!/usr/bin/env bash
# Build native Linux packages against the distribution's own Qt:
#
#   ./build-linux-packages.sh [deb|rpm|tgz|auto]...
#
# auto (the default) builds a .deb where dpkg-deb exists and an .rpm where
# rpmbuild exists. The distribution must ship Qt 6.8+ with QtWebEngine (Debian 13,
# Ubuntu 25.04, Fedora 41, openSUSE Tumbleweed and newer); `./install-deps.sh
# --qt system` installs it. For distributions with an older Qt, use the AppImage
# (build-appimage.sh) or the Flatpak (packaging/flatpak), which carry their own.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT}"

BUILD_DIR="${BUILD_DIR:-build-linux-packages}"
OUTPUT_DIR="${OUTPUT_DIR:-dist}"
FORMATS=("$@")
[[ ${#FORMATS[@]} -eq 0 ]] && FORMATS=(auto)

die() { echo "error: $*" >&2; exit 1; }
step() { printf '\n==> %s\n' "$*"; }

[[ "$(uname -s)" == Linux ]] || die "native Linux packages are built on Linux"

GENERATORS=()
for format in "${FORMATS[@]}"; do
    case "${format}" in
        deb) GENERATORS+=(DEB) ;;
        rpm) GENERATORS+=(RPM) ;;
        tgz) GENERATORS+=(TGZ) ;;
        auto)
            command -v dpkg-deb >/dev/null && GENERATORS+=(DEB)
            command -v rpmbuild >/dev/null && GENERATORS+=(RPM)
            ;;
        *) die "unknown format ${format} (use deb, rpm, tgz or auto)" ;;
    esac
done
[[ ${#GENERATORS[@]} -gt 0 ]] || die "neither dpkg-deb nor rpmbuild is installed; install one or pass tgz"
command -v dpkg-shlibdeps >/dev/null || [[ ! " ${GENERATORS[*]} " =~ " DEB " ]] ||
    die "dpkg-shlibdeps is missing (apt install dpkg-dev)"

step "Configuring against the system Qt"
# A package must use the distribution's Qt, so ignore .qt/ and CMAKE_PREFIX_PATH.
rm -rf "${BUILD_DIR}"
env -u CMAKE_PREFIX_PATH cmake -S . -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSPACEPENGUIN_INSTALL_QT_CONF=OFF \
    -DSPACEPENGUIN_BUILD_TESTS=OFF \
    -DSPACEPENGUIN_USE_LOCAL_QT=OFF

QT_DIR="$(sed -n 's/^Qt6_DIR:PATH=//p' "${BUILD_DIR}/CMakeCache.txt")"
case "${QT_DIR}" in
    /usr/*) echo "    Qt from ${QT_DIR}" ;;
    *) die "Qt was found at ${QT_DIR}, not in the system; a native package needs the distribution's Qt (./install-deps.sh --qt system)" ;;
esac

step "Building"
cmake --build "${BUILD_DIR}" --parallel

mkdir -p "${OUTPUT_DIR}"
for generator in "${GENERATORS[@]}"; do
    step "Packaging ${generator}"
    (cd "${BUILD_DIR}" && cpack -G "${generator}" -B "${ROOT}/${OUTPUT_DIR}/_cpack")
done

# Keep the packages, drop CPack's working files.
find "${OUTPUT_DIR}/_cpack" -maxdepth 1 -type f \( -name '*.deb' -o -name '*.rpm' -o -name '*.tar.gz' \) \
    -exec mv {} "${OUTPUT_DIR}/" \;
rm -rf "${OUTPUT_DIR}/_cpack"

step "Packages"
for package in "${OUTPUT_DIR}"/spacepenguin*; do
    case "${package}" in
        *.deb) dpkg-deb --field "${package}" Package Version Architecture Depends ;;
        *.rpm) rpm -qp --requires "${package}" | sed 's/^/    requires /' | head -n 20 ;;
    esac
    (cd "$(dirname "${package}")" && sha256sum "$(basename "${package}")") >> "${OUTPUT_DIR}/SHA256SUMS"
    echo "    ${package}"
done
