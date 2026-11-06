<?php
declare(strict_types=1);

if (PHP_OS_FAMILY !== 'Darwin') {
    fwrite(STDERR, "Darwin is required\n");
    exit(77);
}
if (!function_exists('pcntl_getqos_class') || !function_exists('pcntl_setqos_class')) {
    throw new RuntimeException('Darwin PCNTL QoS is not available in this PHP build');
}
$original = pcntl_getqos_class();
if (!$original instanceof Pcntl\QosClass) {
    throw new RuntimeException('Unexpected original QoS value');
}
try {
    pcntl_setqos_class(Pcntl\QosClass::Utility);
    if (pcntl_getqos_class() !== Pcntl\QosClass::Utility) {
        throw new RuntimeException('Darwin QoS change did not round-trip');
    }
} finally {
    pcntl_setqos_class($original);
}
echo "Darwin QoS conformance passed\n";
