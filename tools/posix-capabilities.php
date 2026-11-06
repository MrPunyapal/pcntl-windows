<?php
declare(strict_types=1);
$api = [
    'fork','waitpid','waitid','wait','signal','signal_get_handler','signal_dispatch',
    'sigprocmask','sigwaitinfo','sigtimedwait','wifexited','wifstopped',
    'wifcontinued','wifsignaled','wexitstatus','wtermsig','wstopsig','exec',
    'alarm','get_last_error','errno','getpriority','setpriority','strerror',
    'async_signals','unshare','rfork','forkx','setns','getcpuaffinity',
    'setcpuaffinity','getcpu','getqos_class','setqos_class',
];
$functions = [];
foreach ($api as $name) {
    $functions['pcntl_'.$name] = function_exists('pcntl_'.$name);
}
echo json_encode([
    'os' => PHP_OS_FAMILY,
    'php' => PHP_VERSION,
    'sapi' => PHP_SAPI,
    'extension_loaded' => extension_loaded('pcntl'),
    'available' => array_sum($functions),
    'functions' => $functions,
    'CLONE_NEWUSER' => defined('CLONE_NEWUSER'),
], JSON_PRETTY_PRINT | JSON_THROW_ON_ERROR), PHP_EOL;
