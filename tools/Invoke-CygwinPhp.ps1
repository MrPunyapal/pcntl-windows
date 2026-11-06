# Launch a Cygwin-linked PHP binary with real Cygwin PCNTL.
# This does not add POSIX fork to native MSVC-compiled Windows PHP.
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string] $Script,

    [ValidateNotNullOrEmpty()]
    [string] $CygwinRoot = 'C:\cygwin64',

    [string] $PhpBinary,

    [string[]] $PhpArguments = @()
)

$ErrorActionPreference = 'Stop'
$cygpath = Join-Path $CygwinRoot 'bin\cygpath.exe'
if (-not (Test-Path -LiteralPath $cygpath -PathType Leaf)) {
    Write-Error "Cygwin installation not found at $CygwinRoot. Install Cygwin first."
    exit 127
}

if (-not $PhpBinary) {
    $options = @(
        (Join-Path $CygwinRoot 'usr\local\bin\php.exe'),
        (Join-Path $CygwinRoot 'bin\php.exe')
    )
    $PhpBinary = $options | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $PhpBinary -or -not (Test-Path -LiteralPath $PhpBinary -PathType Leaf)) {
    Write-Error 'Cygwin-linked PHP is not installed. Build PHP under Cygwin with --enable-pcntl or provide -PhpBinary.'
    exit 127
}

try {
    $scriptPath = (Resolve-Path -LiteralPath $Script -ErrorAction Stop).ProviderPath
} catch {
    Write-Error "Cannot resolve PHP script: $Script"
    exit 2
}

$convertedPath = & $cygpath -u $scriptPath
if ($LASTEXITCODE -ne 0 -or -not $convertedPath) {
    Write-Error 'Unable to convert PHP script path into Cygwin POSIX path.'
    exit 2
}
$convertedPath = ($convertedPath | Select-Object -First 1).Trim()
$originalPath = $env:PATH
try {
    # Ensure a custom Cygwin PHP executable resolves its cygwin1.dll dependencies.
    $env:PATH = "$(Join-Path $CygwinRoot 'bin');$originalPath"
    $probe = 'if (!extension_loaded("pcntl") || !function_exists("pcntl_fork")) { fwrite(STDERR, "Cygwin PHP PCNTL missing.\n"); exit(78); }'
    & $PhpBinary -r $probe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $PhpBinary $convertedPath @PhpArguments
    exit $LASTEXITCODE
}
finally {
    $env:PATH = $originalPath
}
