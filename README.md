# PCNTL for Windows

Research toward supporting **the complete applicable PHP PCNTL API** on Windows, with real behavior rather than function-name shims.

## Important distinction

This project is **not yet a PCNTL port**. The first proof of concept builds a native Windows extension named `pcntl_windows` to prove the PHP Windows build and test environment and experiment with Win32 process primitives. It does not advertise `ext-pcntl` or export any fake `pcntl_*()` functions.

The target includes fork, signals, signal masks, alarm, process waiting, `exec`, status helpers, priorities, affinity, namespaces, and conditional operating-system features. See [the complete 34-function inventory](docs/compatibility.md).

We are researching two approaches:

- **Windows host with a POSIX runtime:** Build PHP against Cygwin and compile upstream `ext/pcntl`. This offers the closest match for fork/signals, but is **not** a DLL for official native Windows PHP.
- **Native Windows PHP:** Investigate Windows API process control and, if necessary, changes to the Zend engine. A new `CreateProcessW()` process is not `pcntl_fork()`, and console-control events are not POSIX signals.

## Current experimental functions

- `pcntl_windows_wait_process(int $pid, int $timeout_ms = -1): int|false|null`: wait for an accessible Windows process. It returns the **Windows 32-bit exit code**, `null` on timeout, or `false` on failure. Not equivalent to `pcntl_waitpid()`: no own-child requirement, no POSIX wait status, and possible PID reuse between lookup and open.
- `pcntl_windows_processor(): array`: get the current Windows processor group and group-local number.

The extension deliberately exposes neither `pcntl_fork` nor the `pcntl` module name. These primitives are experimental building blocks, not completion of the PCNTL requirements.

## Build

The [CI workflow](.github/workflows/ci.yml) uses the [official PHP Windows extension builder](https://github.com/php/php-windows-builder) to compile and test this extension against PHP 8.5 on Windows x64 TS and NTS, using the configuration flag:

```text
--enable-pcntl-windows
```

On Windows, a locally compiled DLL must match the PHP version, architecture, and TS/NTS ABI. Use the [PHP Windows SDK](https://github.com/php/php-sdk-binary-tools). PHPT tests live in `tests/`. A separate Linux test establishes upstream fork and signal behavior as the compatibility reference.

## Project rules

- Track **all** declared PCNTL functions and applicable supporting constants/types.
- Never claim function-name parity instead of behavioral parity.
- Any future exported `pcntl_*()` function must have cross-platform behavior tests.
- Never silently substitute `proc_open`, threads, or PHP script replay for POSIX `fork()`.
- Keep Linux, Solaris, FreeBSD, and Darwin-only APIs explicitly labeled. Even upstream PCNTL does not expose all those APIs on the same platform.
- Do not publish a Windows `php_pcntl.dll` until the fundamental fork/signal/exec behavior is proven.

More detail: [architecture](docs/architecture.md) and [full API coverage](docs/compatibility.md).

**Status:** experimental, no release or compatibility claim.
