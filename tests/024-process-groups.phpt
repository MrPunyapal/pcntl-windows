--TEST--
Native Windows PCNTL process groups filter registered children
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows PCNTL'); ?>
--FILE--
<?php
$default = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(31);']);
$grouped = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(47);'], true);
var_dump($default > 0 && $grouped > 0);
var_dump(pcntl_waitpid(0, $status) === $default);
var_dump(pcntl_wexitstatus($status) === 31);
var_dump(pcntl_waitpid(-$grouped, $status) === $grouped);
var_dump(pcntl_wexitstatus($status) === 47);
var_dump(pcntl_waitpid(-$grouped, $status));
var_dump(pcntl_errno() === PCNTL_ECHILD);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(-1)
bool(true)
