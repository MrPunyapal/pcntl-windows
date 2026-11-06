# PCNTL compatibility architecture

## Objective

Support the complete applicable PCNTL semantics, not merely the names. PHP 8.5's `ext/pcntl/pcntl.stub.php` declares 34 functions under a mixture of unconditional and OS-specific availability guards.

### Fork is the gating feature

`pcntl_fork()` duplicates the running PHP interpreter and resumes execution immediately after the call in *both* processes, with the same pre-fork PHP heap. `CreateProcessW()` starts a new program image and cannot preserve that contract by itself. Re-running a script is also not equivalent (side effects, external resources, application state, and execution location differ).

A genuine native port must investigate the Zend VM, memory manager, GC, copy-on-write, thread-local state, DLLs, loaded extensions, OPcache/JIT, child initialization, and resource/handle inheritance. Cygwin has a mature, complicated fork emulation, with documented limitations under ASLR and dynamically linked code.

### Signals are another gating feature

Windows console CTRL events are not Unix process signals. They cannot implement the full SIGUSR1/SIGUSR2, SIGSTOP/SIGCONT, SIGCHLD, SIGALRM, signal-mask and queued-signal contracts. Windows console handlers run in system-created threads; executing arbitrary PHP directly in them would be unsafe. A native port would require VM-safe signal scheduling and a precisely specified semantic mapping.

`pcntl_exec()` requires replacement of the current process image. Starting another process and exiting this one changes PID and ownership semantics.

### Scope of the 34-function inventory

OS-specific functions have guarded availability even on Unix: Linux namespaces, BSD `rfork`, Solaris `forkx`, Darwin QoS. Full parity means following upstream's correct availability and semantics on an eligible backend, not inventing behavior for a foreign OS.

## Two research tracks

**Track A: Cygwin-backed POSIX PHP.** Establish a PHP 8.5 build against the Cygwin runtime with upstream `--enable-pcntl` and run upstream `ext/pcntl/tests`. Measure fork reliability under repeated forks, DLL loading, ASLR, and child lifecycle. This is Windows-host support, not native php.net PHP ABI support.

**Track B: Native Windows extension.** Use the PHP SDK to compile `pcntl_windows`. Start with documented API behavior and tests. Add owned child process handles, safe spawning, lifecycle coordination, processor groups/affinity, jobs and console events as **research primitives**. Only promote an implementation to a real `pcntl_*()` function once tested against upstream's behavior.

The first `wait_process` research primitive uses `OpenProcess` by PID; it has a deliberate lifetime/PID reuse race. Real process ownership requires retaining handles from creation time. Windows exit codes are full 32-bit values, unlike the POSIX 8-bit exit code carried in wait-status encoding.

## Windows-hosted POSIX-runtime verification

An independent optional CI workflow downloads a checksum-pinned PHP 8.5 Cygwin runtime from the third-party [swoole/build-static-php v1.18.1](https://github.com/swoole/build-static-php/releases/tag/v1.18.1) release. This is not a trusted native PHP extension or a complete upstream parity claim; it is only a reference test harness for a Windows host with a POSIX runtime. The GitHub release SHA-256 digest is checked before execution. Once verified, the reference test requires real `pcntl` and `posix` modules and exercises fork, waitpid, and SIGUSR1 delivery.

A production-quality track A needs a reproducible source build, provenance, upstream PCNTL PHPT conformance, and repeated fork/signal stress tests. Vendor binaries are never included in this repository.

## Release criteria

- Official Windows builder passes for PHP 8.5 x64 TS and NTS.
- Linux reference suite proves fork continuation, wait-status and signal dispatch.
- All 34 functions stay in coverage tracking, plus conditionally defined constants and types.
- No `ext-pcntl` claim until native or POSIX-runtime semantics are demonstrated.
- New native APIs must include Windows PHPT tests for success, timeout, invalid input, access failures, and shutdown.

## References

- [PHP 8.5 PCNTL stubs](https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.stub.php)
- [Upstream PCNTL implementation](https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.c)
- [POSIX configure requirements](https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/config.m4)
- [Windows PHP builder](https://github.com/php/php-windows-builder)
- [Cygwin fork constraints](https://cygwin.com/cygwin-ug-net/highlights.html)
- [Win32 CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
- [Windows Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
