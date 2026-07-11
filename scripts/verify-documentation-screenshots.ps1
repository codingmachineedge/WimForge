[CmdletBinding()]
param(
    [string] $ScreenshotDirectory = 'docs/screenshots'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$screenshotPath = [System.IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot $ScreenshotDirectory))
if (-not $screenshotPath.StartsWith(
        $repositoryRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Screenshot directory must stay inside the repository. / 截圖資料夾一定要喺 repo 入面：$screenshotPath"
}
if (-not (Test-Path -LiteralPath $screenshotPath -PathType Container)) {
    throw "Screenshot directory does not exist. / 截圖資料夾唔存在：$screenshotPath"
}

$expected = [ordered] @{
    'project-start.png' = @(1440, 900)
    'overview.png' = @(1440, 900)
    'source.png' = @(1440, 900)
    'customize.png' = @(1440, 900)
    'group-policy.png' = @(1440, 900)
    'unattended.png' = @(1440, 900)
    'package-studio.png' = @(1440, 900)
    'winforge-bridge.png' = @(1440, 900)
    'virtual-machine-lab.png' = @(1440, 900)
    'review-run.png' = @(1440, 900)
    'history.png' = @(1440, 900)
    'settings.png' = @(1440, 900)
    'embedded-terminal.png' = @(1440, 900)
    'site-home-desktop.png' = @(1280, 720)
    'site-home-mobile.png' = @(390, 844)
}

$actualNames = @(Get-ChildItem -LiteralPath $screenshotPath -File -Filter '*.png' |
    Select-Object -ExpandProperty Name)
$missing = @($expected.Keys | Where-Object { $_ -notin $actualNames })
$unexpected = @($actualNames | Where-Object { $_ -notin $expected.Keys })
if ($missing.Count -ne 0 -or $unexpected.Count -ne 0) {
    throw "Screenshot set mismatch / 截圖集合唔一致。Missing / 缺少: $($missing -join ', '); Unexpected / 多咗: $($unexpected -join ', ')"
}

Add-Type -AssemblyName System.Drawing
$pngSignature = '137,80,78,71,13,10,26,10'
$results = foreach ($name in $expected.Keys) {
    $path = Join-Path $screenshotPath $name
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 8 -or (($bytes[0..7] -join ',') -ne $pngSignature)) {
        throw "Screenshot is not a true PNG / 截圖唔係真正 PNG：$name"
    }

    $image = [System.Drawing.Image]::FromFile($path)
    try {
        $requiredWidth = $expected[$name][0]
        $requiredHeight = $expected[$name][1]
        if ($image.Width -ne $requiredWidth -or $image.Height -ne $requiredHeight) {
            throw "Wrong screenshot dimensions / 截圖尺寸唔啱：$name is $($image.Width)x$($image.Height), expected $($requiredWidth)x$requiredHeight"
        }
        [pscustomobject] @{
            File = $name
            Dimensions = "$($image.Width)x$($image.Height)"
            Bytes = $bytes.Length
            Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } finally {
        $image.Dispose()
    }
}

$results | Format-Table -AutoSize
Write-Host "Verified all $($expected.Count) documentation screenshots. / 已核對全部 $($expected.Count) 張文件截圖。"
