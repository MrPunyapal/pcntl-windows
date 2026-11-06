<?php

declare(strict_types=1);

// PHP 8.5 ext/pcntl/pcntl.stub.php. Some functions are conditional by OS.
if (!extension_loaded('pcntl')) {
    fwrite(STDERR, "The real pcntl extension must be loaded.\n");
    exit(1);
}

$required = [
    'pcntl_fork', 'pcntl_waitpid', 'pcntl_wait',
    'pcntl_signal', 'pcntl_signal_get_handler', 'pcntl_signal_dispatch',
    'pcntl_wifexited', 'pcntl_wifstopped', 'pcntl_wifsignaled',
    'pcntl_wexitstatus', 'pcntl_wtermsig', 'pcntl_wstopsig',
    'pcntl_exec', 'pcntl_alarm', 'pcntl_get_last_error',
    'pcntl_errno', 'pcntl_strerror', 'pcntl_async_signals',
];

$conditional = [
    'pcntl_waitid' => 'waitid plus POSIX id types',
    'pcntl_sigprocmask' => 'sigprocmask',
    'pcntl_sigwaitinfo' => 'sigwaitinfo and sigtimedwait',
    'pcntl_sigtimedwait' => 'sigwaitinfo and sigtimedwait',
    'pcntl_wifcontinued' => 'WIFCONTINUED',
    'pcntl_getpriority' => 'getpriority',
    'pcntl_setpriority' => 'setpriority',
    'pcntl_unshare' => 'Linux namespace support',
    'pcntl_rfork' => 'BSD rfork',
    'pcntl_forkx' => 'Solaris forkx',
    'pcntl_setns' => 'Linux setns support',
    'pcntl_getcpuaffinity' => 'CPU affinity support',
    'pcntl_setcpuaffinity' => 'CPU affinity support',
    'pcntl_getcpu' => 'sched_getcpu',
    'pcntl_getqos_class' => 'Darwin QoS',
    'pcntl_setqos_class' => 'Darwin QoS',
];

$missing = array_values(array_filter($required, static fn (string $name): bool => !function_exists($name)));
$optional = [];
foreach ($conditional as $name => $condition) {
    $optional[$name] = ['available' => function_exists($name), 'condition' => $condition];
}

$constantGroups = get_defined_constants(true);
$constants = $constantGroups['pcntl'] ?? [];
$functions = get_extension_funcs('pcntl') ?: [];
sort($functions);

echo json_encode([
    'php' => PHP_VERSION,
    'os' => PHP_OS,
    'os_family' => PHP_OS_FAMILY,
    'required_total' => count($required),
    'required_missing' => $missing,
    'conditional_total' => count($conditional),
    'conditional' => $optional,
    'exported_functions' => $functions,
    'exported_constants_count' => count($constants),
    'complete_php_8_5_function_inventory' => count($required) + count($conditional) === 34,
], JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR), PHP_EOL;

if ($missing !== []) {
    fwrite(STDERR, "Upstream PCNTL is missing mandatory PHP 8.5 functions.\n");
    exit(1);
}
