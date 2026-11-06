# Remaining features and runtime strategy

This is an engineering feasibility report, not a promise that all Unix PCNTL features can be delivered as a single DLL loaded into official native Windows PHP.

## Backends

- **Win32 extension (current):** child creation and waiting, status decoding, scheduling, affinity and cooperative signals using native Windows APIs. Continue hardening and extending supported semantics.
- **WSL2 Linux PHP:** run a separate Linux PHP binary to use genuine Linux fork, exec, wait, signal masks, unshare and setns. WSL2 cannot insert Linux syscalls into a live Windows PHP interpreter, and cannot directly support BSD or Solaris-only APIs.
- **Cygwin PHP:** investigate a PHP build against Cygwin's POSIX runtime. Cygwin implements a fork emulation across its own ABI and can provide Unix-like process behavior on Windows. It is not ABI-compatible with a regular Windows PHP DLL. Complex PHP extension, DLL relocation and handle inheritance tests are required.
- **FreeBSD/POSIX host:** needed to validate rfork's BSD-specific RF flags; Linux PCNTL is not the same contract.
- **Solaris/illumos host:** needed to validate forkx and its Solaris-specific flags.
- **Darwin host:** upstream QoS scheduler behavior is Apple-specific. Our Windows thread-priority mapping is useful but cannot guarantee scheduling parity.

## Recommended conformance experiments

1. Fork reference: execute arbitrary PHP after a fork return, confirm independent return values and PIDs, heap copy-on-write behavior, nested forks, streams and open sockets, OPcache, extension globals, ZTS/NTS, cleanup and exceptions. Build this test on a Cygwin-linked PHP runtime and a Linux PHP reference. Never route it through pcntl_windows_spawn.
2. Exec reference: verify executable replacement with the same PID, correct parent wait behavior and inherited file descriptor semantics.
3. Namespace reference: test each CLONE_NEW* flag in the Linux kernel, including permissions, resource visibility and setns transitions. Windows AppContainer and Job Objects can be researched separately as distinct Windows APIs.
4. Forkx and rfork reference: enumerate flag-by-flag expected results on Solaris and FreeBSD rather than assuming Linux fork equivalence.
5. Integrate upstream PHPT comparisons and conformance reports. Require behavior, not just function_exists, before publishing any compatibility claim.

## Final acceptance condition

Native Windows PHP is not feature-complete until every exported API is conformant for its documented scope. Full coverage of upstream platform-specific features across all Unix systems is a separate multi-runtime research project. Exporting unimplemented names that return success is explicitly forbidden.
