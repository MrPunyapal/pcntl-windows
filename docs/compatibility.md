# PHP 8.5 PCNTL feature inventory

Source: [upstream `ext/pcntl/pcntl.stub.php`](https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.stub.php). **34 functions**, some conditional by OS. Every line below is tracked whether or not that function is available on the current platform. This does **not** mean all 34 are available on a single Unix system.

Current native implementation: **zero PCNTL-compatible functions**. Only distinct experimental `pcntl_windows_*()` primitives are compiled. This avoids misleading Composer/runtime feature detection.

## Process creation (3)

- `pcntl_fork`: Cygwin POSIX backend candidate; native PHP-state clone is unresolved
- `pcntl_forkx`: Solaris-specific, conditional upstream
- `pcntl_rfork`: BSD-specific, conditional upstream

## Wait operations (3)

- `pcntl_waitpid`: POSIX backend candidate; native handle ownership and POSIX wait status unresolved
- `pcntl_wait`: POSIX backend candidate; Windows has no POSIX wait-any-child operation
- `pcntl_waitid`: conditional upstream, includes signal info and status features

## Signals and alarms (8)

- `pcntl_signal`: POSIX backend candidate; native signal delivery and Zend dispatch unresolved
- `pcntl_signal_get_handler`: requires real signal registration
- `pcntl_signal_dispatch`: requires queued delivery and PHP-safe dispatch
- `pcntl_sigprocmask`: Windows lacks the POSIX signal-mask contract
- `pcntl_sigwaitinfo`: Windows lacks POSIX queued signal metadata
- `pcntl_sigtimedwait`: Windows lacks POSIX queued signal metadata and timing semantics
- `pcntl_async_signals`: requires signal scheduling integrated with the Zend VM
- `pcntl_alarm`: Windows timers are not equivalent to SIGALRM delivery

## Wait status helpers (7)

- `pcntl_wifexited`: blocked on a well-defined compatible wait-status encoding
- `pcntl_wifstopped`: depends on actual stop events
- `pcntl_wifcontinued`: conditional upstream; depends on real continue events
- `pcntl_wifsignaled`: depends on real signal termination
- `pcntl_wexitstatus`: depends on POSIX encoded exit status
- `pcntl_wtermsig`: depends on real signal termination
- `pcntl_wstopsig`: depends on real stop events

## Execution (1)

- `pcntl_exec`: native process-image replacement semantics unresolved

## Scheduling, priority, affinity and QoS (7)

- `pcntl_getpriority`: Windows priorities are not Unix nice values
- `pcntl_setpriority`: Windows priorities are not Unix nice values
- `pcntl_getcpuaffinity`: Windows processor groups and >64 logical processors need proper mapping
- `pcntl_setcpuaffinity`: Windows processor groups and >64 logical processors need proper mapping
- `pcntl_getcpu`: experimental Win32 group/index primitive exists, not compatible yet
- `pcntl_getqos_class`: Darwin-specific, conditional upstream
- `pcntl_setqos_class`: Darwin-specific, conditional upstream

## Namespaces (2)

- `pcntl_unshare`: Linux namespaces, not available on native Windows
- `pcntl_setns`: Linux namespaces, not available on native Windows

## Errors (3)

- `pcntl_get_last_error`: Win32 error codes and Unix errno are different domains
- `pcntl_errno`: alias for `pcntl_get_last_error`
- `pcntl_strerror`: must follow POSIX error-code mapping

## Experimental Windows primitives (not part of the 34)

- `pcntl_windows_wait_process`: uses `OpenProcess`, `WaitForSingleObject`, `GetExitCodeProcess`
- `pcntl_windows_processor`: uses `GetCurrentProcessorNumberEx`

## Machine-readable upstream comparisons

Both Linux PHP and Cygwin-linked Windows PHP run [tests/reference/surface.php](../tests/reference/surface.php) to audit all 34 function names against the actual installed extension. This test fails when any of the 18 unconditional API entries is missing and records presence of the 16 OS-conditional functions, without pretending that every one should exist on every platform. It also reports the extension's constants count. Actual behavior still requires dedicated tests.

## Constants and types

Upstream has conditionally defined POSIX signal, priority, wait-status, namespace and platform-specific error constants, plus Darwin's `Pcntl\\QosClass` enum. These are also part of the compatibility scope. A later generated inventory will track constants/type availability by reference OS. Windows must not advertise constants for operations with incorrect semantics.
