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

export PATH="/usr/bin:/bin:$PATH"
# The Chocolatey re2c executable is standalone and works as a lexer tool
# from Cygwin. Keep the verified source-build path as a fallback.
if [ -x /cygdrive/c/ProgramData/chocolatey/bin/re2c.exe ]; then
    export PATH="/cygdrive/c/ProgramData/chocolatey/bin:$PATH"
fi
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig"
# Cygwin's package index does not currently include re2c. Build a pinned
# upstream release instead of bypassing PHP's configure requirements.
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

cd "$source"
./buildconf --force
./configure \
    --disable-all \
    --enable-cli \
    --disable-cgi \
    --disable-phpdbg \
    --enable-pcntl \
    --without-pear

make -j2

cd "$workspace"
php_binary="$source/sapi/cli/php.exe"
test -x "$php_binary"
"$php_binary" -n -v
"$php_binary" -n tools/posix-capabilities.php
"$php_binary" -n -r 'if (!extension_loaded("pcntl") || !function_exists("pcntl_fork")) exit(1);'
"$php_binary" -n tests/reference/fork-exec.php
