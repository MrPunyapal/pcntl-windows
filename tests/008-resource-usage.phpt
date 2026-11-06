--TEST--
Windows process resource usage is populated on wait
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(40000);']);
pcntl_waitpid($pid, $status, 0, $usage);
var_dump(is_array($usage));
var_dump(array_key_exists('ru_utime.tv_sec', $usage));
var_dump(array_key_exists('ru_stime.tv_sec', $usage));
var_dump(array_key_exists('ru_maxrss', $usage));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
