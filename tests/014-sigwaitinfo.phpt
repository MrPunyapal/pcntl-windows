--TEST--
sigwaitinfo consumes the cooperative SIGALRM event
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_sigwaitinfo')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
pcntl_alarm(1);
$result = pcntl_sigwaitinfo([SIGALRM], $info);
var_dump($result === SIGALRM);
var_dump($info['signo'] === SIGALRM);
var_dump(is_int($info['pid']));
var_dump(pcntl_alarm(0));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
int(0)
