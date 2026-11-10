--TEST--
Signal wait wakes on matching event, preserves unrelated signals and consumes bursts
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_sigtimedwait')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
pcntl_windows_raise(SIGUSR1);
var_dump(pcntl_sigtimedwait([SIGUSR2], $none, 0, 30000000) === false);
var_dump(pcntl_errno() === PCNTL_EAGAIN);
var_dump(pcntl_sigtimedwait([SIGUSR1], $got, 0, 1) === SIGUSR1);
var_dump($got['signo'] === SIGUSR1);
for ($i = 0; $i < 40; $i++) pcntl_windows_raise(SIGUSR2);
$ok = true;
for ($i = 0; $i < 40; $i++) {
    if (pcntl_sigwaitinfo([SIGUSR2]) !== SIGUSR2) { $ok = false; break; }
}
var_dump($ok);
var_dump(pcntl_sigtimedwait([SIGUSR2], $absent, 0, 1) === false);
var_dump(pcntl_errno() === PCNTL_EAGAIN);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
