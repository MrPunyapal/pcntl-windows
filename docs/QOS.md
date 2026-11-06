# Windows QoS scheduler mapping

The Pcntl\QosClass enum and pcntl_getqos_class / pcntl_setqos_class are backed by Windows thread-priority APIs. The mapping is an approximation, **not Apple QoS equivalence**.

- UserInteractive: THREAD_PRIORITY_HIGHEST.
- UserInitiated: THREAD_PRIORITY_ABOVE_NORMAL.
- Default: THREAD_PRIORITY_NORMAL.
- Utility: THREAD_PRIORITY_BELOW_NORMAL.
- Background: THREAD_PRIORITY_LOWEST.

Changes affect the executing thread, not the entire process. The Windows process priority class and thread priority interact. The extension does not enter the realtime scheduling class, and it does not reproduce macOS energy policy or scheduler guarantees.
