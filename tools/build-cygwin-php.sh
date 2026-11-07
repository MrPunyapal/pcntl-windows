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
    curl -fL --retry 3 --silent --show-error \
        https://github.com/skvadrik/re2c/releases/download/4.6/re2c-4.6.tar.xz \
        -o "$tempdir/re2c.tar.xz"
    tar -xJf "$tempdir/re2c.tar.xz" -C "$tempdir"
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
