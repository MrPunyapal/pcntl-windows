--TEST--
Experimental extension loads but does not claim POSIX PCNTL
--SKIPIF--
<?php if (!extension_loaded('pcntl_windows')) die('skip pcntl_windows not loaded'); ?>
--FILE--
<?php
var_dump(extension_loaded('pcntl_windows'));
var_dump(function_exists('pcntl_windows_wait_process'));
var_dump(function_exists('pcntl_windows_processor'));
var_dump(function_exists('pcntl_fork'));
$processor = pcntl_windows_processor();
var_dump(array_keys($processor));
var_dump(is_int($processor['group']));
var_dump(is_int($processor['number']));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
array(2) {
  [0]=>
  string(5) "group"
  [1]=>
  string(6) "number"
}
bool(true)
bool(true)
