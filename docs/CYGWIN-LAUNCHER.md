# Run a real Cygwin PHP interpreter from PowerShell

The Cygwin native PHP experiment builds an *entire separate PHP CLI executable* with PCNTL and requires Cygwin's runtime DLL. Its `pcntl_fork()` implementation uses Cygwin rather than the native `php_pcntl.dll`.

To run a script from a Windows PowerShell terminal after installing or building a Cygwin PHP CLI with PCNTL:

    ./tools/Invoke-CygwinPhp.ps1 -Script ./tests/reference/fork-exec.php

If PHP is built outside the Cygwin install prefix:

    ./tools/Invoke-CygwinPhp.ps1 -Script ./program.php -PhpBinary C:\path\to\cygwin-php.exe

Use `-PhpArguments @('--name', 'two words')` to pass separate arguments. The launcher converts the script path through Cygwin's `cygpath.exe`, prepends Cygwin runtime DLL location to PATH, verifies real PCNTL is loaded, and then runs the script without a shell.

The launcher is useful only if Cygwin PHP itself builds and passes the fork/exec conformance tests; PHP 8.5 compatibility is still experimental. This tool never modifies or enables PCNTL in a running official Windows PHP process.

See docs/CYGWIN.md and https://cygwin.com/cygwin-ug-net/highlights.html for Cygwin fork limitations.
