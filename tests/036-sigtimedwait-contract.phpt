--TEST--
sigtimedwait rejects zero duration and accepts large durations without overflow
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
pcntl_windows_raise(SIGUSR1);
try { pcntl_sigtimedwait([SIGUSR1], $info); echo "bad\n"; }
catch (ValueError) { echo "zero timeout\n"; }
// Validation failure must preserve a queued signal for the next valid wait.
var_dump(pcntl_sigtimedwait([SIGUSR1], $info, PHP_INT_MAX) === SIGUSR1);
var_dump($info['signo'] === SIGUSR1);
$absent = ['sentinel' => true];
var_dump(pcntl_sigtimedwait([SIGUSR1], $absent, 0, 1));
var_dump($absent === ['sentinel' => true]);
?>
--EXPECT--
zero timeout
bool(true)
bool(true)
bool(false)
bool(true)
