<?php

declare(strict_types=1);

if (!extension_loaded('pcntl') || !extension_loaded('posix')) {
    fwrite(STDERR, "Requires genuine POSIX PHP with PCNTL and POSIX extensions.\n");
    exit(1);
}

$required = [
    'pcntl_fork', 'pcntl_waitpid', 'pcntl_wait', 'pcntl_signal',
    'pcntl_signal_get_handler', 'pcntl_signal_dispatch',
    'pcntl_async_signals', 'pcntl_wifexited', 'pcntl_wexitstatus',
    'pcntl_wifsignaled', 'pcntl_wtermsig', 'pcntl_wifstopped',
    'pcntl_wstopsig', 'pcntl_exec', 'pcntl_alarm',
    'pcntl_get_last_error', 'pcntl_errno', 'pcntl_strerror',
];
foreach ($required as $name) {
    if (!function_exists($name)) {
        throw new RuntimeException("Missing upstream function: {$name}");
    }
}

$sentinel = 'state-before-fork';
$pid = pcntl_fork();

if ($pid === -1) {
    throw new RuntimeException('pcntl_fork failed');
}

if ($pid === 0) {
    exit($sentinel === 'state-before-fork' ? 37 : 9);
}

$result = pcntl_waitpid($pid, $status);
if ($result !== $pid || !pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 37) {
    throw new RuntimeException('fork / wait-status contract violated');
}

$delivered = 0;
pcntl_async_signals(true);
pcntl_signal(SIGUSR1, static function () use (&$delivered): void {
    ++$delivered;
});
if (!posix_kill(getmypid(), SIGUSR1) || $delivered !== 1) {
    throw new RuntimeException('asynchronous signal contract violated');
}

echo "Upstream fork, wait-status and signal semantics validated.\n";
