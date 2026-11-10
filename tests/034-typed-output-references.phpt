--TEST--
PCNTL by-reference outputs preserve typed property constraints
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !function_exists('pcntl_windows_spawn')) die('skip native Windows PCNTL'); ?>
--FILE--
<?php
class Outputs {
    public array $status = [];
    public int $info = 123;
    public array $validInfo = [];
}
$out = new Outputs;
$pid = pcntl_windows_spawn(PHP_BINARY, ['-n', '-r', 'exit(17);']);
if ($pid === false) throw new RuntimeException('spawn');
try { pcntl_waitpid($pid, $out->status); echo "bad\n"; }
catch (TypeError) { echo "status type\n"; }
var_dump($out->status === []);
try { pcntl_waitid(P_PID, $pid, $out->info, WEXITED | WNOWAIT); echo "bad\n"; }
catch (TypeError) { echo "waitid type\n"; }
try { pcntl_waitpid($pid, $status, 0, $out->info); echo "bad\n"; }
catch (TypeError) { echo "usage type\n"; }
// Rejected output assignments must not discard the registered child handle.
var_dump(pcntl_waitpid($pid, $status) === $pid && pcntl_wexitstatus($status) === 17);
try { pcntl_sigprocmask(SIG_BLOCK, [SIGUSR1], $out->info); echo "bad\n"; }
catch (TypeError) { echo "mask type\n"; }
pcntl_windows_raise(SIGUSR1);
try { pcntl_sigtimedwait([SIGUSR1], $out->info, 1); echo "bad\n"; }
catch (TypeError) { echo "signal type\n"; }
var_dump($out->info === 123);
pcntl_windows_raise(SIGUSR1);
var_dump(pcntl_sigtimedwait([SIGUSR1], $out->validInfo, 1) === SIGUSR1);
var_dump($out->validInfo['signo'] === SIGUSR1);
pcntl_sigprocmask(SIG_SETMASK, []);
?>
--EXPECT--
status type
bool(true)
waitid type
usage type
bool(true)
mask type
signal type
bool(true)
bool(true)
bool(true)
