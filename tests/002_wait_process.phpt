--TEST--
Wait for a child and report its actual 32-bit Windows exit code
--SKIPIF--
<?php if (!extension_loaded('pcntl_windows') || PHP_OS_FAMILY !== 'Windows') die('skip Windows extension required'); ?>
--FILE--
<?php
$process = proc_open([PHP_BINARY, '-r', 'usleep(200000); exit(23);'], [
    0 => ['pipe', 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
], $pipes);
if (!is_resource($process)) {
    die('proc_open failed');
}
$pid = proc_get_status($process)['pid'];
var_dump(pcntl_windows_wait_process($pid, 15000));
foreach ($pipes as $pipe) {
    fclose($pipe);
}
proc_close($process);
?>
--EXPECT--
int(23)
