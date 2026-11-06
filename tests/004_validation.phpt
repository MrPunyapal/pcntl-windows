--TEST--
Windows process wait validates process IDs and timeouts
--SKIPIF--
<?php if (!extension_loaded('pcntl_windows')) die('skip pcntl_windows not loaded'); ?>
--FILE--
<?php
foreach ([[0, 0], [-1, 0], [42, -2]] as [$pid, $timeout]) {
    try {
        pcntl_windows_wait_process($pid, $timeout);
        echo "unexpected success\n";
    } catch (ValueError $error) {
        echo "ValueError\n";
    }
}
?>
--EXPECT--
ValueError
ValueError
ValueError
