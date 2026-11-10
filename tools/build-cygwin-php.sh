#!/usr/bin/env bash
set -euo pipefail

# This builds a separate Cygwin-linked PHP binary with upstream PCNTL.
# It cannot be loaded as a DLL into Microsoft Visual C++ PHP builds.
if [[ -z "$GITHUB_WORKSPACE" ]]; then
    echo "Missing checkout path" >&2
    exit 2
fi
workspace="$(cygpath -u "$GITHUB_WORKSPACE")"
source="$workspace/upstream/php-src"
cd "$source"

# Cygwin's configure and Makefile pass POSIX /cygdrive/... paths to re2c.
# Never put the native Windows Chocolatey re2c.exe on PATH: it cannot read
# those paths and fails while generating ext/json/json_scanner.c.
export PATH="/usr/local/bin:/usr/bin:/bin"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig"
# Build a Cygwin-linked re2c from the pinned source release if absent.
if ! command -v re2c >/dev/null 2>&1; then
    tempdir="$(mktemp -d)"
    trap 'rm -rf "$tempdir"' EXIT
    # GitHub release downloads occasionally return HTTP 504. Debian mirrors
    # the byte-identical upstream 4.6 tarball (Debian re2c_4.6.orig.tar.xz).
    # Check the upstream SHA256 before running any downloaded build scripts.
    re2c_sha256="75bf2696445e831d0d44e0d9f2909eeffc18c09757f222b2fb025f7e59fe130b"
    re2c_archive="$tempdir/re2c.tar.xz"
    re2c_source=""
    for url in \
        "https://deb.debian.org/debian/pool/main/r/re2c/re2c_4.6.orig.tar.xz" \
        "https://github.com/skvadrik/re2c/releases/download/4.6/re2c-4.6.tar.xz" \
        "https://archive.raspbian.org/raspbian/pool/main/r/re2c/re2c_4.6.orig.tar.xz"; do
        if curl --fail --location --retry 3 --retry-delay 2 \
                --connect-timeout 20 --silent --show-error \
                "$url" -o "$re2c_archive" \
            && printf '%s  %s\n' "$re2c_sha256" "$re2c_archive" | sha256sum --check --status; then
            re2c_source="$url"
            break
        fi
        echo "re2c download failed or did not match pinned SHA256: $url" >&2
    done
    if [[ -z "$re2c_source" ]]; then
        echo "Unable to fetch verified re2c 4.6 source from any mirror" >&2
        exit 1
    fi
    echo "Verified re2c source: $re2c_source"
    tar -xJf "$re2c_archive" -C "$tempdir"
    (
        cd "$tempdir/re2c-4.6"
        ./configure --disable-docs --prefix=/usr/local
        make -j2
        make install
    )
    re2c --version
fi
re2c_path="$(command -v re2c)"
case "$re2c_path" in
    /usr/local/bin/*|/usr/bin/*) ;;
    *) echo "Refusing to use non-Cygwin re2c: $re2c_path" >&2; exit 1 ;;
esac
printf 're2c executable: %s\n' "$re2c_path"
re2c --version

cd "$source"
./buildconf --force
./configure \
    --disable-all \
    --enable-cli \
    --disable-cgi \
    --disable-phpdbg \
    --enable-pcntl \
    --enable-posix \
    --disable-fiber-asm \
    --without-pear

make -j2

cd "$workspace"
php_binary="$source/sapi/cli/php.exe"
test -x "$php_binary"
"$php_binary" -n -v
"$php_binary" -n tools/posix-capabilities.php
"$php_binary" -n -r 'if (!extension_loaded("pcntl") || !function_exists("pcntl_fork")) exit(1);'
"$php_binary" -n tests/reference/fork-exec.php
"$php_binary" -n tests/reference/fork-matrix.php
"$php_binary" -n tests/reference/fork-socket-pair.php
"$php_binary" -n tests/reference/fork-exec-environment.php
"$php_binary" -n tests/reference/fork-many-children.php
"$php_binary" -n tests/reference/process-lifecycle.php

# Keep exact source/toolchain and runtime dependency evidence beside the binary.
# This is provenance, not a bundle of Cygwin DLLs or a standalone installation.
{
    printf 'PHP source commit: '
    git -C "$source" rev-parse HEAD
    printf 'Extension checkout commit: '
    git -C "$workspace" rev-parse HEAD
    "$php_binary" -n -v
    gcc --version
    re2c --version
    printf '\nRuntime DLL dependencies:\n'
    cygcheck "$php_binary"
    printf '\nInstalled Cygwin packages:\n'
    cygcheck -c
} > "$source/sapi/cli/build-info.txt"
