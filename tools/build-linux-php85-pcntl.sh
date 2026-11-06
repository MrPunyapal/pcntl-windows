#!/usr/bin/env bash
set -euo pipefail

# Separate Linux PHP build with a PCNTL pidfd_open configuration check.
repo="$(cd "$(dirname "$0")/.." && pwd)"
source="$repo/upstream/php-src"
if [[ $# -gt 0 ]]; then source="$1"; fi
source="$(cd "$source" && pwd)"
cd "$source"

python3 - <<'PY'
from pathlib import Path
path = Path('ext/pcntl/config.m4')
original = path.read_text()
needle = "    getpriority\n"
if original.count(needle) != 1:
    raise SystemExit('Unexpected PHP-8.5 config.m4: getpriority anchor absent')
if '    pidfd_open\n' in original:
    print('pidfd_open feature detection is already present')
else:
    path.write_text(original.replace(needle, needle + "    pidfd_open\n"))
    print('Restored pidfd_open detection for PHP-8.5 PCNTL')
PY

./buildconf --force
./configure --disable-all --enable-cli --disable-cgi --disable-phpdbg \
    --enable-pcntl --without-pear
make -j2

binary="$source/sapi/cli/php"
test -x "$binary"
"$binary" -n -v
"$binary" -n "$repo/tools/posix-capabilities.php"
"$binary" -n -r '
if (!extension_loaded("pcntl") || !function_exists("pcntl_setns")) {
    fwrite(STDERR, "pcntl_setns unavailable after restored pidfd_open check\n");
    exit(1);
}
echo "PHP 8.5 pcntl_setns is available\n";
'
"$binary" -n "$repo/tests/reference/fork-exec.php"
if [ "${PCNTL_TEST_NAMESPACE:-0}" = "1" ]; then
    "$binary" -n "$repo/tests/reference/linux-namespace-conformance.php"
fi
