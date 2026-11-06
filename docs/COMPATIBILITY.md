# Full PCNTL compatibility research and implementation plan

**Target:** the entire conditional PHP 8.5 upstream `ext/pcntl/pcntl.stub.php` API, including platform-specific operations. Different Unix platforms expose different subsets, so 'all features' means investigating every declared function and platform branch, **not** claiming identical semantics on every operating system.

**Terminology:** `PoC` = implemented in the current Windows prototype, `Candidate` = needs an implementation and validation, `Research` = no demonstrated faithful native Win32 implementation. Do not expose a canonical PCNTL name with intentionally incorrect semantics.

## Process lifecycle

- `pcntl_fork`: **Research, highest priority.** Investigate Cygwin-style copying of address space and rebuilding runtime state, and a purpose-built checkpoint/restart of Zend VM and PHP runtime. Windows `CreateProcessW` alone cannot duplicate the interpreter's live C/PHP stack, open handles, closures, user objects, extension state, and control flow. A re-exec/replay mechanism is NOT compatible with arbitrary fork calls. Explore a separate POSIX runtime (WSL2/Cygwin) for real fork semantics, and test compatibility with *official native Windows PHP* separately. Avoid exporting `pcntl_fork` until proven by conformance tests.
- `pcntl_exec`: **Research.** Windows starts a new process with `CreateProcessW`; true process image replacement and preserving the PID/handle are not provided by the supported public Win32 process API. Investigate controlled supervisor hand-off and `ExitProcess`, but label this as approximate and test observability/PID guarantees.
- `pcntl_waitpid`: **PoC** for registered child PIDs and `-1` (any), `WNOHANG`. Extend to process groups, stop/continue, arbitrary children, rusage, EINTR behavior, and more than 64 waits. Investigate Job Objects + I/O completion ports or threadpool waits.
- `pcntl_wait`: **PoC**, same caveats as waitpid.
- `pcntl_waitid`: **Candidate.** Use handles plus job notifications for `P_PID`/`P_ALL`, inspect `WNOWAIT`, `WEXITED`; process groups and `siginfo_t` details need explicit investigation.
- `pcntl_forkx`: **Research.** Solaris-specific fork variant. Inspect individual `FORK_*` flag semantics, not just generic spawning.
- `pcntl_rfork`: **Research.** BSD-specific flags covering child resource sharing and namespaces; implement each flag only if actual semantics can be guaranteed.

## Signals and timers

- `pcntl_signal`: **Candidate.** Console control handlers support a small subset (`CTRL_C_EVENT`, `CTRL_BREAK_EVENT`) and have thread-context constraints. Build a queued delivery bridge into Zend VM; distinguish graceful app-defined events from real POSIX signals.
- `pcntl_signal_get_handler`: **Candidate.** Track callable/default/ignore handlers per request with correct lifetime and ZTS handling.
- `pcntl_signal_dispatch`: **Candidate.** Explicitly dispatch queued userland handlers on the PHP execution thread.
- `pcntl_async_signals`: **Candidate.** Integrate dispatch with Zend VM interrupt mechanism; never run PHP callbacks from Windows console-control threads or timer callbacks.
- `pcntl_alarm`: **Candidate.** Waitable timers / threadpool timer enqueue SIGALRM-style events; handler dispatch must happen on the PHP thread.
- `pcntl_sigprocmask`: **Research.** POSIX signal masks are per-thread. Emulating only extension-managed events is a limited subset and must be labeled as such.
- `pcntl_sigwaitinfo`, `pcntl_sigtimedwait`: **Research.** Consume queued emulated events with origin/time metadata where available; POSIX kernel-delivered signal fidelity needs a separate runtime.

## Status and error functions

- `pcntl_wifexited`, `pcntl_wexitstatus`: **PoC**, parse our POSIX-style low-byte encoding of Win32 exit codes.
- `pcntl_wifstopped`, `pcntl_wifsignaled`, `pcntl_wstopsig`, `pcntl_wtermsig`: **PoC** status decoders, but Win32 exit events currently do not produce POSIX stop/termination signals; validate against future event encodings.
- `pcntl_wifcontinued`: **Candidate.** Could decode an explicit synthetic continued state; Windows does not automatically generate POSIX `SIGCONT` events.
- `pcntl_get_last_error`, `pcntl_errno`, `pcntl_strerror`: **PoC** with CRT `errno`-style codes; expand error mapping using Win32 `GetLastError` and `FormatMessageW` where needed.

## Scheduling and machine topology

- `pcntl_getpriority`, `pcntl_setpriority`: **Candidate.** Map priority classes (`GetPriorityClass`/`SetPriorityClass`) and possibly thread priority. Windows priority classes are not POSIX nice values; full mapping is not one-to-one.
- `pcntl_getcpuaffinity`, `pcntl_setcpuaffinity`: **Candidate.** `GetProcessAffinityMask`/`SetProcessAffinityMask` are limited by processor groups; investigate CPU Sets and multi-group support.
- `pcntl_getcpu`: **Candidate.** `GetCurrentProcessorNumberEx` (processor group and number), mapped carefully onto available CPU numbering.
- `pcntl_getqos_class`, `pcntl_setqos_class`: **Research.** macOS-specific QoS. Investigate Windows scheduling, EcoQoS/power throttling and process/thread priority, but do not mislabel them as Apple QoS guarantees.

## Namespaces and OS-specific isolation

- `pcntl_unshare`: **Research.** Linux `CLONE_NEW*` namespaces have no general-purpose Win32 equivalent. Evaluate AppContainer, Job Objects, Windows silos, Windows containers/Host Compute Service, and WSL2 Linux runtime per flag. Mark each supported only if resource isolation and security guarantees match the requested flag.
- `pcntl_setns`: **Research.** Linux namespace membership is defined by the kernel; entering an existing Linux namespace is possible inside WSL2 Linux PHP, not for an arbitrary native Windows PHP process via Win32 calls.

## Required research gates before claiming compatibility

1. Verify each function against the upstream PHP stub and runtime tests for PHP 8.4 and 8.5; track conditional constants/enums (`W*`, `SIG*`, `P_*`, `CLONE_*`, `FORK_*`, `RF*`, `PRIO_*`, `PCNTL_E*`, QoS).
2. For `fork`, test continuation at exactly the same instruction, distinct PIDs, child return 0, heap copy-on-write semantics, nested forks, resource/descriptor inheritance, loaded extensions, ZTS/NTS, opcache, cleanup and exception paths. If any fail, do not claim `pcntl_fork` support.
3. For signals, test handler callbacks on the PHP main thread, delivery ordering, nested dispatch, timers, cancellation, console vs GUI processes, and masked delivery.
4. For wait semantics, test multiple generations of children, `WNOHANG`, group PIDs, `WNOWAIT`, exit vs signaled/stopped states and high exit codes; validate resource usage and error codes.
5. Run native Windows CLI CI across supported PHP/architecture/thread-safety combinations and a Linux POSIX reference test job. Any Windows-only approximation gets a separate opt-in capability flag and precise documentation.

## Primary references

- [Official PHP PCNTL reference](https://www.php.net/manual/en/book.pcntl.php)
- [Upstream conditional API declarations](https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.stub.php)
- [Upstream POSIX feature probes](https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/config.m4)
- [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
- [WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)
- [Windows PHP extension builder](https://github.com/php/php-windows-builder)
