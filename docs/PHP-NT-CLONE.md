# Experimental PHP-VM clone research

**Danger: unsupported, must not be used in production.** The standalone process clone test passed on Windows. This experiment moves from a trivial C process to a loaded native PHP 8.5 CLI extension.

Only special builds using `--enable-pcntl --enable-pcntl-unsafe-clone` export `pcntl_windows_clone_experimental()`. A second environment gate, `PCNTL_UNSAFE_CLONE_TEST=1`, must be enabled. The default Windows extension build does **not** compile this function.

The PHPT test tries genuine parent/child continuation after the native NT clone call, PHP associative-array heap isolation, child PID change, normal `exit(73)`, and reaping via the ordinary PCNTL wait interface. Failure, hang, or crash is a **negative experimental result**, not partial support.

Passing one test will not establish production suitability: extension globals, streams, C runtime, OPcache, loaded libraries and threads require extensive compatibility work. The undocumented Windows API lacks an OS compatibility guarantee.

This function must not be exposed as `pcntl_fork` without full behavioral conformance. See [NT-CLONE.md](NT-CLONE.md).

## Confirmed failing PHP experiment

On 7 November 2026, the special Windows PHP 8.5 NTS CI build reached the child after `RtlCloneUserProcess` (the native synchronization probe printed `nt-clone-child-reached=1`), but the cloned PHP child did not successfully return from the Zend VM or exit normally. The conformance test reported `child-hung` and failed. Native process duplication alone does not restore PHP's runtime locks, threads or internal state.

The unsafe clone workflow is **manual-only** while this failure is known. This prevents an expected negative research result from failing routine supported-extension builds, but does not declare the experiment successful. Run the workflow on demand for new research hypotheses; an upstream-compatible `pcntl_fork` remains unavailable in the standard Windows DLL.

## Next isolated experiments

1. Preserve the known-failing reproduction and record the exact PHP commit, native compiler, runner image, TS/NTS mode, extension list, process/thread IDs and timeout. Keep the test in the manual-only unsafe workflow.
2. Add Win32 event or direct native file markers immediately before the NT clone, on child return, and immediately before and after returning control from the extension C handler to Zend. Do not invoke Zend allocation or logging from the native marker path until it is known safe.
3. Capture a crash dump or timed process dump of the cloned child and compare its stacks, loader lock, CRT locks, TLS/FLS pointers, Zend executor state and inherited handle ownership with the parent. Distinguish deadlock from a child that exits without delivering a usable status.
4. Test one hypothesis at a time in an NTS CLI with OPcache and optional extensions disabled. A successful native marker is not proof that the PHP VM returned safely; require PHP-level output, correct PID, independent heap writes, normal exit and parent `waitpid` in the isolated test.
5. Only after a single clone passes, run repeated fork/wait, C runtime I/O, inherited streams and sockets, PHP shutdown, async signals and extension combinations. Confirm child/parent resource cleanup and no stuck processes. Never promote experimental cloning to `pcntl_fork` based on a smoke test.

A Windows-native API with no supported cloning ABI is a compatibility risk independent of test coverage. If process continuation cannot be made reliable, document it as an unresolved platform constraint rather than substituting `CreateProcessW` behind `pcntl_fork`.
