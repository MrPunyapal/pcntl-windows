--TEST--
The Windows PCNTL extension exports exactly its documented API surface
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !extension_loaded('pcntl')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$implemented = [
    'pcntl_waitpid', 'pcntl_waitid', 'pcntl_wait',
    'pcntl_signal', 'pcntl_signal_get_handler', 'pcntl_signal_dispatch',
    'pcntl_sigprocmask', 'pcntl_sigwaitinfo', 'pcntl_sigtimedwait',
    'pcntl_wifexited', 'pcntl_wifstopped', 'pcntl_wifcontinued',
    'pcntl_wifsignaled', 'pcntl_wexitstatus', 'pcntl_wtermsig',
    'pcntl_wstopsig', 'pcntl_alarm', 'pcntl_get_last_error',
    'pcntl_errno', 'pcntl_getpriority', 'pcntl_setpriority',
    'pcntl_strerror', 'pcntl_async_signals', 'pcntl_getcpuaffinity',
    'pcntl_setcpuaffinity', 'pcntl_getcpu',
    'pcntl_getqos_class', 'pcntl_setqos_class',
];
$unavailable = [
    'pcntl_fork', 'pcntl_exec', 'pcntl_unshare',
    'pcntl_setns', 'pcntl_rfork', 'pcntl_forkx',
];
var_dump(count($implemented) === 28);
var_dump(count(array_filter($implemented, fn ($name) => !function_exists($name))) === 0);
var_dump(count($unavailable) === 6);
var_dump(count(array_filter($unavailable, 'function_exists')) === 0);
var_dump(function_exists('pcntl_windows_spawn'));
var_dump(function_exists('pcntl_windows_raise'));
var_dump(enum_exists('Pcntl\\QosClass'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
