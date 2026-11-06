# Experimental PHP-VM clone research

**Danger: unsupported, must not be used in production.** The standalone process clone test passed on Windows. This experiment moves from a trivial C process to a loaded native PHP 8.5 CLI extension.

Only special builds using `--enable-pcntl --enable-pcntl-unsafe-clone` export `pcntl_windows_clone_experimental()`. A second environment gate, `PCNTL_UNSAFE_CLONE_TEST=1`, must be enabled. The default Windows extension build does **not** compile this function.

The PHPT test tries genuine parent/child continuation after the native NT clone call, PHP associative-array heap isolation, child PID change, normal `exit(73)`, and reaping via the ordinary PCNTL wait interface. Failure, hang, or crash is a **negative experimental result**, not partial support.

Passing one test will not establish production suitability: extension globals, streams, C runtime, OPcache, loaded libraries and threads require extensive compatibility work. The undocumented Windows API lacks an OS compatibility guarantee.

This function must not be exposed as `pcntl_fork` without full behavioral conformance. See [NT-CLONE.md](NT-CLONE.md).
