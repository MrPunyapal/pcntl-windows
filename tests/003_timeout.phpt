--TEST--
Nonblocking process wait reports timeout separately from Win32 failure
--SKIPIF--
<?php if (!extension_loaded('pcntl_windows') || PHP_OS_FAMILY !== 'Windows') die('skip Windows extension required'); ?>
--FILE--
<?php
$process = proc_open([PHP_BINARY, '-r', 'sleep(2);'], [
    0 => ['pipe', 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
], $pipes);
if (!is_resource($process)) {
    die('proc_open failed');
}
$pid = proc_get_status($process)['pid'];
var_dump(pcntl_windows_wait_process($pid, 0));
foreach ($pipes as $pipe) {
    fclose($pipe);
}
proc_terminate($process);
proc_close($process);
?>
--EXPECT--
NULL
