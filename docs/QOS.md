# Windows QoS scheduler mapping

The Pcntl\\QosClass enum and pcntl_getqos_class/pcntl_setqos_class are exposed using actual Win32 thread priority APIs. This is a deliberately limited **semantic approximation** of Darwin QoS, not an Apple QoS implementation.

| PHP QoS | Windows priority |
| --- | --- |
| UserInteractive | THREAD_PRIORITY_HIGHEST |
| UserInitiated | THREAD_PRIORITY_ABOVE_NORMAL |
| Default | THREAD_PRIORITY_NORMAL |
| Utility | THREAD_PRIORITY_BELOW_NORMAL |
| Background | THREAD_PRIORITY_LOWEST |

Changes affect the executing thread, not the whole PHP process. Windows class and per-thread priority interact; there is no one-to-one mapping to macOS energy policy, timer behavior or scheduler QoS. No realtime thread priority is used.
