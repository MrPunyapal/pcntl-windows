<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli' || !function_exists('pcntl_fork')) {
    fwrite(STDERR, "POSIX PHP CLI with pcntl_fork is required\n");
    exit(77);
}

if (!function_exists('stream_socket_pair') || !defined('STREAM_PF_UNIX')) {
    fwrite(STDERR, "Unix socket pairs are unavailable; test cannot verify socket inheritance\n");
    exit(77);
}

$sockets = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, 0);
if ($sockets === false) {
    throw new RuntimeException('Unable to create a Unix socket pair before fork');
}
foreach ($sockets as $socket) {
    stream_set_timeout($socket, 5);
}
$pid = pcntl_fork();
if ($pid === -1) {
    throw new RuntimeException('Fork failed: '.pcntl_strerror(pcntl_errno()));
}
if ($pid === 0) {
    fclose($sockets[0]);
    $request = fread($sockets[1], 4);
    $received = $request === 'ping';
    $sent = fwrite($sockets[1], 'pong') === 4;
    fclose($sockets[1]);
    exit($received && $sent ? 29 : 70);
}
fclose($sockets[1]);
$sent = fwrite($sockets[0], 'ping') === 4;
$response = fread($sockets[0], 4);
$metadata = stream_get_meta_data($sockets[0]);
fclose($sockets[0]);
$waited = pcntl_waitpid($pid, $status);
if (!$sent || $response !== 'pong' || $metadata['timed_out']
    || $waited !== $pid || !pcntl_wifexited($status)
    || pcntl_wexitstatus($status) !== 29) {
    throw new RuntimeException('Inherited socket communication or child status failed');
}
echo "Inherited Unix socket pair communication passed\n";
