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
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig"
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
