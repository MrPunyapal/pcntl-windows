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

### November 11, 2026: shutdown hang localized

[Instrumented run 42876382760](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42876382760)
at `52059bef59c8c8ac9405433eafb6a5689d830b1f` reached all five checkpoints:
return to PHP, PID check, copied array mutation, `exit(73)`, and a PHP shutdown
callback. It then hung and was terminated/reaped by the parent. This corrects
the earlier uncertainty about Zend VM continuation: this narrow PHP workload
does resume successfully; ordinary process shutdown still fails.

The preserved child minidump, inspected with Microsoft DbgEng and the matching
Microsoft `ntdll.pdb`, shows the sole child thread waiting in:

```text
ntdll!NtWaitForAlertByThreadId+0x14
ntdll!RtlAcquireSRWLockExclusive+0x13d
ntdll!LdrProtectMrdata+0x18
ntdll!RtlxRemoveInvertedFunctionTable+0x41
ntdll!RtlRemoveInvertedFunctionTable+0x1d
ntdll!LdrpUnmapModule+0x23
ntdll!LdrpDereferenceModule+0xad
ntdll!LdrUnloadDll+0x52
KERNELBASE+0x2ffbe
```

The stack contains the `php_pcntl` module base as the unload target. The matching
ntdll image is 10.0.20348.5756 (resource product version 10.0.20348.5773).
This is evidence of a loader SRW-lock wait during extension unloading, not proof
of which thread originally owned the lock. The small dump lacks arbitrary lock
memory and complete PHP symbols. Do not reset undocumented loader locks.

The manual workflow now accepts `pin_module=true` as one isolated follow-up:
pin only the extension in the child using `GetModuleHandleExW` before returning
to PHP. A positive result would isolate module unmapping as a trigger; pinning
a DLL is not a general solution for inherited loader/runtime state. Default
research behavior remains unpinned. Neither mode is included in release builds.

[Pinned follow-up 42877946620](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42877946620)
reached every checkpoint and avoided the hang, but exited with native status
`3221225477` (`0xC0000005`, access violation), not 73. The test correctly failed.
Thus pinning is insufficient. WER local crash capture did not produce a dump
for this cloned process. The optional `debug_child=true` experiment instead
creates the child suspended, attaches the Windows debug API, resumes it, and
pumps events for at most 15 seconds, preserving a dump on second-chance
exceptions. This instrumentation changes process/debugger state and cannot
establish uninstrumented safety. Do not enable either option outside research.

[Debugger run 42878290271](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42878290271)
instead stopped before checkpoint 4 with second-chance `0xC0000008`
(`STATUS_INVALID_HANDLE`). Matching Microsoft symbols resolve the stack through
`KERNELBASE!CloseHandle`, `ucrtbase!_close_nolock_internal`, `_close_internal`,
`_fclose_nolock_internal`, `_fclose_internal`, `fclose`, and `php8+0x754873`.
The original PHP-side CRT descriptor table can reference kernel handles that
were not inherited by the clone. This trace establishes invalid-handle cleanup;
it does not establish that this exception is the uninstrumented access violation.

The next differential test runs the same assertions through `php -r`, removing
the source-file CRT descriptor while keeping module pinning enabled. Its output
is compared exactly with the PHPT expectation and preserved as `clone-inline.txt`.
The ordinary file-based PHPT remains mandatory and failing. Future diagnostic
artifacts also retain the exact research DLL/PDB and PHP binaries for unwinding.

The diagnostic artifact contains PHPT output/diffs/logs and
`pcntl-clone-child.dmp` for seven days. Reproduce via the manual workflow rather
than treating an expired artifact link as permanent evidence. Checkpoints 0–4
must all pass, followed by normal exit, status 73, parent heap isolation and
successful reaping. A dump or successful checkpoint alone does not pass the test.

1. Preserve the known-failing reproduction and record the exact PHP commit, native compiler, runner image, TS/NTS mode, extension list, process/thread IDs and timeout. Keep the test in the manual-only unsafe workflow.
2. Add Win32 event or direct native file markers immediately before the NT clone, on child return, and immediately before and after returning control from the extension C handler to Zend. Do not invoke Zend allocation or logging from the native marker path until it is known safe.
3. Capture a crash dump or timed process dump of the cloned child and compare its stacks, loader lock, CRT locks, TLS/FLS pointers, Zend executor state and inherited handle ownership with the parent. Distinguish deadlock from a child that exits without delivering a usable status.
4. Test one hypothesis at a time in an NTS CLI with OPcache and optional extensions disabled. A successful native marker is not proof that the PHP VM returned safely; require PHP-level output, correct PID, independent heap writes, normal exit and parent `waitpid` in the isolated test.
5. Only after a single clone passes, run repeated fork/wait, C runtime I/O, inherited streams and sockets, PHP shutdown, async signals and extension combinations. Confirm child/parent resource cleanup and no stuck processes. Never promote experimental cloning to `pcntl_fork` based on a smoke test.

A Windows-native API with no supported cloning ABI is a compatibility risk independent of test coverage. If process continuation cannot be made reliable, document it as an unresolved platform constraint rather than substituting `CreateProcessW` behind `pcntl_fork`.
