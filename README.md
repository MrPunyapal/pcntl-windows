# pcntl-windows

Experimental **native Windows PHP PCNTL extension**. The goal is to investigate the entire upstream PCNTL API, including functions with no native Win32 equivalent. This is **not a drop-in PCNTL replacement** today.

The first slice provides real process creation and waiting using `CreateProcessW`, Windows process handles, and `WaitForMultipleObjects`.

## Implemented in the proof of concept

- `pcntl_windows_spawn(string $executable, array $arguments = []): int|false` (Windows-specific helper, **not** `pcntl_fork`)
- `pcntl_waitpid`, `pcntl_wait` for child PIDs returned by `pcntl_windows_spawn`
- `pcntl_wifexited`, `pcntl_wexitstatus`, `pcntl_wifstopped`, `pcntl_wifsignaled`, `pcntl_wstopsig`, `pcntl_wtermsig`
- `pcntl_get_last_error`, `pcntl_errno`, `pcntl_strerror`
- `WNOHANG` and selected `PCNTL_E*` constants

Everything else is tracked in [the complete compatibility investigation](docs/COMPATIBILITY.md). No stub `pcntl_fork()` is exported. A Windows executable created with `CreateProcessW` starts a **new interpreter**; it cannot continue at the caller's `pcntl_fork()` return site with copied PHP heap/stack state.

## Build

The experimental Windows build uses the PHP Foundation's [Windows extension builder](https://github.com/php/php-windows-builder) in GitHub Actions for PHP 8.4/8.5, x64 NTS and TS. The extension's `config.w32` enables `--enable-pcntl`. No prebuilt binaries are published yet.

On a suitable Windows PHP development environment, use `Invoke-PhpBuildExtension -PhpVersion 8.4 -Arch x64 -Ts nts -Args '--enable-pcntl'` from this repository (see the builder's local setup instructions).

Enable the resulting extension in the matching Windows PHP CLI build:

```ini
extension=php_pcntl.dll
```

## Example

```php
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(37);']);
if ($pid === false) {
    throw new RuntimeException(pcntl_strerror(pcntl_errno()));
}

$result = pcntl_waitpid($pid, $status);
if ($result === -1) {
    throw new RuntimeException(pcntl_strerror(pcntl_errno()));
}

assert($result === $pid);
assert(pcntl_wifexited($status));
assert(pcntl_wexitstatus($status) === 37);
```

## Intentional first-slice limitations

- Child registration is **request-local**. `waitpid` supports a specific registered PID or `-1` (any registered child); PID `0` and negative process groups are not yet supported.
- Only `WNOHANG` is supported. Up to 64 outstanding child handles, pending a scalable wait backend.
- Normal exits are encoded like POSIX wait statuses using the low eight bits of the Win32 exit code. This loses Windows-specific 32-bit exit information and **does not** claim POSIX signal termination semantics.
- `resource_usage` is returned as an empty array; Win32 resource usage translation is not yet implemented.
- All spawned children must run within this request to be waited on, but closing an outstanding handle at request shutdown does **not** kill the child.
- The helper takes an explicit executable and string arguments, without shell evaluation or inherited arbitrary handles.
- Not production ready. No actual Windows build has been confirmed until CI succeeds.

## Contributing

Prioritize semantic correctness and tests. Avoid implementations that merely make `function_exists('pcntl_fork')` true without matching upstream behavior. Preserve the public API's PHP signatures and behavior where possible. Unix-specific APIs will be investigated through kernel-backed capabilities, a POSIX compatibility runtime, or an explicitly documented alternate runtime.
