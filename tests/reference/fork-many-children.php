<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli' || !function_exists('pcntl_fork') || !function_exists('pcntl_waitpid')) {
    fwrite(STDERR, "POSIX PHP CLI with fork and waitpid is required\n");
    exit(77);
}

$children = [];
for ($index = 1; $index <= 12; $index++) {
    $pid = pcntl_fork();
    if ($pid === -1) {
        throw new RuntimeException('Fork failed while creating child '.$index);
    }
    if ($pid === 0) {
        usleep(($index % 4) * 20_000);
        exit(40 + $index);
    }
    $children[$pid] = 40 + $index;
}

$deadline = microtime(true) + 40;
while ($children && microtime(true) < $deadline) {
    $pid = pcntl_waitpid(-1, $status, WNOHANG);
    if ($pid === 0) {
        usleep(20_000);
        continue;
    }
    if ($pid === -1) {
        throw new RuntimeException('waitpid(-1) reported no children while known children remain');
    }
    if (!isset($children[$pid]) || !pcntl_wifexited($status)
        || pcntl_wexitstatus($status) !== $children[$pid]) {
        throw new RuntimeException('waitpid(-1) returned unexpected child or exit status');
    }
    unset($children[$pid]);
}
if ($children) {
    throw new RuntimeException('Timed out reaping '.count($children).' forked children');
}
if (pcntl_waitpid(-1, $status, WNOHANG) !== -1) {
    throw new RuntimeException('waitpid(-1) should report ECHILD after all children are reaped');
}
echo "Repeated fork, WNOHANG, and child reaping passed\n";
