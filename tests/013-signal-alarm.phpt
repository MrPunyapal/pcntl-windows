--TEST--
Windows timer enqueues SIGALRM, dispatched on PHP execution thread
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_alarm')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$received = [];
pcntl_signal(SIGALRM, function ($sig) use (&$received) { $received[] = $sig; });
var_dump(pcntl_alarm(1));
usleep(1300000);
var_dump(pcntl_signal_dispatch());
var_dump($received === [SIGALRM]);
var_dump(pcntl_alarm(0));
?>
--EXPECT--
int(0)
bool(true)
bool(true)
int(0)
