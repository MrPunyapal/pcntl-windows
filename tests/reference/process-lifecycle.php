<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli' || !function_exists('pcntl_fork') || !function_exists('posix_kill')) {
    fwrite(STDERR, "POSIX CLI with pcntl and posix extensions required\n");
    exit(77);
}

function check(bool $condition, string $message): void {
    if (!$condition) throw new RuntimeException($message);
}

function eventually(callable $condition, string $message, ?callable $diagnostic = null): void {
    $deadline = microtime(true) + 15;
    do {
        if ($condition()) return;
        usleep(10_000);
    } while (microtime(true) < $deadline);
    throw new RuntimeException($message.($diagnostic ? ': '.json_encode($diagnostic(), JSON_THROW_ON_ERROR) : ''));
}

$children = [];
$originalAsync = pcntl_async_signals(false);
check(pcntl_sigprocmask(SIG_BLOCK, [SIGUSR1, SIGCHLD], $originalMask), 'read signal mask');
$usrHandler = pcntl_signal_get_handler(SIGUSR1);
$chldHandler = pcntl_signal_get_handler(SIGCHLD);
$alarmHandler = pcntl_signal_get_handler(SIGALRM);
try {
    $received = [];
    check(pcntl_signal(SIGUSR1, function (int $signal) use (&$received) { $received[] = $signal; }), 'install handler');
    check(pcntl_sigprocmask(SIG_BLOCK, [SIGUSR1]), 'block signal');
    check(posix_kill(getmypid(), SIGUSR1), 'send blocked signal');
    pcntl_signal_dispatch();
    check($received === [], 'blocked signal dispatched prematurely');
    check(pcntl_sigprocmask(SIG_UNBLOCK, [SIGUSR1]), 'unblock signal');
    eventually(function () use (&$received) {
        pcntl_signal_dispatch();
        return $received === [SIGUSR1];
    }, 'manual signal dispatch');

    // Consume a kernel-pending signal without invoking its PHP callback.
    if (function_exists('pcntl_sigtimedwait')) {
        check(pcntl_sigprocmask(SIG_BLOCK, [SIGUSR1]), 'block for sigtimedwait');
        check(posix_kill(getmypid(), SIGUSR1), 'queue sigtimedwait signal');
        check(pcntl_sigtimedwait([SIGUSR1], $info, 5) === SIGUSR1, 'sigtimedwait signal');
        check($info['signo'] === SIGUSR1 && $info['pid'] === getmypid(), 'siginfo sender');
        $unchanged = ['sentinel' => 1];
        check(pcntl_sigtimedwait([SIGUSR1], $unchanged, 0, 20_000_000) === false, 'sigtimedwait timeout');
        check($unchanged === ['sentinel' => 1], 'timeout modified siginfo');
        check(count($received) === 1, 'sigwait invoked callback');
    } else {
        echo "SKIP sigtimedwait: unavailable in this runtime\n";
    }

    pcntl_async_signals(true);
    check(pcntl_sigprocmask(SIG_UNBLOCK, [SIGUSR1]), 'unblock async signal');
    check(posix_kill(getmypid(), SIGUSR1), 'send async signal');
    eventually(function () use (&$received) { return count($received) === 2; }, 'async signal dispatch',
        function () use (&$received) {
            return ['received' => $received, 'async' => pcntl_async_signals(), 'errno' => pcntl_errno()];
        });
    $alarms = 0;
    check(pcntl_signal(SIGALRM, function () use (&$alarms) { $alarms++; }), 'alarm handler');
    pcntl_alarm(5);
    $remaining = pcntl_alarm(0);
    check($remaining > 0 && $remaining <= 5, 'alarm cancellation remaining time');
    pcntl_alarm(1);
    eventually(function () use (&$alarms) { return $alarms === 1; }, 'async alarm delivery');
    check(pcntl_alarm(0) === 0, 'expired alarm remained active');

    $sigchld = 0;
    check(pcntl_signal(SIGCHLD, function () use (&$sigchld) { $sigchld++; }), 'SIGCHLD handler');
    check(pcntl_sigprocmask(SIG_BLOCK, [SIGCHLD]), 'block SIGCHLD');
    // A socket handshake prevents the parent checking the child's group before setpgid.
    $pair = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
    check($pair !== false, 'socket pair');
    $pid = pcntl_fork();
    check($pid !== -1, 'fork group child');
    if ($pid === 0) {
        fclose($pair[0]);
        pcntl_alarm(10); // Also bounds a child whose parent fails during the handshake.
        pcntl_signal(SIGALRM, SIG_DFL);
        if (!posix_setpgid(0, 0)) exit(81);
        if (fwrite($pair[1], "ready\n") !== 6) exit(82);
        stream_set_timeout($pair[1], 10);
        if (fgets($pair[1]) !== "exit\n") exit(83);
        exit(47);
    }
    $children[$pid] = true;
    fclose($pair[1]);
    stream_set_timeout($pair[0], 10);
    check(fgets($pair[0]) === "ready\n", 'child group handshake');
    check(posix_getpgid($pid) === $pid, 'child process group');
    check(pcntl_waitpid(-$pid, $status, WNOHANG) === 0, 'live group WNOHANG');
    check(fwrite($pair[0], "exit\n") === 5, 'release child');
    fclose($pair[0]);
    if (function_exists('pcntl_waitid') && defined('WNOWAIT')) {
        eventually(function () use ($pid) {
            check(pcntl_waitid(P_PID, $pid, $info, WEXITED | WNOHANG | WNOWAIT), 'waitid');
            if (($info['pid'] ?? 0) === 0) return false;
            check($info['pid'] === $pid && $info['status'] === 47, 'waitid status');
            return true;
        }, 'waitid WNOWAIT');
    } else {
        echo "SKIP waitid: unavailable in this runtime\n";
    }
    eventually(function () use ($pid, &$children) {
        $waited = pcntl_waitpid(-$pid, $status, WNOHANG);
        check($waited !== -1, 'group wait failed');
        if ($waited === 0) return false;
        unset($children[$pid]);
        check($waited === $pid && pcntl_wifexited($status) && pcntl_wexitstatus($status) === 47, 'group exit status');
        return true;
    }, 'group child reaping');
    check(pcntl_sigprocmask(SIG_UNBLOCK, [SIGCHLD]), 'unblock SIGCHLD');
    eventually(function () use (&$sigchld) { return $sigchld > 0; }, 'async SIGCHLD');
    check(pcntl_waitpid($pid, $status, WNOHANG) === -1 && pcntl_errno() === PCNTL_ECHILD, 'double reap must fail with ECHILD');
    echo "Signals, masks, async dispatch, timers, process groups and child cleanup passed\n";
} finally {
    pcntl_async_signals(false);
    pcntl_alarm(0);
    foreach ($children as $pid => $_) {
        posix_kill($pid, SIGKILL);
        eventually(function () use ($pid) { return pcntl_waitpid($pid, $status, WNOHANG) !== 0; }, 'cleanup child');
    }
    pcntl_signal(SIGUSR1, $usrHandler);
    pcntl_signal(SIGCHLD, $chldHandler);
    pcntl_signal(SIGALRM, $alarmHandler);
    pcntl_sigprocmask(SIG_SETMASK, $originalMask);
    pcntl_async_signals($originalAsync);
}
