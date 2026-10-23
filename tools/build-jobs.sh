#!/bin/sh
# Prints how many compiler jobs this machine can run at once without running out
# of memory. One SpacePenguin source file takes about 500 MB to compile (the
# QtWebEngine headers are large), so a build that starts one job per CPU — or,
# with `make -j`, one per file — can freeze a 4 or 8 GB laptop or Chromebook.
#
#   cmake --build build --parallel "$(tools/build-jobs.sh)"
#
# Budget: 1 GB of currently available memory per job, at most one per CPU,
# at least one. SPACEPENGUIN_BUILD_JOBS overrides it.
if [ -n "${SPACEPENGUIN_BUILD_JOBS:-}" ]; then
    echo "${SPACEPENGUIN_BUILD_JOBS}"
    exit 0
fi

cpus=$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
mem_mib=""
if [ -r /proc/meminfo ]; then
    mem_mib=$(sed -n 's/^MemAvailable: *\([0-9]*\) kB$/\1/p' /proc/meminfo)
    [ -n "${mem_mib}" ] && mem_mib=$((mem_mib / 1024))
elif command -v sysctl >/dev/null 2>&1 && sysctl -n hw.physmem >/dev/null 2>&1; then
    # FreeBSD, OpenBSD, NetBSD: physical memory; assume half of it is free.
    mem_mib=$(( $(sysctl -n hw.physmem) / 1024 / 1024 / 2 ))
elif command -v sysctl >/dev/null 2>&1 && sysctl -n hw.memsize >/dev/null 2>&1; then
    # macOS reports no "available" figure; assume half the memory is free.
    mem_mib=$(( $(sysctl -n hw.memsize) / 1024 / 1024 / 2 ))
fi

jobs=${cpus}
if [ -n "${mem_mib}" ]; then
    by_memory=$((mem_mib / 1024))
    [ "${by_memory}" -lt "${jobs}" ] && jobs=${by_memory}
fi
[ "${jobs}" -lt 1 ] && jobs=1
echo "${jobs}"
