# Cygwin PHP 8.5 with real PCNTL on Windows

## Verified fork/exec baseline: November 11, 2026

A separate **Cygwin-linked PHP 8.5.14-dev CLI** built successfully on a Windows GitHub Actions runner. It passed **real `pcntl_fork()` and `pcntl_exec()` tests**, including PID and process-image identity, nested forks, PHP heap isolation, inherited temporary streams, bidirectional Unix socket-pair communication, and wait statuses.

- [Passing CI run](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42874490177)
- [Compiled `cygwin-php85-pcntl-x64` artifact](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42874490177/artifacts/13792457814)
- [Source build script](../tools/build-cygwin-php.sh) and [CI workflow](../.github/workflows/cygwin-reference.yml)

The artifact contains **`php.exe` and its SHA256 checksum**, not the Cygwin runtime or shared library dependencies. Its recorded expiry is **2026-11-24T18:46:57Z** under the workflow's 14-day retention policy. Rebuild after expiration. The downloaded binary's SHA256 was independently checked as `edcc0d9456a66669a974af77b52a6735993e0652dc774ee194ff93e2f668e3d9`.

**Expanded signal conformance is failing.** [Run 42876175564](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42876175564)
passed the fork/exec/resource/socket/environment/reaping probes, then failed
`process-lifecycle.php` at async signal delivery after `sigtimedwait`. The workflow
correctly withheld the verified artifact. Do not infer signal/timer/group
conformance from the earlier passing fork baseline. Failed binaries are now
retained only as explicitly unsupported diagnostic artifacts.

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

`process-lifecycle.php` adds kernel signal masks, manual and async dispatch,
`sigtimedwait` and unchanged output on timeout, alarm cancellation/delivery,
`SIGCHLD`, real `setpgid`, negative-group `waitpid`, and double-reap `ECHILD`.
It exercises `waitid(WNOWAIT)` only when exported and explicitly prints a skip
otherwise. Child handshakes and waits are bounded; failures kill and reap tracked
children. The Cygwin build now enables upstream POSIX as well as PCNTL for these
checks. A passing Linux result alone does not establish Cygwin conformance.

New builds pin the PHP source commit and include `build-info.txt` with PHP and
extension commits, compiler/generator versions, resolved DLL dependencies and
installed Cygwin packages. Cygwin packages still come from a rolling mirror;
the manifest records that remaining reproducibility limit. The artifact remains
dependent on a compatible Cygwin installation.

The passing baseline above also verified `fork-exec-environment.php` (PID, working directory, explicit environment, whitespace and empty arguments) and `fork-many-children.php` (12 children, `WNOHANG`, exit status and `ECHILD` after reaping), both directly and through the Windows PowerShell launcher. The newly added combined lifecycle test remains failing; downstream group and SIGCHLD checks have not yet been reached under Cygwin.
