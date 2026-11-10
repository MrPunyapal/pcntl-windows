--TEST--
Unblocking pending signals schedules async delivery after an earlier interrupt
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
foreach ([SIG_UNBLOCK, SIG_SETMASK] as $mode) {
    $count = 0;
    pcntl_signal(SIGUSR1, function () use (&$count) { $count++; });
    pcntl_sigprocmask(SIG_BLOCK, [SIGUSR1]);
    pcntl_async_signals(true);
    pcntl_windows_raise(SIGUSR1);
    // Let the original VM interrupt run while the signal is still blocked.
    for ($i = 0; $i < 10000; $i++) {}
    var_dump($count === 0);
    pcntl_sigprocmask($mode, $mode === SIG_UNBLOCK ? [SIGUSR1] : []);
    for ($i = 0; $i < 10000 && $count === 0; $i++) {}
    var_dump($count === 1);
    pcntl_async_signals(false);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
