# Verified native PCNTL scope

This project is a Windows compatibility experiment, not an unconditional substitute for POSIX pcntl.

Native functions added: pcntl_getpriority, pcntl_setpriority, pcntl_getcpuaffinity, pcntl_setcpuaffinity, pcntl_getcpu, pcntl_wifcontinued.

Priority classes map onto coarse POSIX nice-like values. Only PRIO_PROCESS works; changing priority will not enter Windows realtime class. Affinity operates inside the processor group visible to GetProcessAffinityMask. Windows machines with multiple CPU processor groups need a separate CPU-sets backend. pcntl_wifcontinued decodes a POSIX-style status, but this extension does not produce SIGCONT status events.

No native Windows API provides arbitrary live process fork/exec or Linux namespace operations with full POSIX semantics. The missing PCNTL functions still need research and system-level backends. Avoid substituting CreateProcessW for pcntl_fork.

Source references: https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.stub.php and https://learn.microsoft.com/en-us/windows/win32/procthread/processor-groups
