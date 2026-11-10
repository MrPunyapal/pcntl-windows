# Cygwin PHP 8.5 with real PCNTL on Windows

## Verified result: November 8, 2026

A separate **Cygwin-linked PHP 8.5.14-dev CLI** built successfully on a Windows GitHub Actions runner. It passed **real `pcntl_fork()` and `pcntl_exec()` tests**, including PID and process-image identity, nested forks, PHP heap isolation, inherited temporary streams, bidirectional Unix socket-pair communication, and wait statuses.

- [Passing CI run](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42477101712)
- [Compiled `cygwin-php85-pcntl-x64` artifact](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42477101712/artifacts/13617142152)
- [Source build script](../tools/build-cygwin-php.sh) and [CI workflow](../.github/workflows/cygwin-reference.yml)

The artifact contains **`php.exe` and its SHA256 checksum**, not the Cygwin runtime or shared library dependencies. It expires **November 22, 2026** under the workflow's 14-day retention policy. Rebuild after expiration.

## What works and what does not

This binary links against Cygwin's POSIX runtime and is **not ABI-compatible** with standard MSVC-built Windows PHP or its DLL extensions. It does not change any already running native Windows PHP interpreter.

The conformance job reported **27/34** conditionally defined PHP 8.5 PCNTL function names. It includes `pcntl_fork` and `pcntl_exec`. It does **not** export `pcntl_waitid`, `pcntl_unshare`, `pcntl_rfork`, `pcntl_forkx`, `pcntl_setns`, `pcntl_getqos_class`, or `pcntl_setqos_class`.

Cygwin's fork mechanism can still fail with DLL relocation, ASLR or runtime/thread state. These focused tests do **not** certify arbitrary network services, extension combinations, OPcache, threaded workloads, or production use. Linux namespaces require a Linux PHP runtime such as [WSL2](WSL.md).

## Run the compiled PHP

1. Install compatible x64 Cygwin and all DLL packages required by the compiled executable.
2. Download and extract `php.exe` from the verified artifact, or compile PHP yourself using the checked-in workflow.
3. Use the [Cygwin PHP launcher](CYGWIN-LAUNCHER.md) to select this interpreter:

```powershell
./tools/Invoke-CygwinPhp.ps1 -Script ./tests/reference/fork-exec.php -PhpBinary C:\path\to\php.exe
./tools/Invoke-CygwinPhp.ps1 -Script ./tests/reference/fork-matrix.php -PhpBinary C:\path\to\php.exe
./tools/Invoke-CygwinPhp.ps1 -Script ./tests/reference/fork-socket-pair.php -PhpBinary C:\path\to\php.exe
```

The launcher converts script paths with `cygpath.exe`, checks that PCNTL fork is present, and runs PHP without a shell. The build artifact is **not** a standalone Windows PHP distribution.

## Reproducing the build

PHP is configured with `--enable-pcntl --disable-fiber-asm` under Cygwin. A Cygwin-native `re2c` must generate the lexer because the Windows-native generator cannot read paths prefixed with `/cygdrive/`. CI caches its Cygwin-native re2c build. Source downloads come from Debian's upstream mirror, with GitHub and Raspbian fallbacks, and must match the pinned SHA256 before the downloaded build scripts execute. An external mirror outage therefore does not silently permit unverified toolchain input. The workflow uploads `php.exe` and a `php.exe.sha256` checksum **only if** the fork/exec, nested fork/resource tests and a Windows PowerShell launcher smoke test pass. This describes newer CI artifacts; the older linked successful artifact above contains only `php.exe`.

Read [Cygwin's documented fork limitations](https://cygwin.com/cygwin-ug-net/highlights.html) before relying on this build. The separate native Windows DLL still exports **28/34** PCNTL names with documented Win32 limitations.

## Additional conformance gates

The Cygwin and Linux reference workflows also run `fork-exec-environment.php` (PID, working directory, explicit environment, whitespace and empty arguments) and `fork-many-children.php` (12 children, `WNOHANG`, exit status and `ECHILD` after reaping). These were first run under Linux PHP 8.4; Cygwin results must be checked on the corresponding commit before claiming Cygwin conformance. All scripts run both directly in the Cygwin build and through the Windows PowerShell launcher.
