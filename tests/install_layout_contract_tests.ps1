[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $CMakeExecutable,

    [Parameter(Mandatory = $true)]
    [string] $BuildDirectory,

    [AllowEmptyString()]
    [ValidateSet('', 'Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string] $Configuration = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$buildPath = [IO.Path]::GetFullPath($BuildDirectory)
if (-not (Test-Path -LiteralPath (Join-Path $buildPath 'CMakeCache.txt') -PathType Leaf)) {
    throw "Build directory is not configured: $buildPath"
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'wimforge-install-layout-tests-{0}' -f [Guid]::NewGuid().ToString('N'))
$installRoot = Join-Path $testRoot 'runtime'

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    $installArguments = @('--install', $buildPath)
    if (-not [string]::IsNullOrWhiteSpace($Configuration)) {
        $installArguments += @('--config', $Configuration)
    }
    $installArguments += @('--prefix', 'runtime')
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        Push-Location -LiteralPath $testRoot
        try {
            $installOutput = @(& $CMakeExecutable @installArguments 2>&1)
            $installExitCode = $LASTEXITCODE
        } finally {
            Pop-Location
        }
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    if ($installExitCode -ne 0) {
        throw "CMake install failed:`n$($installOutput -join [Environment]::NewLine)"
    }

    $binPath = Join-Path $installRoot 'bin'
    $guiPath = Join-Path $binPath 'WimForge.exe'
    $cliPath = Join-Path $binPath 'WimForgeCli.exe'
    foreach ($requiredPath in @($guiPath, $cliPath, (Join-Path $binPath 'qt.conf'))) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Installed runtime file is missing: $requiredPath"
        }
    }

    if (Test-Path -LiteralPath (Join-Path $installRoot 'WimForge.exe') -PathType Leaf) {
        throw 'WimForge.exe was installed outside bin.'
    }
    if (Test-Path -LiteralPath (Join-Path $installRoot 'WimForgeCli.exe') -PathType Leaf) {
        throw 'WimForgeCli.exe was installed outside bin.'
    }

    $requiredQtFamilies = @('Qt6Core', 'Qt6QuickControls2')
    foreach ($family in $requiredQtFamilies) {
        $matches = @(Get-ChildItem -LiteralPath $binPath -File -Filter "$family*.dll")
        if ($matches.Count -eq 0) {
            throw "Deployed Qt runtime is missing $family."
        }
    }

    $platformPlugins = @(Get-ChildItem -LiteralPath (Join-Path $installRoot 'plugins\platforms') `
        -File -Filter 'qwindows*.dll')
    if ($platformPlugins.Count -eq 0) {
        throw 'The installed runtime is missing the Windows platform plugin.'
    }

    $ErrorActionPreference = 'Continue'
    try {
        $cliOutput = @(& $cliPath help 2>&1)
        $cliExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    if ($cliExitCode -ne 0) {
        throw "The installed CLI did not start:`n$($cliOutput -join [Environment]::NewLine)"
    }

    $configurationLabel = if ([string]::IsNullOrWhiteSpace($Configuration)) {
        'default'
    } else {
        $Configuration
    }
    Write-Host "install_layout_contract_tests: PASS ($configurationLabel, relative prefix)"
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
