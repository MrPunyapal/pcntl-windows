--TEST--
CreateProcessW child and waitpid exit status
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows extension required'); ?>
--FILE--
<?php
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(37);']);
var_dump($pid > 0);
$result = pcntl_waitpid($pid, $status);
var_dump($result === $pid);
var_dump(pcntl_wifexited($status));
var_dump(pcntl_wexitstatus($status));
var_dump(pcntl_errno());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
int(37)
int(0)
