# Windows process termination

The extension exposes pcntl_windows_terminate(int $processId, int $exitCode = 1): bool as a Windows-specific helper. It is not part of upstream PCNTL.

It uses TerminateProcess on the saved child process handle and accepts only children returned by pcntl_windows_spawn() during the current PHP request. It cannot terminate arbitrary PIDs.

Successful termination is observed via pcntl_waitpid. The caller-selected Windows exit code is represented in the low eight bits of the POSIX-shaped wait status.

This is forceful process termination without graceful cleanup, destructor execution or POSIX signal delivery. Applications needing graceful shutdown should use IPC/cooperative shutdown first and this helper only for escalation.

If the child already exited or was not registered, this call fails and sets the extension error. It does not spawn another process.
