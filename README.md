# pcntl-windows

Experimental native Windows implementation of PHP process-control APIs, tested with PHP 8.4 and 8.5. This is NOT a drop-in replacement for Unix PCNTL. Features with unavailable kernel semantics remain unexported instead of returning misleading successes.

## Implemented APIs

28 of the 34 function names from the PHP 8.5 conditional pcntl stub are exported. See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) for the per-function compatibility matrix and exact limitations.

- **Child processes**: pcntl_windows_spawn (extra helper), pcntl_wait, pcntl_waitpid, pcntl_waitid, wait status decoders, resource usage. Child handles are request-local, exit codes are reduced to POSIX-compatible 8-bit status, and nonblocking/any-child waits are supported.
- **Process scheduling**: pcntl_getpriority, pcntl_setpriority, pcntl_getcpuaffinity, pcntl_setcpuaffinity and pcntl_getcpu using Win32 process APIs. These expose coarse priority mapping and processor-group-relative affinity.
- **Cooperative signals**: pcntl_signal, pcntl_signal_get_handler, pcntl_signal_dispatch, pcntl_async_signals, pcntl_sigprocmask, pcntl_alarm, pcntl_sigwaitinfo and pcntl_sigtimedwait. Console control events, Windows timers and in-process injected events feed a queue. PHP handlers run on the PHP execution thread.
- **Thread QoS**: pcntl_getqos_class and pcntl_setqos_class expose Pcntl\QosClass, mapped to Windows thread priorities. This is not identical to macOS scheduling QoS.
- **Errors**: pcntl_errno, pcntl_get_last_error, pcntl_strerror and selected POSIX-style error constants.

## Still unavailable on native Windows

- pcntl_fork: live memory, stack and instruction-pointer duplication of the running PHP interpreter.
- pcntl_exec: process image replacement with identical PID and inherited process identity.
- pcntl_forkx: Solaris-specific process creation flags.
- pcntl_rfork: BSD-specific fork/resource sharing.
- pcntl_unshare: Linux namespace creation and isolation.
- pcntl_setns: Linux namespace membership changes.

These need real additional runtime capabilities, not stub implementations. [docs/PORTABILITY.md](docs/PORTABILITY.md) covers Windows-native, Cygwin, WSL2 and platform-specific backends.

## Build and verify

The repo provides a Windows GitHub Actions workflow using the PHP Foundation's official Windows extension builder. Builds target PHP 8.4 / 8.5, x64 and x86, TS and NTS. For every supported configuration, the builder runs the PHPT suite and uploads a php_pcntl.dll build artifact when successful.

On Windows with matching PHP development tools, build with php/php-windows-builder and the --enable-pcntl configuration option (defined in config.w32). Then load the DLL using the matching PHP CLI build and enable it in php.ini:

```ini
extension=php_pcntl.dll
```

Example:

```php
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(37);']);
if ($pid === false || pcntl_waitpid($pid, $status) !== $pid) {
    throw new RuntimeException(pcntl_strerror(pcntl_errno()));
}
echo pcntl_wexitstatus($status); // 37
```

## Compatibility and safety

The supported signal queue does not implement arbitrary POSIX process-directed signals, Linux syscall interruption semantics, or native Unix signal dispositions. Some functions return narrowed Windows-derived results, and several POSIX-specific constants are intentionally absent. Use the helper pcntl_windows_raise only for cooperative local events and testing, not as a substitute for posix_kill.

Supported usage is a single active PHP CLI request per process. Concurrent requests in ZTS servers, unusual extension combinations, process groups, Linux namespaces and exact POSIX fork semantics remain outside the verified scope. Review [docs/SIGNALS.md](docs/SIGNALS.md) and [docs/NATIVE-SCOPE.md](docs/NATIVE-SCOPE.md) before use.

**Status: experimental.** Do not publish the extension as production-ready or claim full Unix PCNTL compatibility.
