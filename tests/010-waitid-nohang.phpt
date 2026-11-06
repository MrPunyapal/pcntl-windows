--TEST--
Waitid WNOHANG leaves running children registered
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_waitid')) die('skip native Windows PCNTL waitid required'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(500000); exit(5);']);
var_dump(pcntl_waitid(P_PID, $pid, $info, WEXITED | WNOHANG));
var_dump($info === []);
var_dump(pcntl_waitid(P_ALL, null, $done));
var_dump($done['pid'] === $pid);
var_dump($done['status'] === 5);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
