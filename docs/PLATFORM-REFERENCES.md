# Remaining platform-specific PCNTL functions

The full PHP 8.5 PCNTL stub declares functions from several unrelated Unix kernels. No native Win32 DLL can reproduce all their kernel contracts.

- Linux or WSL2 Linux PHP: pcntl_fork, pcntl_exec, pcntl_unshare and pcntl_setns. Namespace creation and membership depend on Linux kernel permissions, containers and privileges. Run the Linux reference workflow and optional namespace probe.
- FreeBSD PHP: pcntl_rfork, with RFPROC and descriptor flags. A FreeBSD VM workflow installs the upstream PHP 8.4 PCNTL package and runs the rfork conformance test.
- Solaris/illumos PHP: `pcntl_forkx` with FORK_WAITPID **passed** in a Solaris VM using upstream PHP 8.4 compiled with `--enable-pcntl` ([CI](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42297736941)).
- Darwin PHP: pcntl_getqos_class and pcntl_setqos_class use Apple's QoS behavior. A macOS CI workflow compares those functions with the native macOS scheduler.

A separate Cygwin-linked PHP 8.5 build on Windows passed [real fork/exec and nested-fork conformance](https://github.com/MrPunyapal/pcntl-windows/actions/runs/42371525809). It exports 27/34 conditional PCNTL names and requires the Cygwin runtime. See [CYGWIN.md](CYGWIN.md).

These reference tests invoke PHP compiled for the relevant operating system. They do not load our Windows DLL and are not evidence that the Windows PHP process can fork, exec or enter Linux namespaces.

References: https://www.php.net/manual/en/book.pcntl.php and https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.stub.php.
