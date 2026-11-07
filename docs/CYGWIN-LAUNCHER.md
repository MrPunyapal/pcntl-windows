# Run a real Cygwin PHP interpreter from PowerShell

The Cygwin native PHP experiment builds an *entire separate PHP CLI executable* with PCNTL and requires Cygwin's runtime DLL. Its `pcntl_fork()` implementation uses Cygwin rather than the native `php_pcntl.dll`.

To run a script from a Windows PowerShell terminal after installing or building a Cygwin PHP CLI with PCNTL:

    ./tools/Invoke-CygwinPhp.ps1 -Script ./tests/reference/fork-exec.php

If PHP is built outside the Cygwin install prefix:

    ./tools/Invoke-CygwinPhp.ps1 -Script ./program.php -PhpBinary C:\path\to\cygwin-php.exe

Use `-PhpArguments @('--name', 'two words')` to pass separate arguments. The launcher converts the script path through Cygwin's `cygpath.exe`, prepends Cygwin runtime DLL location to PATH, verifies real PCNTL is loaded, and then runs the script without a shell.

**Verified:** A Cygwin-linked PHP 8.5.14-dev CLI built and passed fork/exec and nested-fork tests on November 7, 2026. Download `cygwin-php85-pcntl-x64` from the [successful CI run](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42371525809), extract `php.exe`, and pass its Windows path with `-PhpBinary`. The artifact expires November 21, 2026, and needs a compatible Cygwin runtime and dependent DLL packages installed separately. The tested interpreter is still experimental.

This tool never enables PCNTL in an already-running official Windows PHP process.

See docs/CYGWIN.md and https://cygwin.com/cygwin-ug-net/highlights.html for Cygwin fork limitations.
