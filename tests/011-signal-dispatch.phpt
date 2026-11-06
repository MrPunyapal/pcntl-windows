--TEST--
Cooperative signal registration and queued dispatch
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$received = [];
pcntl_signal(SIGUSR1, function (int $signal) use (&$received) { $received[] = $signal; });
var_dump(is_callable(pcntl_signal_get_handler(SIGUSR1)));
var_dump(pcntl_windows_raise(SIGUSR1));
var_dump($received === []);
var_dump(pcntl_signal_dispatch());
var_dump($received === [SIGUSR1]);
pcntl_signal(SIGUSR1, SIG_IGN);
var_dump(pcntl_signal_get_handler(SIGUSR1) === SIG_IGN);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
