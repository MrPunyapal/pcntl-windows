# Deterministic wrapper contract test without needing a running WSL2 VM.
# A global command named wsl.exe intercepts the exact PowerShell invocations.
$ErrorActionPreference = 'Stop'
$global:WslMockPhase = 0

function global:wsl.exe {
    $a = @($args)
    $global:LASTEXITCODE = 0

    switch ($global:WslMockPhase) {
        0 {
            if ($a.Count -ne 6 -or $a[0] -ne '-d' -or $a[1] -ne 'Ubuntu-Test' -or
                $a[2] -ne '--exec' -or $a[3] -ne 'wslpath' -or $a[4] -ne '-u' -or
                -not $a[5].EndsWith('fork-exec.php')) {
                throw "Unexpected WSL script translation arguments: $($a -join '|')"
            }
            $global:WslMockPhase = 1
            Write-Output '/mnt/c/mock script.php'
        }
        1 {
            if ($a.Count -ne 6 -or $a[0] -ne '-d' -or $a[1] -ne 'Ubuntu-Test' -or
                $a[2] -ne '--exec' -or $a[3] -ne 'wslpath' -or $a[4] -ne '-u') {
                throw "Unexpected WSL working-directory translation: $($a -join '|')"
            }
            $global:WslMockPhase = 2
            Write-Output '/mnt/c/mock cwd'
        }
        2 {
            if ($a.Count -ne 8 -or $a[0] -ne '-d' -or $a[1] -ne 'Ubuntu-Test' -or
                $a[2] -ne '--cd' -or $a[3] -ne '/mnt/c/mock cwd' -or
                $a[4] -ne '--exec' -or $a[5] -ne '/usr/bin/php' -or
                $a[6] -ne '-r' -or $a[7] -notmatch 'pcntl_setns') {
                throw "Unexpected WSL PCNTL capability probe: $($a -join '|')"
            }
            $global:WslMockPhase = 3
        }
        3 {
            if ($a.Count -ne 9 -or $a[0] -ne '-d' -or $a[1] -ne 'Ubuntu-Test' -or
                $a[2] -ne '--cd' -or $a[3] -ne '/mnt/c/mock cwd' -or
                $a[4] -ne '--exec' -or $a[5] -ne '/usr/bin/php' -or
                $a[6] -ne '/mnt/c/mock script.php' -or
                $a[7] -ne '--message' -or $a[8] -ne 'two words') {
                throw "Unexpected WSL PHP execution arguments: $($a -join '|')"
            }
            $global:WslMockPhase = 4
        }
        default { throw "Unexpected extra WSL command: $($a -join '|')" }
    }
}

$launcher = Join-Path $PSScriptRoot 'Invoke-WslPhp.ps1'
$script = Join-Path $PSScriptRoot '../tests/reference/fork-exec.php'
# The launcher terminates with the child's exit code; all four mock calls must
# be verified inside the function before execution reaches that exit.
& $launcher -Script $script -Distribution 'Ubuntu-Test' -LinuxPhp '/usr/bin/php' -RequireSetns -PhpArguments @('--message', 'two words')
