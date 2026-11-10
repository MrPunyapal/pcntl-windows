<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli' || !function_exists('pcntl_fork') || !function_exists('pcntl_exec')) {
    fwrite(STDERR, "POSIX PHP CLI with fork and exec is required\n");
    exit(77);
}

$parentPid = getmypid();
$parentCwd = getcwd();
$parentMarker = getenv('PCNTL_CONFORMANCE_EXEC_MARKER');
$directory = sys_get_temp_dir().'/pcntl-exec-'.bin2hex(random_bytes(6));
if (!mkdir($directory, 0700)) {
    throw new RuntimeException('Failed to make working directory for exec');
}

try {
    $expectedCwd = realpath($directory);
    if ($expectedCwd === false) {
        throw new RuntimeException('Unable to resolve exec test working directory');
    }
    $pid = pcntl_fork();
    if ($pid === -1) {
        throw new RuntimeException('Fork failed: '.pcntl_strerror(pcntl_errno()));
    }
    if ($pid === 0) {
        if (!chdir($directory)) {
            exit(70);
        }
        $environment = [
            'PATH' => getenv('PATH') ?: '/usr/bin',
            'PCNTL_CONFORMANCE_EXEC_MARKER' => 'child-only-value',
        ];
        pcntl_exec(PHP_BINARY, [
            '-n',
            __DIR__.'/fixtures/exec-environment-child.php',
            (string) getmypid(),
            $expectedCwd,
            'argument with spaces',
            '--leading-dashes',
            '',
        ], $environment);
        exit(71);
    }

    $waited = pcntl_waitpid($pid, $status);
    if ($waited !== $pid || !pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 41) {
        throw new RuntimeException('Exec did not preserve PID, working directory, arguments, and environment');
    }
    if (getmypid() !== $parentPid || getcwd() !== $parentCwd
        || getenv('PCNTL_CONFORMANCE_EXEC_MARKER') !== $parentMarker) {
        throw new RuntimeException('Child exec modified parent process state');
    }
    echo "Fork/exec environment, arguments, and working directory passed\n";
} finally {
    rmdir($directory);
}
