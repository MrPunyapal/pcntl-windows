--TEST--
Invalid child, flags and missing executable fail explicitly
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows extension required'); ?>
--FILE--
<?php
var_dump(pcntl_waitpid(99999999, $s));
var_dump(pcntl_errno() === PCNTL_ECHILD);
var_dump(pcntl_waitpid(-2, $s));
var_dump(pcntl_errno() === PCNTL_EINVAL);
var_dump(pcntl_windows_spawn('Z:\\nonexistent\\nothere.exe'));
var_dump(pcntl_errno() === PCNTL_ENOENT);
?>
--EXPECT--
int(-1)
bool(true)
int(-1)
bool(true)
bool(false)
bool(true)
