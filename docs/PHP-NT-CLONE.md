# Experimental PHP-VM clone research

**Danger: unsupported, must not be used in production.** The standalone process clone test passed on Windows. This experiment moves from a trivial C process to a loaded native PHP 8.5 CLI extension.

Only special builds using `--enable-pcntl --enable-pcntl-unsafe-clone` export `pcntl_windows_clone_experimental()`. A second environment gate, `PCNTL_UNSAFE_CLONE_TEST=1`, must be enabled. The default Windows extension build does **not** compile this function.

The PHPT test tries genuine parent/child continuation after the native NT clone call, PHP associative-array heap isolation, child PID change, normal `exit(73)`, and reaping via the ordinary PCNTL wait interface. Failure, hang, or crash is a **negative experimental result**, not partial support.

Passing one test will not establish production suitability: extension globals, streams, C runtime, OPcache, loaded libraries and threads require extensive compatibility work. The undocumented Windows API lacks an OS compatibility guarantee.

This function must not be exposed as `pcntl_fork` without full behavioral conformance. See [NT-CLONE.md](NT-CLONE.md).

## Confirmed failing PHP experiment

On 7 November 2026, the special Windows PHP 8.5 NTS CI build reached the child after `RtlCloneUserProcess` (the native synchronization probe printed `nt-clone-child-reached=1`), but the cloned PHP child did not successfully return from the Zend VM or exit normally. The conformance test reported `child-hung` and failed. Native process duplication alone does not restore PHP's runtime locks, threads or internal state.

The unsafe clone workflow is **manual-only** while this failure is known. This prevents an expected negative research result from failing routine supported-extension builds, but does not declare the experiment successful. Run the workflow on demand for new research hypotheses; an upstream-compatible `pcntl_fork` remains unavailable in the standard Windows DLL.
