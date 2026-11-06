<?php
declare(strict_types=1);

/**
 * End-to-end Linux PCNTL namespace conformance. Only child processes change
 * network namespaces. The parent runner retains its own network configuration.
 * Prints SKIP (not PASS) when kernel permissions do not permit the operation.
 */
if (PHP_OS_FAMILY !== 'Linux' || PHP_SAPI !== 'cli'
    || !function_exists('pcntl_fork')
    || !function_exists('pcntl_unshare')
    || !function_exists('pcntl_setns')
    || !defined('CLONE_NEWNET')) {
    echo "SKIP: Linux PHP with PCNTL fork, unshare, setns, CLONE_NEWNET required\n";
    exit(0);
}

$stem = sys_get_temp_dir().'/pcntl-ns-'.getmypid().'-'.bin2hex(random_bytes(6));
$ready = $stem.'.ready';
$release = $stem.'.release';
$target = pcntl_fork();
if ($target < 0) {
    throw new RuntimeException('Failed to fork namespace owner');
}
if ($target === 0) {
    try {
        if (!@pcntl_unshare(CLONE_NEWNET)) {
            file_put_contents($ready, 'SKIP: pcntl_unshare denied errno='.pcntl_errno());
            exit(77);
        }
        $identity = @readlink('/proc/self/ns/net');
        if (!$identity) {
            file_put_contents($ready, 'FAIL: cannot inspect created network namespace');
            exit(1);
        }
        file_put_contents($ready, 'READY: '.$identity);
        $limit = microtime(true) + 18;
        while (!is_file($release) && microtime(true) < $limit) {
            usleep(20000);
        }
        exit(is_file($release) ? 0 : 1);
    } catch (Throwable $e) {
        file_put_contents($ready, 'FAIL: '.$e->getMessage());
        exit(1);
    }
}

$status = 0;
$checker = null;
$checkStatus = null;
try {
    $limit = microtime(true) + 10;
    while (!is_file($ready) && microtime(true) < $limit) {
        usleep(20000);
    }
    if (!is_file($ready)) {
        throw new RuntimeException('Namespace owner failed to report readiness');
    }
    $message = trim((string) file_get_contents($ready));
    if (str_starts_with($message, 'SKIP:')) {
        throw new RuntimeException($message, 77);
    }
    if (!str_starts_with($message, 'READY: ')) {
        throw new RuntimeException($message);
    }
    $expectedNamespace = substr($message, strlen('READY: '));

    $checker = pcntl_fork();
    if ($checker < 0) {
        throw new RuntimeException('Failed to fork namespace checker');
    }
    if ($checker === 0) {
        $before = @readlink('/proc/self/ns/net');
        try {
            if (!@pcntl_setns($target, CLONE_NEWNET)) {
                $errno = pcntl_errno();
                // EACCES=13 or EPERM=1: security policy / privilege limitation.
                exit(in_array($errno, [1, 13], true) ? 77 : 1);
            }
            $after = @readlink('/proc/self/ns/net');
            exit($before !== $after && $after === $expectedNamespace ? 0 : 1);
        } catch (Throwable $e) {
            exit(1);
        }
    }
    pcntl_waitpid($checker, $checkStatus);
    $checker = null;
    if (!pcntl_wifexited($checkStatus)) {
        throw new RuntimeException('Namespace checker exited abnormally');
    }
    $exitStatus = pcntl_wexitstatus($checkStatus);
    if ($exitStatus === 77) {
        throw new RuntimeException("SKIP: pcntl_setns denied by Linux kernel policy", 77);
    }
    if ($exitStatus !== 0) {
        throw new RuntimeException('pcntl_setns did not join the target network namespace');
    }
    echo "PASS: pcntl_unshare and pcntl_setns changed network namespace in separate children\n";
} catch (RuntimeException $e) {
    if ($e->getCode() === 77) {
        echo $e->getMessage(), "\n";
    } else {
        throw $e;
    }
} finally {
    file_put_contents($release, 'done');
    if ($checker !== null && $checker > 0) {
        pcntl_waitpid($checker, $checkStatus);
    }
    pcntl_waitpid($target, $status);
    @unlink($ready);
    @unlink($release);
}
