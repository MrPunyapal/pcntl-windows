--TEST--
Async signal callbacks execute on the PHP execution thread
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_async_signals')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$events = [];
pcntl_signal(SIGUSR1, function (int $signal) use (&$events) { $events[] = $signal; });
var_dump(pcntl_async_signals());
var_dump(pcntl_async_signals(true));
pcntl_windows_raise(SIGUSR1);
for ($i = 0; $i < 10000 && $events === []; $i++) { }
var_dump($events === [SIGUSR1]);
var_dump(pcntl_async_signals(false));
var_dump(pcntl_async_signals());
?>
--EXPECT--
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
