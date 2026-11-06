--TEST--
SIGCHLD is dispatched asynchronously from a real child exit
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_async_signals')) die('skip Windows PCNTL required'); ?>
--FILE--
<?php
$count = 0;
pcntl_signal(SIGCHLD, function () use (&$count) { $count++; });
pcntl_async_signals(true);
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(5);']);
$limit = microtime(true) + 4;
while ($count < 1 && microtime(true) < $limit) { usleep(10000); }
var_dump($count === 1);
var_dump(pcntl_waitpid($pid, $status) === $pid);
pcntl_signal_dispatch();
var_dump($count === 1);
var_dump(pcntl_wexitstatus($status) === 5);
pcntl_async_signals(false);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
