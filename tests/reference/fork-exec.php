<?php
declare(strict_types=1);
if (PHP_SAPI !== 'cli' || !function_exists('pcntl_fork') || !function_exists('pcntl_exec')) {
    fwrite(STDERR, "Real POSIX PCNTL CLI required\n");
    exit(77);
}
$heap = ['value' => 5];
$parent = getmypid();
$child = pcntl_fork();
if ($child < 0) throw new RuntimeException('fork failed');
if ($child === 0) {
    if (getmypid() === $parent || $heap['value'] !== 5) exit(70);
    $heap['value'] = 99;
    // exec must preserve the POSIX process ID.
    pcntl_exec(PHP_BINARY, [__DIR__.'/fixtures/exec-child.php', (string)getmypid()]);
    exit(71);
}
if ($heap['value'] !== 5) throw new RuntimeException('Parent heap modified');
$result = pcntl_waitpid($child, $status);
if ($result !== $child || !pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 73) {
    throw new RuntimeException('Fork/exec identity or wait behavior differs from POSIX');
}
echo "fork/exec passed\n";
