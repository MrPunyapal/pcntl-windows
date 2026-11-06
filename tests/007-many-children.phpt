--TEST--
More than 64 child handles can be spawned and waited on
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$pids = [];
for ($i = 0; $i < 66; $i++) {
    $pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(0);']);
    if ($pid === false) {
        echo "spawn-failed\n";
        exit;
    }
    $pids[] = $pid;
}
var_dump(count($pids) === 66);
$count = 0;
while (($pid = pcntl_wait($status)) > 0) {
    if (pcntl_wifexited($status) && pcntl_wexitstatus($status) === 0) $count++;
}
var_dump($count === 66);
var_dump(pcntl_errno() === PCNTL_ECHILD);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
