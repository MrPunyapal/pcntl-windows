--TEST--
Waitid returns a child's exit details and honors WNOWAIT
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_waitid')) die('skip native Windows PCNTL waitid required'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(23);']);
var_dump(pcntl_waitid(P_PID, $pid, $info, WEXITED | WNOWAIT, $usage));
var_dump($info['pid'] === $pid);
var_dump($info['status'] === 23);
var_dump(array_key_exists('ru_maxrss', $usage));
var_dump(pcntl_waitpid($pid, $status) === $pid);
var_dump(pcntl_wexitstatus($status) === 23);
var_dump(pcntl_waitid(P_PID, $pid, $other));
var_dump(pcntl_errno() === PCNTL_ECHILD);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
