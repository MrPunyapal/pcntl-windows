--TEST--
Successful Windows WNOHANG waits clear a stale extension error
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(850000); exit(37);']);
var_dump($pid > 0);
var_dump(pcntl_waitpid(99999999, $unused) === -1);
var_dump(pcntl_errno() === PCNTL_ECHILD);
var_dump(pcntl_waitpid($pid, $status, WNOHANG) === 0);
var_dump(pcntl_errno() === 0);
var_dump(pcntl_waitpid(99999999, $unused) === -1);
var_dump(pcntl_wait($status, WNOHANG) === 0);
var_dump(pcntl_errno() === 0);
var_dump(pcntl_waitpid($pid, $status) === $pid);
var_dump(pcntl_wifexited($status) && pcntl_wexitstatus($status) === 37);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
