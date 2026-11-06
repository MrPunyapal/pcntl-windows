# PHP 8.5 Linux `pcntl_setns` source-build check

The GitHub-hosted PHP 8.5 package used in our reference CI exports `pcntl_unshare` but not `pcntl_setns`, while PHP 8.4 exports both and passes actual network namespace creation/join conformance.

PHP 8.5's upstream `ext/pcntl/pcntl_arginfo.h` only exports `pcntl_setns` when `HAVE_PIDFD_OPEN` is defined. PHP 8.5's `ext/pcntl/config.m4` checks `SYS_pidfd_open` but no longer lists `pidfd_open` in `AC_CHECK_FUNCS`, unlike PHP 8.4.

The experimental `tools/build-linux-php85-pcntl.sh` script checks out PHP-8.5 (as a separate source directory), restores the `pidfd_open` configuration check in that checkout, runs `./buildconf --force`, compiles the PHP CLI with `--enable-pcntl`, then requires `function_exists('pcntl_setns')` and runs fork/exec conformance.

Linux CI also invokes the real network namespace creation/join test under root, reporting `PASS` only when namespace membership changes, and `SKIP` when kernel policy denies it.

Running the script inside WSL2 builds a separate Linux PHP runtime and does not add Linux kernel namespaces to native Windows PHP.

Source references:
- https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/config.m4
- https://github.com/php/php-src/blob/PHP-8.5/ext/pcntl/pcntl_arginfo.h
- https://github.com/php/php-src/blob/PHP-8.4/ext/pcntl/config.m4
