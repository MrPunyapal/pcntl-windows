# Run an entire PHP script in Linux under WSL2 with real ext/pcntl.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string] $Script,
    [string] $Distribution,
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
$path = & wsl.exe @($prefix + @('--exec', 'wslpath', '-u', $source))
if ($LASTEXITCODE -ne 0 -or -not $path) {
    Write-Error 'Unable to resolve PHP script inside WSL.'
    exit 2
}
$cwd = & wsl.exe @($prefix + @('--exec', 'wslpath', '-u', $working))
if ($LASTEXITCODE -ne 0 -or -not $cwd) {
    Write-Error 'Unable to resolve working directory inside WSL.'
    exit 2
}
$path = ($path | Select-Object -First 1).Trim()
$cwd = ($cwd | Select-Object -First 1).Trim()
$probe = 'if (!extension_loaded("pcntl")) { fwrite(STDERR, "Linux PHP PCNTL is not loaded.\n"); exit(78); }'
& wsl.exe @($prefix + @('--cd', $cwd, '--exec', 'php', '-r', $probe))
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
# --exec invokes Linux PHP directly; script arguments are not shell-evaluated.
& wsl.exe @($prefix + @('--cd', $cwd, '--exec', 'php', $path) + $PhpArguments)
exit $LASTEXITCODE
