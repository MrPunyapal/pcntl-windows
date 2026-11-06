<?php
declare(strict_types=1);

if (PHP_OS_FAMILY !== 'Solaris') {
    fwrite(STDERR, "Solaris or illumos is required\n");
    exit(77);
}
if (!function_exists('pcntl_forkx') || !defined('FORK_WAITPID')) {
    throw new RuntimeException('This Solaris PHP build does not expose pcntl_forkx and FORK_WAITPID');
}

$parent = getmypid();
$pid = pcntl_forkx(FORK_WAITPID);
if ($pid < 0) {
    throw new RuntimeException('pcntl_forkx failed: '.pcntl_strerror(pcntl_errno()));
}
if ($pid === 0) {
    exit(getmypid() !== $parent ? 44 : 45);
}
if (pcntl_waitpid($pid, $status) !== $pid) {
    throw new RuntimeException('Unable to wait specifically for forkx child');
}
if (!pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 44) {
    throw new RuntimeException('forkx process identity or wait status was incorrect');
}
echo "Solaris forkx conformance passed\n";
