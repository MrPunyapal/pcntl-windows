<?php
declare(strict_types=1);

if (PHP_OS_FAMILY !== 'BSD') {
    fwrite(STDERR, "FreeBSD is required\n");
    exit(77);
}
if (!function_exists('pcntl_rfork') || !defined('RFPROC') || !defined('RFFDG')) {
    throw new RuntimeException('This BSD PHP build does not expose pcntl_rfork or its required flags');
}

$parent = getmypid();
$pid = pcntl_rfork(RFPROC | RFFDG);
if ($pid < 0) {
    throw new RuntimeException('pcntl_rfork failed: '.pcntl_strerror(pcntl_errno()));
}
if ($pid === 0) {
    exit(getmypid() !== $parent ? 43 : 44);
}
if (pcntl_waitpid($pid, $status) !== $pid) {
    throw new RuntimeException('Unable to wait for rfork child');
}
if (!pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 43) {
    throw new RuntimeException('rfork process identity or wait status was incorrect');
}
echo "FreeBSD rfork conformance passed\n";
