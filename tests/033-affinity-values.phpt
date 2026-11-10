--TEST--
CPU affinity accepts referenced integers and canonical integer strings
--SKIPIF--
<?php if (!function_exists('pcntl_setcpuaffinity')) die('skip CPU affinity required'); ?>
--FILE--
<?php
$original = pcntl_getcpuaffinity();
try {
    $cpu = $original[0];
    var_dump(pcntl_setcpuaffinity(null, [&$cpu]));
    var_dump(pcntl_getcpuaffinity() === [$cpu]);
    var_dump(pcntl_setcpuaffinity(null, [(string) $cpu]));
    var_dump(pcntl_getcpuaffinity() === [$cpu]);
    foreach (['1.0', '01', 'cpu', '999999999999999999999999999999'] as $invalid) {
        try { pcntl_setcpuaffinity(null, [$invalid]); echo "bad\n"; }
        catch (ValueError) { echo "invalid\n"; }
    }
    foreach ([false, 1.0, []] as $invalid) {
        try { pcntl_setcpuaffinity(null, [$invalid]); echo "bad\n"; }
        catch (TypeError) { echo "type\n"; }
    }
} finally {
    pcntl_setcpuaffinity(null, $original);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
invalid
invalid
invalid
invalid
type
type
type
