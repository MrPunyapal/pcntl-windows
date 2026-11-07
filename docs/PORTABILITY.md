# Remaining features and runtime strategy

This is an engineering feasibility report, not a promise that all Unix PCNTL features can be delivered as a single DLL loaded into official native Windows PHP.

## Backends

- **Win32 extension (current):** child creation and waiting, status decoding, scheduling, affinity and cooperative signals using native Windows APIs. Continue hardening and extending supported semantics.
- **WSL2 Linux PHP:** tools/Invoke-WslPhp.ps1 launches the entire script inside Linux with upstream PCNTL; Linux CI verifies fork/exec semantics. Actual WSL host-wrapper execution requires a configured WSL2 installation. Linux unshare and setns still depend on privileges. WSL2 cannot insert Linux syscalls into a live Windows PHP interpreter, and cannot directly support BSD or Solaris-only APIs.
- **Cygwin PHP (focused conformance passed):** a Cygwin-linked PHP 8.5.14-dev CLI built successfully on Windows and passed actual fork/exec, nested-fork, copied heap, temporary stream and wait tests ([CI](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42371525809)). Its 27/34-function interpreter is a different ABI, not a DLL for native Windows PHP. More sockets, extension, DLL relocation and threading coverage is needed.
- **FreeBSD/POSIX host:** a FreeBSD VM workflow uses the real PHP PCNTL implementation to test rfork; Linux PCNTL is not the same contract.
- **Solaris/illumos host:** the reference VM built PHP 8.4 with PCNTL and passed the actual `forkx` test ([CI](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42297736941)). Solaris process behavior is not available in ordinary Windows PHP.
- **Darwin host:** a macOS reference workflow tests Apple's native QoS API. That behavior is Apple-specific. Our Windows thread-priority mapping is useful but cannot guarantee scheduling parity.

## Recommended conformance experiments

1. Fork reference: core fork/exec, copied PHP heap, temporary streams, nested forks and child waiting have already passed on Cygwin PHP and Linux. The next tests should cover sockets, OPcache, extension globals, threads, cleanup and exceptions. Never route them through `pcntl_windows_spawn`.
2. Exec reference: verify executable replacement with the same PID, correct parent wait behavior and inherited file descriptor semantics.
3. Namespace reference: test each CLONE_NEW* flag in the Linux kernel, including permissions, resource visibility and setns transitions. Windows AppContainer and Job Objects can be researched separately as distinct Windows APIs.
4. Forkx and rfork reference: enumerate flag-by-flag expected results on Solaris and FreeBSD rather than assuming Linux fork equivalence.
5. Integrate upstream PHPT comparisons and conformance reports. Require behavior, not just function_exists, before publishing any compatibility claim.

## Final acceptance condition

Native Windows PHP is not feature-complete until every exported API is conformant for its documented scope. Full coverage of upstream platform-specific features across all Unix systems is a separate multi-runtime research project. Exporting unimplemented names that return success is explicitly forbidden.
