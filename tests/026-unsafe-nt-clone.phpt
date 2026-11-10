--TEST--
UNSAFE Windows native NT cloning resumes Zend VM with copied PHP heap
--SKIPIF--
<?php if (PHP_OS_FAMILY !== 'Windows'
    || getenv('PCNTL_UNSAFE_CLONE_TEST') !== '1'
    || !function_exists('pcntl_windows_clone_experimental')) die('skip opt-in NT PHP clone experiment'); ?>
--FILE--
<?php
$original = ['marker' => 113];
$parentPid = getmypid();
$isCloneChild = false;
register_shutdown_function(function () use (&$isCloneChild) {
    if ($isCloneChild) pcntl_windows_clone_checkpoint_experimental(4);
});
$pid = pcntl_windows_clone_experimental();
if ($pid === 0) {
    pcntl_windows_clone_checkpoint_experimental(0);
    $isCloneChild = true;
    if ($original['marker'] !== 113 || getmypid() === $parentPid) exit(82);
    pcntl_windows_clone_checkpoint_experimental(1);
    $original['marker'] = 227;
    pcntl_windows_clone_checkpoint_experimental(2);
    pcntl_windows_clone_checkpoint_experimental(3);
    exit(73);
}
if ($pid <= 0) {
    echo "clone-failed\n";
    exit(1);
}
$until = microtime(true) + 8;
while (true) {
    $waited = pcntl_waitpid($pid, $status, WNOHANG);
    if ($waited !== 0 || microtime(true) >= $until) break;
    usleep(10000);
}
if ($waited === 0) {
    pcntl_windows_terminate($pid, 99);
    pcntl_waitpid($pid, $status);
    echo "child-hung\n";
    exit(1);
}
var_dump($waited === $pid);
var_dump(pcntl_wifexited($status));
var_dump(pcntl_wexitstatus($status) === 73);
var_dump($original['marker'] === 113);
?>
--EXPECT--
nt-clone-child-reached=1
nt-clone-checkpoint-0=1
nt-clone-checkpoint-1=1
nt-clone-checkpoint-2=1
nt-clone-checkpoint-3=1
nt-clone-checkpoint-4=1
bool(true)
bool(true)
bool(true)
bool(true)
