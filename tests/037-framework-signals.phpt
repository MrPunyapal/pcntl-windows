--TEST--
Symfony Console and Laravel worker signal numbers are registerable in the Windows cooperative queue
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$consoleSignals = [SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2, SIGALRM];
$workerSignals = [SIGQUIT, SIGTERM, SIGINT, SIGUSR2, SIGCONT];
$events = [];

foreach (array_unique(array_merge($consoleSignals, $workerSignals)) as $signal) {
    if (!pcntl_signal($signal, static function (int $received) use (&$events): void {
        $events[] = $received;
    })) {
        throw new RuntimeException("Unable to register signal $signal");
    }
    if (!is_callable(pcntl_signal_get_handler($signal))) {
        throw new RuntimeException("Signal $signal handler was not registered");
    }
}

pcntl_async_signals(false);
if (!pcntl_windows_raise(SIGQUIT) || !pcntl_windows_raise(SIGCONT)) {
    throw new RuntimeException('Unable to enqueue cooperative framework signals');
}
pcntl_signal_dispatch();
if ($events !== [SIGQUIT, SIGCONT]) {
    throw new RuntimeException('SIGQUIT/SIGCONT cooperative callbacks were not delivered');
}
echo "Framework signal registration and cooperative dispatch passed\n";
?>
--EXPECT--
Framework signal registration and cooperative dispatch passed
