<?php
declare(strict_types=1);
if (PHP_OS_FAMILY !== 'Linux' || PHP_SAPI !== 'cli') exit(77);
foreach (['pcntl_unshare','pcntl_setns','pcntl_rfork','pcntl_forkx'] as $name) {
    printf("%s: %s\n", $name, function_exists($name) ? 'available' : 'unavailable on this runtime');
}
if (!function_exists('pcntl_unshare') || !defined('CLONE_NEWUSER') ||
    getenv('PCNTL_PROBE_UNSHARE') !== '1') {
    echo "Namespace test needs Linux support, permissions and opt-in\n";
    exit(0);
}
$pid = pcntl_fork();
if ($pid < 0) throw new RuntimeException('fork failed');
if ($pid === 0) {
    $before = @readlink('/proc/self/ns/user');
    if (!@pcntl_unshare(CLONE_NEWUSER)) exit(77);
    $after = @readlink('/proc/self/ns/user');
    exit($before && $after && $before !== $after ? 0 : 1);
}
pcntl_waitpid($pid, $status);
$code = pcntl_wexitstatus($status);
if ($code === 77) {
    echo "Namespace creation denied by Linux policy\n";
    exit(0);
}
if ($code !== 0) throw new RuntimeException('Namespace identity did not change');
echo "Namespace creation passed\n";
