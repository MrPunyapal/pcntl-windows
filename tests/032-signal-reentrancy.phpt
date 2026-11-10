--TEST--
Signal callbacks cannot recursively enter dispatch and exceptions release the guard
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
$events = [];
pcntl_signal(SIGUSR1, function () use (&$events) {
    $events[] = 'enter';
    if (count($events) === 1) {
        pcntl_windows_raise(SIGUSR1);
        pcntl_signal_dispatch();
    }
    $events[] = 'leave';
});
pcntl_windows_raise(SIGUSR1);
pcntl_signal_dispatch();
var_dump($events === ['enter', 'leave']);
pcntl_signal_dispatch();
var_dump($events === ['enter', 'leave', 'enter', 'leave']);
pcntl_signal(SIGUSR1, function () { throw new RuntimeException('handler'); });
pcntl_windows_raise(SIGUSR1);
try { pcntl_signal_dispatch(); } catch (RuntimeException $e) { echo $e->getMessage(), "\n"; }
pcntl_signal(SIGUSR1, function () { echo "recovered\n"; });
pcntl_windows_raise(SIGUSR1);
pcntl_signal_dispatch();
?>
--EXPECT--
bool(true)
bool(true)
handler
recovered
