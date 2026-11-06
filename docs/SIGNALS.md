# Windows signal compatibility

This subsystem delivers only cooperative Windows events, and it is CLI-oriented. It is not a POSIX signal implementation.

Available: pcntl_signal, pcntl_signal_get_handler, pcntl_signal_dispatch, pcntl_sigprocmask, pcntl_alarm, pcntl_sigwaitinfo, pcntl_sigtimedwait and a nonstandard pcntl_windows_raise for testable in-process events. It installs a Windows console control handler to queue CTRL_C as SIGINT and CTRL_BREAK as a SIGTERM-like cooperative event. The console callback never runs PHP code. Registered PHP handlers execute only when the PHP thread calls pcntl_signal_dispatch. The timer queue callback also only increments an atomic queue entry.

Known differences: SIGUSR1, SIGUSR2, SIGHUP and SIGCHLD have no native OS delivery source in this version (they can be sent through pcntl_windows_raise). Standard POSIX signal masks apply only to extension-owned cooperative events. SIG_DFL does not synthesize Unix signal dispositions for injected events. Restarting interrupted system calls is not supported. pcntl_alarm schedules a Windows timer but does not automatically execute PHP handlers. No POSIX siginfo metadata or process-directed kill is guaranteed.

Do not treat pcntl_windows_raise as posix_kill. Do not claim async signal support until the Zend VM interrupt bridge is correct under both TS and NTS.

Synchronous signal waits consume extension-managed pending events only. Timeout handling uses Windows millisecond scheduling and may exceed a requested nanosecond interval. These calls do not receive arbitrary process-directed POSIX kernel signals.
