# A real POSIX fork on Windows: Cygwin runtime experiment

Official Windows PHP binaries are built with MSVC; Cygwin-ABI binaries cannot be mixed with those PHP extensions. The alternative explored here is **compiling the entire PHP CLI against the Cygwin POSIX runtime**, so PCNTL can call Cygwin fork rather than pretending `CreateProcessW()` implements POSIX fork.

The Cygwin fork implementation copies live stack, heap, and program state using process initialization and memory copying. It is not perfectly reliable: DLL address collisions and ASLR can produce fork failures. This experiment therefore tests actual continuation behavior with `tests/reference/fork-exec.php`; a successful build would establish *a distinct Cygwin-linked PHP runtime*, not complete PCNTL parity in an ordinary Windows PHP DLL.

Workflow: `.github/workflows/cygwin-reference.yml` checks out PHP 8.5, installs Cygwin build dependencies, compiles PHP with `--enable-pcntl`, and runs upstream PCNTL fork/exec behavior tests. This is a research workflow; build success is not assumed until verified.

PHP's official Windows SDK documentation identifies Cygwin and MinGW as unsupported compiler toolchains for official builds. This source-based build is consequently experimental, even when tests pass.

References:
- https://cygwin.com/cygwin-ug-net/highlights.html
- https://wiki.php.net/internals/windows/stepbystepbuild_sdk_2
- https://www.php.net/manual/en/install.unix.source.php

## Build outputs and dependencies

If and only if the Cygwin workflow's **real fork/exec and nested-fork tests pass**, it uploads a `cygwin-php85-pcntl-x64` artifact containing the compiled `php.exe`. The executable needs the **Cygwin runtime and its shared-library dependencies installed** on the destination Windows machine. It is not a self-contained Windows PHP distribution and cannot load extensions compiled with MSVC for ordinary PHP Windows builds.

No standalone binary is advertised until the entire job succeeds. The Cygwin toolchain must use a Cygwin-linked `re2c`; the Windows-native generator cannot consume paths beginning with `/cygdrive/`.
