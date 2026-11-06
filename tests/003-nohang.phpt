--TEST--
WNOHANG does not consume an active child
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows extension required'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(750000); exit(4);']);
var_dump(pcntl_waitpid($pid, $status, WNOHANG));
var_dump(pcntl_waitpid($pid, $status) === $pid);
var_dump(pcntl_wexitstatus($status));
var_dump(pcntl_waitpid($pid, $unknown));
var_dump(pcntl_errno() === PCNTL_ECHILD);
?>
--EXPECT--
int(0)
bool(true)
int(4)
int(-1)
bool(true)
