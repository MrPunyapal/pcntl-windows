--TEST--
Windows CreateProcessW argument round trip covers empty Unicode quotes and backslashes
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
$arguments = ['', 'two words', '\\', 'a\\\"b', 'quote"last', 'trail\\\\', 'λ'];
$assert = 'if (array_slice($argv, 1) !== '.var_export($arguments, true).') exit(81);';
$pid = pcntl_windows_spawn(PHP_BINARY, array_merge(['-n', '-r', $assert, '--'], $arguments));
var_dump($pid > 0);
var_dump(pcntl_waitpid($pid, $status) === $pid);
var_dump(pcntl_wexitstatus($status) === 0);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
