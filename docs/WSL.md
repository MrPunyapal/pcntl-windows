# Linux PCNTL under WSL2

Windows cannot clone the live address space of native PHP or enter Linux kernel namespaces. For genuine fork/exec/unshare/setns, run the entire script with Linux PHP inside WSL2. This is an alternate PHP runtime, not a native extension feature.

1. Install WSL2 with a distribution: wsl --install.
2. Inside Ubuntu, install Linux PHP CLI: sudo apt update && sudo apt install php-cli.
3. Verify Linux PHP has pcntl: wsl --exec php -m.
4. In PowerShell from this repository: ./tools/Invoke-WslPhp.ps1 -Script ./tests/reference/fork-exec.php
5. Optionally select a distribution: -Distribution Ubuntu.
6. Pass arguments as an array: -PhpArguments @('--name', 'two words').

The wrapper uses wslpath to translate the script and working directory, refuses to run without Linux PCNTL, invokes PHP directly without a shell and returns the Linux PHP exit code.

Unix process IDs, open descriptors, memory and extensions belong to the Linux interpreter. Native Windows PHP is not migrated, and the Windows php_pcntl.dll must not be loaded into WSL.

Run tools/posix-capabilities.php to list all 34 conditional upstream function names and their availability. Linux supports fork and exec, but BSD rfork, Solaris forkx and Darwin QoS are not guaranteed. Namespace calls need suitable Linux kernel support and privileges; test CLONE_NEWUSER only with PCNTL_PROBE_UNSHARE=1.

See tests/reference/fork-exec.php for a genuine fork/exec conformance test.

The PowerShell wrapper's syntax and command construction are checked on Windows CI. A mocked `wsl.exe` verifies path translation, distribution selection, namespace capability checks and separate CLI arguments (including spaces). A real WSL2 machine is still required to validate the actual Linux runtime, path translation and kernel behavior end-to-end.

Linux CI also runs an isolated namespace conformance test: `tests/reference/linux-namespace-conformance.php`. It creates a fresh network namespace with `pcntl_unshare` and attempts to join that namespace using `pcntl_setns` from a different forked PHP child. The parent process retains its own network namespace. This test prints `PASS` only after verifying namespace identity changes. It prints `SKIP` when the Linux kernel disallows namespace creation or joining; an exit code of zero alone is not evidence that the namespace feature passed.

## Select a PHP binary with verified namespace support

Some standard PHP 8.5 Linux builds omit `pcntl_setns`. The [source-build workaround](LINUX-SETNS-85.md) restores upstream's missing `pidfd_open` configure check; the resulting PHP 8.5 binary passed real network namespace creation and joining tests in Linux CI.

The WSL runner accepts a custom Linux interpreter and an optional strict capability check:

```powershell
./tools/Invoke-WslPhp.ps1 -Script ./program.php -LinuxPhp ./upstream/php-src/sapi/cli/php -RequireSetns
```

Use `-LinuxPhp /home/user/php-src/sapi/cli/php` for a binary residing inside WSL's Linux filesystem. If supplied a Windows-accessible path, the launcher translates it using `wslpath`. If supplied a Linux path or command name, it executes it as given. `-RequireSetns` refuses to run when `pcntl_setns()` is not exported, instead of silently running with reduced capabilities. Availability alone does not guarantee the current process has Linux namespace privileges.
