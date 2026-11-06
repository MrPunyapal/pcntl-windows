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

The PowerShell wrapper's syntax is checked in Windows CI. A real WSL2 machine is still required to validate path translation, distribution selection and CLI argument forwarding end-to-end.
