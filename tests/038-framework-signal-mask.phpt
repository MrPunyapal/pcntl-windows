--TEST--
Cooperative SIGQUIT and SIGCONT support signal masking and async dispatch
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_raise')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
$received = [];
foreach ([SIGQUIT, SIGCONT] as $signal) {
    pcntl_signal($signal, static function (int $signum) use (&$received): void {
        $received[] = $signum;
    });
}
pcntl_async_signals(false);
if (!pcntl_sigprocmask(SIG_BLOCK, [SIGCONT], $previous) || $previous !== []) {
    throw new RuntimeException('Could not block SIGCONT');
}
pcntl_windows_raise(SIGCONT);
pcntl_windows_raise(SIGQUIT);
pcntl_signal_dispatch();
if ($received !== [SIGQUIT]) {
    throw new RuntimeException('Masked SIGCONT was delivered before unblocking');
}
if (!pcntl_sigprocmask(SIG_UNBLOCK, [SIGCONT], $blocked) || $blocked !== [SIGCONT]) {
    throw new RuntimeException('Could not unblock SIGCONT');
}
pcntl_signal_dispatch();
if ($received !== [SIGQUIT, SIGCONT]) {
    throw new RuntimeException('Pending SIGCONT not delivered after unblocking');
}
pcntl_async_signals(true);
pcntl_windows_raise(SIGQUIT);
$deadline = microtime(true) + 2;
while (count($received) < 3 && microtime(true) < $deadline) {}
pcntl_async_signals(false);
if ($received !== [SIGQUIT, SIGCONT, SIGQUIT]) {
    throw new RuntimeException('Async SIGQUIT callback was not delivered');
}
echo "Framework cooperative masking and async signals passed\n";
?>
--EXPECT--
Framework cooperative masking and async signals passed
