--TEST--
sigtimedwait consumes queued events and returns false on timeout
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_sigtimedwait')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
pcntl_windows_raise(SIGUSR1);
$result = pcntl_sigtimedwait([SIGUSR1], $info, 0, 50000000);
var_dump($result === SIGUSR1);
var_dump($info['signo'] === SIGUSR1);
var_dump(pcntl_sigtimedwait([SIGUSR1], $absent, 0, 50000000));
var_dump(pcntl_errno() === PCNTL_EAGAIN);
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
