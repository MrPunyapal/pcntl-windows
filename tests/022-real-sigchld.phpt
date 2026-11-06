--TEST--
SIGCHLD arrives once for a real Windows child and is delivered safely
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows PCNTL required'); ?>
--FILE--
<?php
$seen = [];
pcntl_signal(SIGCHLD, function ($signal) use (&$seen) { $seen[] = $signal; });
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(19);']);
var_dump($pid > 0);
var_dump(pcntl_waitpid($pid, $status) === $pid);
pcntl_signal_dispatch();
var_dump($seen === [SIGCHLD]);
var_dump(pcntl_wexitstatus($status) === 19);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
