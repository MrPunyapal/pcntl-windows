# Windows-native process cloning research

Experimental standalone C test of undocumented RtlCloneUserProcess, completely isolated from the PHP extension. The native Win32 clone call can copy a process address space and return to a corresponding instruction in parent and child, unlike CreateProcessW. This is a research candidate for full pcntl_fork behavior.

## What is tested

`research/rtl-clone.c` resolves `RtlCloneUserProcess` from ntdll, clones its own simple C process, checks that the child continues from the clone return point, changes its private memory, terminates via `NtTerminateProcess`, then confirms that the parent's memory is intact and the child exit status is 39. The job runs on Windows and has a short timeout. An isolated, special-build PHP clone experiment **has already been attempted and failed**: after reaching the native child path, the cloned PHP VM hung. See [PHP-NT-CLONE.md](PHP-NT-CLONE.md). Do not use this experiment in production.

## Why this is NOT a PHP implementation

These are undocumented NT APIs without a supported Microsoft compatibility contract. Even a passing standalone C probe does not establish that PHP's Zend VM, heap allocator, OPcache, C runtime, loaded extensions, sockets, thread pools, registered signal handlers, or concurrency locks are safe in a cloned child. Kernel handles are not all inheritable; some parent-child library state can deadlock or crash. Thread duplication and shutdown semantics remain very different from Linux fork. The native Windows PHP extension must not export pcntl_fork until an isolated PHP interpreter experiment proves genuine parent-child continuation under representative workloads.

## Relationship to alternative runtimes

WSL2 provides real Linux fork, exec and namespaces by running Linux PHP. Cygwin provides a userland POSIX compatibility implementation with its own ABI, which does not load as a DLL into official Windows PHP. These are different approaches from directly cloning a native Windows process.

Sources:
- https://www.huntandhackett.com/blog/the-definitive-guide-to-process-cloning-on-windows
- https://github.com/huntandhackett/process-cloning
- https://cygwin.com/faq.html
