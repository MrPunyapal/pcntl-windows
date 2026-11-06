--TEST--
Pcntl QosClass enum and Windows thread scheduler mapping
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_getqos_class')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
var_dump(pcntl_getqos_class() instanceof Pcntl\QosClass);
pcntl_setqos_class(Pcntl\QosClass::Utility);
var_dump(pcntl_getqos_class() === Pcntl\QosClass::Utility);
pcntl_setqos_class(Pcntl\QosClass::UserInitiated);
var_dump(pcntl_getqos_class() === Pcntl\QosClass::UserInitiated);
pcntl_setqos_class();
var_dump(pcntl_getqos_class() === Pcntl\QosClass::Default);
var_dump(count(Pcntl\QosClass::cases()) === 5);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
