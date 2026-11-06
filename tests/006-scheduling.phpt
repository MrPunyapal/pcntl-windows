--TEST--
Windows PCNTL priority and affinity
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !extension_loaded('pcntl')) die('skip native Windows extension required'); ?>
--FILE--
<?php
var_dump(is_int(pcntl_getpriority()));
var_dump(pcntl_setpriority(0));
var_dump(pcntl_getpriority() === 0);
$cpus = pcntl_getcpuaffinity();
var_dump(is_array($cpus) && count($cpus) > 0);
var_dump(pcntl_setcpuaffinity(null, $cpus));
var_dump(pcntl_getcpuaffinity() === $cpus);
var_dump(is_int(pcntl_getcpu()));
var_dump(pcntl_wifcontinued(0xffff));
var_dump(!pcntl_wifcontinued(0));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
