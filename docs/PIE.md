# Installing with PIE on Windows

[PIE](https://github.com/php/pie) can install prebuilt native Windows PHP extensions, so users do not need a C compiler. This repository uses [php/php-windows-builder](https://github.com/php/php-windows-builder) to build and package DLLs.

**Availability:** The PIE manifest and release workflow are prepared. Composer metadata is validated on relevant pushes to `main`, without publishing a release. However, a published GitHub release with matching binary assets **and** Packagist registration are required before a normal `pie install` command will work. Do not advertise the command as ready until an end-to-end installation has passed.

## Supported targets

- Windows-native PHP 8.4 and 8.5, x86 and x64, TS and NTS.
- Use the matching PHP architecture, thread-safety mode, and compiler/ABI.
- This Windows DLL is experimental and supports only the Windows-compatible PCNTL subset. It cannot add native `pcntl_fork`, `pcntl_exec`, `pcntl_forkx`, `pcntl_rfork`, `pcntl_unshare`, or `pcntl_setns`.
- Cygwin PHP and WSL2 Linux PHP are separate runtimes. PIE installing this DLL does not enable their kernel functions.

## Publishing the first installable version

1. Choose a version (for example, `0.1.0-beta1`) and set `PHP_PCNTL_WINDOWS_VERSION` in `php_pcntl.h` to match. Use an unprefixed semver tag (for example `0.1.0-beta1`, not `v0.1.0-beta1`).
2. Push the version tag. `.github/workflows/pie-release.yml` validates the Composer manifest, builds and tests all eight Windows PHP configurations, and stages PIE-named ZIPs on a **draft** GitHub release. A manual workflow dispatch can test building without creating a release.
3. Verify every ZIP asset is present, check that each contains the correct `php_pcntl.dll` build and that the eight test jobs passed. Test PIE installation from the release on clean matching Windows PHP installations; verify `php -m` and a simple spawn/wait smoke test.
4. Only after installation succeeds, publish the draft as a **pre-release** while the implementation remains experimental.
5. Submit `https://github.com/MrPunyapal/pcntl-windows` at [Packagist package submission](https://packagist.org/packages/submit). Package name: `mrpunyapal/pcntl-windows`; extension name: `pcntl`.

Publishing a version alone is insufficient: PIE resolves the Composer package through Packagist and downloads the compatible Windows asset from its GitHub release. GitHub Actions build artifacts from regular commits are not a substitute for release assets.

## Installation after publishing and registration

For a beta, pass an explicit pre-release version constraint:

```powershell
pie install "mrpunyapal/pcntl-windows:0.1.0-beta1@beta"
php -m
php --ri pcntl
```

The version above is an **example**, not an existing release. Replace it with the actually published tag. Once a stable version is published, `pie install mrpunyapal/pcntl-windows` can select it normally.

To target a separate installed Windows PHP executable, use PIE's `--with-php-path` flag:

```powershell
php C:\pie.phar install --with-php-path=C:\php-8.5\php.exe "mrpunyapal/pcntl-windows:0.1.0-beta1@beta"
```

If PIE cannot enable the extension automatically, use `extension=pcntl` in that PHP installation's `php.ini`. Make sure the PHP binary and DLL have matching ABI settings.

See [PIE's maintainer documentation](https://php.github.io/pie/#/extension-maintainers) and [PIE usage documentation](https://php.github.io/pie/) for updated installation and release requirements.
