# Remaining platform-specific PCNTL functions

The full PHP 8.5 PCNTL stub declares functions from several unrelated Unix kernels. No native Win32 DLL can reproduce all their kernel contracts.

- Linux or WSL2 Linux PHP: pcntl_fork, pcntl_exec, pcntl_unshare and pcntl_setns. Namespace creation and membership depend on Linux kernel permissions, containers and privileges. Run the Linux reference workflow and optional namespace probe.
- FreeBSD PHP: pcntl_rfork, with RFPROC and descriptor flags. A FreeBSD VM workflow installs the upstream PHP 8.4 PCNTL package and runs the rfork conformance test.
- Solaris/illumos PHP: pcntl_forkx, with FORK_WAITPID. A separate manual Solaris VM workflow is provided for this OS-specific contract. It is not marked verified until the workflow is run successfully.
- Darwin PHP: pcntl_getqos_class and pcntl_setqos_class use Apple's QoS behavior. A macOS CI workflow compares those functions with the native macOS scheduler.

These reference tests invoke PHP compiled for the relevant operating system. They do not load our Windows DLL and are not evidence that the Windows PHP process can fork, exec or enter Linux namespaces.

References: https://www.php.net/manual/en/book.pcntl.php and https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl.stub.php.
