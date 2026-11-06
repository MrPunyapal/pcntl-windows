--TEST--
Windows PCNTL module loads without pretending fork works
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !extension_loaded('pcntl')) die('skip Windows extension required'); ?>
--FILE--
<?php
var_dump(extension_loaded('pcntl'));
var_dump(function_exists('pcntl_windows_spawn'));
var_dump(function_exists('pcntl_fork'));
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
