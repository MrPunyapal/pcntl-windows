--TEST--
Async Windows alarm dispatches without explicit pcntl_signal_dispatch
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_async_signals')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$called = false;
pcntl_signal(SIGALRM, function () use (&$called) { $called = true; });
pcntl_async_signals(true);
pcntl_alarm(1);
$end = microtime(true) + 3;
while (!$called && microtime(true) < $end) {
    usleep(10000);
}
var_dump($called);
pcntl_alarm(0);
pcntl_async_signals(false);
?>
--EXPECT--
bool(true)
