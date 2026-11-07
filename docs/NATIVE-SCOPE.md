# Native Windows implementation boundaries

The Windows extension provides 28 upstream PCNTL function names and two helper APIs, with intentionally narrower semantics where Win32 differs from POSIX. The full, maintained function-by-function status is in [COMPATIBILITY.md](COMPATIBILITY.md).

## Confirmed native building blocks

- Process spawning: CreateProcessW; process waiting: Win32 handles, WaitForSingleObject and threadpool child-exit notifications for scalable any-child waiting.
- Process accounting: GetProcessTimes and GetProcessMemoryInfo.
- Scheduling: Windows priority classes, process-affinity masks and processor numbers.
- Signal subsystem: a cooperative queue fed by console control handlers, in-process injection, and Windows timers; Zend VM interrupt dispatch runs PHP callbacks on PHP's execution thread.
- QoS-style calls: current-thread Windows priorities mapped to Pcntl\QosClass enum cases.

## Deliberate limitations

- Child handles exist only for children created by this request's pcntl_windows_spawn helper, not arbitrary Unix fork descendants or process groups.
- Status words retain only the low eight bits of Windows exit codes; stop, continue and signal-originated states have no POSIX kernel analogue.
- Signal masks and synchronous waits cover only extension-managed queued events. They do not install POSIX kernel signal masks or provide arbitrary cross-process POSIX signaling.
- The current affinity APIs expose one processor group rather than complete CPU Sets on machines with many logical processors.
- Windows scheduler classes are not numerically equivalent to POSIX nice values or macOS QoS policies.
- The extension targets one active PHP CLI request per process. Concurrent host-driven ZTS request isolation is not verified.

## Missing kernel operations

The six unexported upstream names are pcntl_fork, pcntl_exec, pcntl_forkx, pcntl_rfork, pcntl_unshare and pcntl_setns. Full behavioral parity requires a compatible runtime and sometimes a different operating-system kernel. See [PORTABILITY.md](PORTABILITY.md).

A successful native Windows build and passing PHPT suite validate this prototype, not production safety or drop-in compatibility.
