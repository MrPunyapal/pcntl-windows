# Run an entire PHP script in Linux under WSL2 with real ext/pcntl.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string] $Script,
    [string] $Distribution,
    [ValidateNotNullOrEmpty()]
    [string] $LinuxPhp = 'php',
    [switch] $RequireSetns,
    [string[]] $PhpArguments = @()
)
$ErrorActionPreference = 'Stop'
if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    Write-Error 'WSL2 is not installed.'
    exit 127
}
try {
    $source = (Resolve-Path -LiteralPath $Script -ErrorAction Stop).ProviderPath
    $working = (Get-Location).ProviderPath
} catch {
    Write-Error "PHP script not found: $Script"
    exit 2
}
$prefix = @()
if (-not [string]::IsNullOrWhiteSpace($Distribution)) {
    $prefix += @('-d', $Distribution)
}
$translateScript = $prefix + @('--exec', 'wslpath', '-u', $source)
$path = & wsl.exe @translateScript
if ($LASTEXITCODE -ne 0 -or -not $path) {
    Write-Error 'Unable to resolve PHP script inside WSL.'
    exit 2
}
$translateCwd = $prefix + @('--exec', 'wslpath', '-u', $working)
$cwd = & wsl.exe @translateCwd
if ($LASTEXITCODE -ne 0 -or -not $cwd) {
    Write-Error 'Unable to resolve working directory inside WSL.'
    exit 2
}
$path = ($path | Select-Object -First 1).Trim()
$cwd = ($cwd | Select-Object -First 1).Trim()

# A Windows filesystem path to a custom Linux PHP binary is translated via
# wslpath. An absolute Linux path or a PATH command (default: php) passes
# straight to wsl.exe --exec without invoking a shell.
$phpCommand = $LinuxPhp
if (Test-Path -LiteralPath $LinuxPhp -PathType Leaf) {
    $windowsPhp = (Resolve-Path -LiteralPath $LinuxPhp).ProviderPath
    $translatePhp = $prefix + @('--exec', 'wslpath', '-u', $windowsPhp)
    $mappedPhp = & wsl.exe @translatePhp
    if ($LASTEXITCODE -ne 0 -or -not $mappedPhp) {
        Write-Error 'Unable to resolve custom Linux PHP binary inside WSL.'
        exit 2
    }
    $phpCommand = ($mappedPhp | Select-Object -First 1).Trim()
}
$probe = 'if (!extension_loaded("pcntl")) { fwrite(STDERR, "Linux PHP PCNTL is not loaded.\n"); exit(78); }'
if ($RequireSetns) {
    $probe += 'if (!function_exists("pcntl_setns")) { fwrite(STDERR, "Linux PHP pcntl_setns is unavailable. See docs/LINUX-SETNS-85.md.\n"); exit(78); }'
}
$probeArgs = $prefix + @('--cd', $cwd, '--exec', $phpCommand, '-r', $probe)
& wsl.exe @probeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
# --exec invokes Linux PHP directly; script arguments are not shell-evaluated.
$executeArgs = $prefix + @('--cd', $cwd, '--exec', $phpCommand, $path) + $PhpArguments
& wsl.exe @executeArgs
exit $LASTEXITCODE
