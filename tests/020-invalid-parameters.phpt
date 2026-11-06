--TEST--
Invalid Windows PCNTL scheduling and signal parameters fail explicitly
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows' || !extension_loaded('pcntl')) die('skip native Windows PCNTL required'); ?>
--FILE--
<?php
try { pcntl_setpriority(999); echo "wrong\n"; } catch (ValueError) { echo "priority\n"; }
try { pcntl_setcpuaffinity(null, []); echo "wrong\n"; } catch (ValueError) { echo "affinity\n"; }
try { pcntl_sigprocmask(99, []); echo "wrong\n"; } catch (ValueError) { echo "mask\n"; }
try { pcntl_sigtimedwait([SIGUSR1], $info, 0, 1000000000); echo "wrong\n"; } catch (ValueError) { echo "timeout\n"; }
var_dump(!pcntl_signal(999, SIG_IGN));
var_dump(pcntl_errno() === PCNTL_EINVAL);
?>
--EXPECT--
priority
affinity
mask
timeout
bool(true)
bool(true)
