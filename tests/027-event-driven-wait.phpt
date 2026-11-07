--TEST--
Windows any-child waits and waitid wake for the appropriate registered process
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows PCNTL'); ?>
--FILE--
<?php
$slow = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(260000); exit(27);'], true);
$fast = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'usleep(60000); exit(14);']);
var_dump($slow > 0 && $fast > 0);
var_dump(pcntl_wait($status) === $fast);
var_dump(pcntl_wexitstatus($status) === 14);
var_dump(pcntl_waitid(P_PGID, $slow, $info, WEXITED | WNOWAIT));
var_dump($info['pid'] === $slow && $info['status'] === 27);
var_dump(pcntl_waitpid(-$slow, $status) === $slow);
var_dump(pcntl_wexitstatus($status) === 27);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
