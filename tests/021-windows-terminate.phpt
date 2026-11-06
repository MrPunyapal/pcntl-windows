--TEST--
Native Windows termination of a registered child process
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_terminate')) die('skip Windows PCNTL required'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(5000000); exit(9);']);
var_dump($pid > 0);
var_dump(pcntl_windows_terminate($pid, 42));
var_dump(pcntl_waitpid($pid, $status) === $pid);
var_dump(pcntl_wifexited($status));
var_dump(pcntl_wexitstatus($status));
var_dump(pcntl_windows_terminate($pid, 42));
var_dump(pcntl_errno() === PCNTL_ECHILD);
try {
    pcntl_windows_terminate(1, 256);
} catch (ValueError $e) {
    echo "value-error\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
int(42)
bool(false)
bool(true)
value-error
