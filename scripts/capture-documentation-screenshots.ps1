[CmdletBinding()]
param(
    [string] $BuildDirectory = 'build/Debug',
    [string] $OutputDirectory = 'docs/screenshots',
    [ValidateSet('en', 'zh-HK', 'bilingual')]
    [string] $Language = 'en'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildPath = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $BuildDirectory))
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputDirectory))
$executable = Join-Path $buildPath 'WimForge.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Build WimForge first. The desktop executable was not found at $executable"
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

$pages = [ordered] @{
    overview = 'overview.png'; source = 'source.png'; customize = 'customize.png'
    gpo = 'group-policy.png'; unattended = 'unattended.png'; packages = 'package-studio.png'
    winforge = 'winforge-bridge.png'; plan = 'review-run.png'; history = 'history.png'
    settings = 'settings.png'
}
$originalEnvironment = @{
    TEMP = $env:TEMP; TMP = $env:TMP; QT_SCALE_FACTOR = $env:QT_SCALE_FACTOR
    QT_SCREEN_SCALE_FACTORS = $env:QT_SCREEN_SCALE_FACTORS
    LOCALAPPDATA = $env:LOCALAPPDATA; APPDATA = $env:APPDATA
    WIMFORGE_NOTIFICATION_STORE = $env:WIMFORGE_NOTIFICATION_STORE
}

try {
    $publicRoot = if ([string]::IsNullOrWhiteSpace($env:PUBLIC)) {
        [Environment]::GetFolderPath([Environment+SpecialFolder]::CommonDocuments)
    } else { Join-Path $env:PUBLIC 'Documents' }
    $neutralRoot = [System.IO.Path]::GetFullPath((Join-Path $publicRoot 'WimForge-Screenshot'))
    New-Item -ItemType Directory -Path $neutralRoot -Force | Out-Null
    $env:QT_SCALE_FACTOR = '1'; $env:QT_SCREEN_SCALE_FACTORS = '1'

    foreach ($entry in $pages.GetEnumerator()) {
        $routeRoot = [System.IO.Path]::GetFullPath((Join-Path $neutralRoot $entry.Key))
        if (-not $routeRoot.StartsWith($neutralRoot + [System.IO.Path]::DirectorySeparatorChar,
                                       [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe screenshot fixture path: $routeRoot"
        }
        if (Test-Path -LiteralPath $routeRoot) {
            Remove-Item -LiteralPath $routeRoot -Recurse -Force
        }
        $routeTemp = Join-Path $routeRoot 'Temp'
        $routeLocal = Join-Path $routeRoot 'LocalAppData'
        $routeRoaming = Join-Path $routeRoot 'AppData'
        New-Item -ItemType Directory -Path $routeTemp, $routeLocal, $routeRoaming -Force | Out-Null
        $env:TEMP = $routeTemp; $env:TMP = $routeTemp
        $env:LOCALAPPDATA = $routeLocal; $env:APPDATA = $routeRoaming
        $env:WIMFORGE_NOTIFICATION_STORE = Join-Path $routeRoot 'NotificationStore'

        $destination = Join-Path $outputPath $entry.Value
        $process = Start-Process -FilePath $executable `
            -ArgumentList @('--demo', '--language', $Language, '--page', $entry.Key,
                            '--screenshot', $destination) `
            -Wait -PassThru -WindowStyle Hidden
        if ($process.ExitCode -ne 0) {
            throw "Screenshot capture failed for route '$($entry.Key)' with exit code $($process.ExitCode)"
        }
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            throw "Screenshot capture did not create $destination"
        }
    }
}
finally {
    foreach ($name in $originalEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $originalEnvironment[$name], 'Process')
    }
}

Add-Type -AssemblyName System.Drawing
$captures = foreach ($entry in $pages.GetEnumerator()) {
    $path = Join-Path $outputPath $entry.Value
    $image = [System.Drawing.Image]::FromFile($path)
    try {
        [pscustomobject] @{ Route = $entry.Key; File = $entry.Value; Width = $image.Width
            Height = $image.Height; Bytes = (Get-Item -LiteralPath $path).Length }
    } finally { $image.Dispose() }
}
$dimensions = @($captures | ForEach-Object { "$($_.Width)x$($_.Height)" } | Sort-Object -Unique)
if ($dimensions.Count -ne 1) {
    throw "Documentation screenshots have inconsistent dimensions: $($dimensions -join ', ')"
}
$captures | Format-Table -AutoSize
