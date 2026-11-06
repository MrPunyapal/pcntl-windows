--TEST--
Signal mask defers cooperative Windows signals until unblocked
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_sigprocmask')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$count = 0;
pcntl_signal(SIGUSR2, function () use (&$count) { $count++; });
var_dump(pcntl_sigprocmask(SIG_BLOCK, [SIGUSR2], $old));
var_dump($old === []);
pcntl_windows_raise(SIGUSR2);
pcntl_signal_dispatch();
var_dump($count === 0);
var_dump(pcntl_sigprocmask(SIG_UNBLOCK, [SIGUSR2], $blocked));
var_dump($blocked === [SIGUSR2]);
pcntl_signal_dispatch();
var_dump($count === 1);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
