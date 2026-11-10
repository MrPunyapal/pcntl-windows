<?php
declare(strict_types=1);

$valid = getmypid() === (int) ($argv[1] ?? -1)
    && getcwd() === ($argv[2] ?? null)
    && ($argv[3] ?? null) === 'argument with spaces'
    && ($argv[4] ?? null) === '--leading-dashes'
    && array_key_exists(5, $argv) && $argv[5] === ''
    && count($argv) === 6
    && getenv('PCNTL_CONFORMANCE_EXEC_MARKER') === 'child-only-value';

if (!$valid) {
    fwrite(STDERR, 'Exec environment/identity check failed: '.json_encode([
        'pid' => getmypid(),
        'cwd' => getcwd(),
        'argv' => $argv,
        'marker' => getenv('PCNTL_CONFORMANCE_EXEC_MARKER'),
    ])."\n");
}
exit($valid ? 41 : 72);
