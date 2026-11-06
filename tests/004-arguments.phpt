--TEST--
Windows argument quoting round trip
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip Windows extension required'); ?>
--FILE--
<?php
$script = <<<'PHP'
if ($argv[1] !== 'space here' || $argv[2] !== 'quote"here' || $argv[3] !== 'trailing\\') exit(7);
PHP;
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', $script, '--', 'space here', 'quote"here', 'trailing\\']);
pcntl_waitpid($pid, $status);
var_dump(pcntl_wexitstatus($status));
?>
--EXPECT--
int(0)
