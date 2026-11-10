--TEST--
Async signals queued inside handlers are delivered without recursive callbacks
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
$calls = 0;
$depth = 0;
$maxDepth = 0;
pcntl_signal(SIGUSR1, function () use (&$calls, &$depth, &$maxDepth) {
    $depth++;
    $maxDepth = max($maxDepth, $depth);
    if (++$calls === 1) {
        pcntl_windows_raise(SIGUSR1);
        // Exercise VM interrupts as well as explicit nested dispatch.
        for ($i = 0; $i < 1000; $i++) {}
        pcntl_signal_dispatch();
    }
    $depth--;
});
pcntl_async_signals(true);
pcntl_windows_raise(SIGUSR1);
$deadline = microtime(true) + 2;
while ($calls < 2 && microtime(true) < $deadline) {}
pcntl_async_signals(false);
var_dump($calls === 2);
var_dump($maxDepth === 1);
?>
--EXPECT--
bool(true)
bool(true)
