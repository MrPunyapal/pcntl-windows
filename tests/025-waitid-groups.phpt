--TEST--
Native Windows PCNTL waitid(P_PGID) checks scoped process groups
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_waitid')) die('skip Windows PCNTL'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(39);'], true);
var_dump($pid > 0);
var_dump(pcntl_waitid(P_PGID, $pid, $info, WEXITED | WNOWAIT));
var_dump($info['pid'] === $pid && $info['status'] === 39);
var_dump(pcntl_waitpid(-$pid, $status) === $pid);
var_dump(pcntl_waitid(P_PGID, $pid, $unused, WEXITED));
var_dump(pcntl_errno() === PCNTL_ECHILD);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
