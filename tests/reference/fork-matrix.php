<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli' || !function_exists('pcntl_fork')) {
    fwrite(STDERR, "POSIX PHP CLI with pcntl_fork is required\n");
    exit(77);
}

$state = ['value' => 'parent'];
$stream = fopen('php://temp', 'w+');
if ($stream === false || fwrite($stream, 'parent') !== 6) {
    throw new RuntimeException('Unable to initialize PHP stream before fork');
}

$parentPid = getmypid();
$outer = pcntl_fork();
if ($outer === -1) {
    throw new RuntimeException('Outer fork failed: '.pcntl_strerror(pcntl_errno()));
}
if ($outer === 0) {
    if (getmypid() === $parentPid || $state['value'] !== 'parent') exit(60);

    rewind($stream);
    if (stream_get_contents($stream) !== 'parent') exit(61);

    $state['value'] = 'child';
    rewind($stream);
    ftruncate($stream, 0);
    fwrite($stream, 'child');

    $inner = pcntl_fork();
    if ($inner === -1) exit(62);
    if ($inner === 0) {
        exit($state['value'] === 'child' && getmypid() !== $parentPid ? 23 : 63);
    }

    $result = pcntl_waitpid($inner, $nestedStatus);
    exit($result === $inner && pcntl_wifexited($nestedStatus)
        && pcntl_wexitstatus($nestedStatus) === 23 ? 31 : 64);
}

if ($state['value'] !== 'parent') {
    throw new RuntimeException('Parent heap was modified immediately after fork');
}
$result = pcntl_waitpid($outer, $status);
if ($result !== $outer || !pcntl_wifexited($status)
    || pcntl_wexitstatus($status) !== 31) {
    throw new RuntimeException('Nested fork failed to preserve PHP heap or wait statuses');
}
rewind($stream);
if (stream_get_contents($stream) !== 'parent') {
    throw new RuntimeException('The parent PHP stream memory was modified by a child');
}
fclose($stream);
echo "Nested fork, resource inheritance, and heap isolation passed\n";
