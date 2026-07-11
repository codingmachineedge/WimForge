[CmdletBinding()]
param(
    [ValidatePattern('^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$')]
    [string] $Version,

    [string] $RepositoryPath,

    [ValidatePattern('^6\.(8|9|[1-9]\d)\.\d+$')]
    [string] $QtVersion = '6.8.3',

    [string] $QtInstallRoot,

    [string] $LogPath,

    [switch] $SkipTests,

    [switch] $Plan,

    # Used only by the argument-safe self-elevation handoff. Supplying this
    # flag manually never bypasses the administrator-token check below.
    [switch] $Elevated,

    # Internal phase marker. The elevated copy repairs dependencies and exits;
    # source checkout, configure, build, tests, and packaging stay unelevated.
    [switch] $DependencyRepairOnly,

    # Exact package plan produced by the unelevated dependency probes. The
    # elevated child accepts no package ID outside this fixed allowlist.
    [ValidateSet(
        'Git.Git',
        'Kitware.CMake',
        'Microsoft.VisualStudio.2022.BuildTools',
        'JRSoftware.InnoSetup')]
    [string[]] $RepairPackageIds,

    # Signed protected App Installer executable resolved by the normal-token
    # parent, so over-the-shoulder UAC never depends on another profile's Appx
    # registration. The elevated child revalidates it before use.
    [string] $TrustedWingetPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$cloneUri = 'https://github.com/codingmachineedge/WimForge.git'
$minimumQtVersion = [Version]'6.8.0'
$minimumCMakeVersion = [Version]'3.24.0'
$managedWorkspaceMarker = 'WimForge bootstrap build workspace v1'
$script:stage = 'startup'
$script:transcriptStarted = $false
$knownLocalAppData = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::LocalApplicationData)
$knownProgramFiles = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::ProgramFiles)
$knownProgramFilesX86 = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::ProgramFilesX86)
$knownUserProfile = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::UserProfile)
if (-not $QtInstallRoot) {
    $QtInstallRoot = Join-Path $knownLocalAppData 'WimForge\Qt'
}
if (-not $LogPath) {
    $logName = 'bootstrap-build-{0}-{1}.log' -f `
        (Get-Date -Format 'yyyyMMdd-HHmmss'), [Guid]::NewGuid().ToString('N')
    $LogPath = Join-Path $knownLocalAppData (Join-Path 'WimForge\logs' $logName)
}
function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-SupportedHost {
    if ($env:OS -ne 'Windows_NT') {
        throw 'The bootstrap supports Windows only.'
    }
    if (-not [Environment]::Is64BitOperatingSystem) {
        throw 'The WimForge release build requires 64-bit Windows.'
    }
    if (-not [Environment]::Is64BitProcess) {
        throw 'Run the bootstrap from 64-bit Windows PowerShell; 32-bit path redirection is unsupported.'
    }
    if ($PSVersionTable.PSVersion -lt [Version]'5.1') {
        throw 'Windows PowerShell 5.1 or newer is required.'
    }
    if (-not $knownLocalAppData -or -not $knownProgramFiles -or
        -not $knownProgramFilesX86 -or -not $knownUserProfile) {
        throw 'Required Windows known folders could not be resolved safely.'
    }
}

function Set-ElevatedRepairEnvironment {
    if (-not $DependencyRepairOnly -or -not $Elevated -or
        -not (Test-IsAdministrator)) {
        throw 'The restricted elevated environment is available only to the dependency-repair child.'
    }
    $windowsRoot = Split-Path -Parent ([Environment]::SystemDirectory)
    $trustedPowerShellHome = Join-Path ([Environment]::SystemDirectory) `
        'WindowsPowerShell\v1.0'
    $env:PSModulePath = Join-Path $trustedPowerShellHome 'Modules'
    $env:Path = @(
        [Environment]::SystemDirectory,
        $windowsRoot,
        (Join-Path ([Environment]::SystemDirectory) 'Wbem'),
        $trustedPowerShellHome
    ) -join ';'
    $env:TEMP = Join-Path $windowsRoot 'Temp'
    $env:TMP = $env:TEMP
}

function Clear-GitProcessOverrides {
    # GIT_DIR, GIT_WORK_TREE, GIT_INDEX_FILE, alternate object directories,
    # and GIT_CONFIG_* can silently redirect otherwise explicit `git -C`
    # checks. This bootstrap is a terminating process, so do not restore them.
    foreach ($entry in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'GIT_*' })) {
        Remove-Item -LiteralPath ("Env:{0}" -f $entry.Name) -ErrorAction SilentlyContinue
    }
    $env:GIT_TERMINAL_PROMPT = '0'
}

function Start-ElevatedCopy {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]] $PackageIds,
        [Parameter(Mandatory)][string] $WingetPath
    )

    # The elevated process receives an encoded handoff, not reconstructed
    # user arguments. It verifies the exact source bytes observed before UAC,
    # copies them and the serialized parameters into an administrator-only
    # Windows temp directory, then runs that protected copy in a child process.
    # Minimize the elevated input surface. Repository/build/version/Qt options
    # stay exclusively in the normal-token parent.
    $payload = @{
        Elevated = $true
        DependencyRepairOnly = $true
        RepairPackageIds = @($PackageIds)
        TrustedWingetPath = $WingetPath
        LogPath = $LogPath
    }

    $payloadXml = [Management.Automation.PSSerializer]::Serialize($payload, 4)
    $encodedPayload = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($payloadXml))
    $sourceHash = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $escapedScript = $PSCommandPath.Replace("'", "''")
    $handoff = (
        "`$systemDirectory = [Environment]::SystemDirectory; " +
        "`$windowsRoot = [IO.Directory]::GetParent(`$systemDirectory).FullName; " +
        "`$trustedPowerShellHome = [IO.Path]::Combine(`$systemDirectory, 'WindowsPowerShell', 'v1.0'); " +
        "[Environment]::SetEnvironmentVariable('PSModulePath', [IO.Path]::Combine(`$trustedPowerShellHome, 'Modules'), 'Process'); " +
        "`$restrictedPath = [string]::Join(';', @(`$systemDirectory, `$windowsRoot, [IO.Path]::Combine(`$systemDirectory, 'Wbem'), `$trustedPowerShellHome)); " +
        "[Environment]::SetEnvironmentVariable('Path', `$restrictedPath, 'Process'); " +
        "[Environment]::SetEnvironmentVariable('TEMP', [IO.Path]::Combine(`$windowsRoot, 'Temp'), 'Process'); " +
        "[Environment]::SetEnvironmentVariable('TMP', [IO.Path]::Combine(`$windowsRoot, 'Temp'), 'Process'); " +
        "[IO.Directory]::SetCurrentDirectory(`$windowsRoot); " +
        "`$sourcePath = '$escapedScript'; " +
        "`$expectedHash = '$sourceHash'; " +
        "`$payloadXml = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('$encodedPayload')); " +
        "`$secureRoot = [IO.Path]::Combine(`$windowsRoot, 'Temp'); " +
        "`$secureDirectory = [IO.Path]::Combine(`$secureRoot, ('WimForge-bootstrap-' + [Guid]::NewGuid().ToString('N'))); " +
        "`$exitCode = 1; " +
        "try { " +
        "if (Test-Path -LiteralPath `$secureDirectory) { throw 'Secure elevation staging path already exists.' }; " +
        "`$acl = New-Object Security.AccessControl.DirectorySecurity; " +
        "`$acl.SetAccessRuleProtection(`$true, `$false); " +
        "`$inheritance = [Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit; " +
        "`$rights = [Security.AccessControl.FileSystemRights]::FullControl; " +
        "`$propagation = [Security.AccessControl.PropagationFlags]::None; " +
        "`$allow = [Security.AccessControl.AccessControlType]::Allow; " +
        "`$administrators = New-Object Security.Principal.SecurityIdentifier('S-1-5-32-544'); " +
        "`$system = New-Object Security.Principal.SecurityIdentifier('S-1-5-18'); " +
        "`$acl.SetOwner(`$administrators); " +
        "`$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(`$administrators, `$rights, `$inheritance, `$propagation, `$allow))); " +
        "`$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(`$system, `$rights, `$inheritance, `$propagation, `$allow))); " +
        "`$null = [IO.Directory]::CreateDirectory(`$secureDirectory, `$acl); " +
        "`$secureItem = Get-Item -LiteralPath `$secureDirectory -Force -ErrorAction Stop; " +
        "if (`$secureItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Secure elevation staging became a reparse point.' }; " +
        "`$actualAcl = `$secureItem.GetAccessControl([Security.AccessControl.AccessControlSections]::Access -bor [Security.AccessControl.AccessControlSections]::Owner); " +
        "`$actualOwner = `$actualAcl.GetOwner([Security.Principal.SecurityIdentifier]); " +
        "if (-not `$actualAcl.AreAccessRulesProtected -or -not `$actualOwner.Equals(`$administrators)) { throw 'Secure elevation staging ACL ownership is invalid.' }; " +
        "`$actualRules = @(`$actualAcl.GetAccessRules(`$true, `$false, [Security.Principal.SecurityIdentifier])); " +
        "`$unexpectedRule = `$actualRules | Where-Object { (-not `$_.IdentityReference.Equals(`$administrators) -and -not `$_.IdentityReference.Equals(`$system)) -or `$_.AccessControlType -ne `$allow -or ((`$_.FileSystemRights -band `$rights) -ne `$rights) } | Select-Object -First 1; " +
        "if (`$unexpectedRule -or `$actualRules.Count -ne 2) { throw 'Secure elevation staging ACL rules are invalid.' }; " +
        "[byte[]]`$sourceBytes = [IO.File]::ReadAllBytes(`$sourcePath); " +
        "`$sha = [Security.Cryptography.SHA256]::Create(); " +
        "try { `$actualHash = [Convert]::ToBase64String(`$sha.ComputeHash(`$sourceBytes)) } finally { `$sha.Dispose() }; " +
        "`$expectedBytes = for (`$index = 0; `$index -lt `$expectedHash.Length; `$index += 2) { [Convert]::ToByte(`$expectedHash.Substring(`$index, 2), 16) }; " +
        "if (`$actualHash -ne [Convert]::ToBase64String([byte[]]`$expectedBytes)) { throw 'Bootstrap source changed during elevation handoff.' }; " +
        "`$secureScript = Join-Path `$secureDirectory 'bootstrap-build.ps1'; " +
        "`$securePayload = Join-Path `$secureDirectory 'parameters.clixml'; " +
        "`$secureWrapper = Join-Path `$secureDirectory 'invoke.ps1'; " +
        "[IO.File]::WriteAllBytes(`$secureScript, `$sourceBytes); " +
        "[IO.File]::WriteAllText(`$securePayload, `$payloadXml, (New-Object Text.UTF8Encoding(`$false))); " +
        "`$quotedScript = `$secureScript.Replace([char]39, [string]([char]39) + [char]39); " +
        "`$quotedPayload = `$securePayload.Replace([char]39, [string]([char]39) + [char]39); " +
        "`$wrapper = '`$parameters = Import-Clixml -LiteralPath ' + [char]39 + `$quotedPayload + [char]39 + '; & ' + [char]39 + `$quotedScript + [char]39 + ' @parameters; exit `$LASTEXITCODE'; " +
        "[IO.File]::WriteAllText(`$secureWrapper, `$wrapper, (New-Object Text.UTF8Encoding(`$false))); " +
        "Set-Location -LiteralPath `$secureDirectory; " +
        "`$trustedPowerShell = Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'; " +
        "& `$trustedPowerShell -NoLogo -NoProfile -ExecutionPolicy Bypass -File `$secureWrapper; " +
        "`$exitCode = `$LASTEXITCODE " +
        "} finally { if (Test-Path -LiteralPath `$secureDirectory) { Remove-Item -LiteralPath `$secureDirectory -Recurse -Force -ErrorAction SilentlyContinue } }; " +
        "exit `$exitCode")
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($handoff))
    if ($encoded.Length -gt 30000) {
        throw 'Elevation parameters are too large for a safe Windows command-line handoff.'
    }

    Write-Host 'Administrator access is required only for WimForge dependency repair.'
    $trustedPowerShell = Join-Path ([Environment]::SystemDirectory) `
        'WindowsPowerShell\v1.0\powershell.exe'
    $trustedWorkingDirectory = Split-Path -Parent ([Environment]::SystemDirectory)
    $process = Start-Process -FilePath $trustedPowerShell -Verb RunAs -Wait -PassThru `
        -WorkingDirectory $trustedWorkingDirectory `
        -ArgumentList @('-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                        '-EncodedCommand', $encoded)
    return $process.ExitCode
}

function Assert-NoReparsePath {
    param([Parameter(Mandatory)][string] $Path)

    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing a reparse-point path: $cursor"
            }
        }
        $parent = Split-Path -Parent $cursor
        if (-not $parent -or $parent.Equals($cursor, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $cursor = $parent
    }
}

function Start-DeterministicTranscript {
    $fullLogPath = [IO.Path]::GetFullPath($LogPath)
    $logDirectory = Split-Path -Parent $fullLogPath
    Assert-NoReparsePath -Path $logDirectory
    if (-not (Test-Path -LiteralPath $logDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }
    Assert-NoReparsePath -Path $logDirectory
    if (Test-Path -LiteralPath $fullLogPath -PathType Container) {
        throw "LogPath points to a directory: $fullLogPath"
    }
    if (Test-Path -LiteralPath $fullLogPath) {
        Assert-NoReparsePath -Path $fullLogPath
        Start-Transcript -Path $fullLogPath -Append | Out-Null
    } else {
        Start-Transcript -Path $fullLogPath | Out-Null
    }
    $script:transcriptStarted = $true
    return $fullLogPath
}

function Stop-DeterministicTranscript {
    if ($script:transcriptStarted) {
        Stop-Transcript | Out-Null
        $script:transcriptStarted = $false
    }
}

function Resolve-FullPath {
    param(
        [Parameter(Mandatory)][string] $Path,
        [string] $BasePath = (Get-Location).Path
    )

    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

function Test-WimForgeCheckout {
    param([Parameter(Mandatory)][string] $Path)

    $cmakeFile = Join-Path $Path 'CMakeLists.txt'
    $gitEntry = Join-Path $Path '.git'
    if (-not (Test-Path -LiteralPath $cmakeFile -PathType Leaf) -or
        -not (Test-Path -LiteralPath $gitEntry)) {
        return $false
    }
    $header = Get-Content -LiteralPath $cmakeFile -Raw -ErrorAction SilentlyContinue
    return [bool]($header -match '(?im)^\s*project\s*\(\s*WimForge(?:\s|\))')
}

function Find-WimForgeCheckout {
    param([Parameter(Mandatory)][string] $StartPath)

    $candidate = Resolve-FullPath -Path $StartPath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $candidate = Split-Path -Parent $candidate
    }
    while ($candidate) {
        if (Test-WimForgeCheckout -Path $candidate) {
            return $candidate
        }
        $parent = Split-Path -Parent $candidate
        if (-not $parent -or $parent.Equals($candidate, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $candidate = $parent
    }
    return $null
}

function Get-RepositoryDecision {
    if ($RepositoryPath) {
        $target = Resolve-FullPath -Path $RepositoryPath
        return [pscustomobject]@{
            Path = $target
            Exists = (Test-WimForgeCheckout -Path $target)
            Source = 'explicit path'
        }
    }

    $checkout = Find-WimForgeCheckout -StartPath (Get-Location).Path
    if (-not $checkout) {
        $checkout = Find-WimForgeCheckout -StartPath $PSScriptRoot
    }
    if ($checkout) {
        return [pscustomobject]@{
            Path = $checkout
            Exists = $true
            Source = 'existing checkout'
        }
    }

    $defaultPath = Join-Path $knownUserProfile 'source\repos\WimForge'
    return [pscustomobject]@{
        Path = [IO.Path]::GetFullPath($defaultPath)
        Exists = (Test-WimForgeCheckout -Path $defaultPath)
        Source = 'default user path'
    }
}

function Find-Executable {
    param(
        [Parameter(Mandatory)][string] $Name,
        [string[]] $Candidates = @()
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [IO.Path]::GetFullPath($candidate)
        }
    }
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) {
        return $command.Source
    }
    return $null
}

function Find-Git {
    return Find-Executable -Name 'git.exe' -Candidates @(
        (Join-Path $knownProgramFiles 'Git\cmd\git.exe'),
        (Join-Path $knownProgramFilesX86 'Git\cmd\git.exe'))
}

function Find-CMake {
    return Find-Executable -Name 'cmake.exe' -Candidates @(
        (Join-Path $knownProgramFiles 'CMake\bin\cmake.exe'))
}

function Find-Ninja {
    return Find-Executable -Name 'ninja.exe' -Candidates @(
        (Join-Path $knownLocalAppData 'Microsoft\WinGet\Links\ninja.exe'))
}

function Find-InnoCompiler {
    return Find-Executable -Name 'iscc.exe' -Candidates @(
        (Join-Path $knownLocalAppData 'Programs\Inno Setup 6\ISCC.exe'),
        (Join-Path $knownProgramFilesX86 'Inno Setup 6\ISCC.exe'),
        (Join-Path $knownProgramFiles 'Inno Setup 6\ISCC.exe'))
}

function Find-Aqt {
    return Find-Executable -Name 'aqt.exe' -Candidates @(
        (Join-Path $knownLocalAppData 'Microsoft\WinGet\Links\aqt.exe'))
}

function Find-VsWhere {
    return Find-Executable -Name 'vswhere.exe' -Candidates @(
        (Join-Path $knownProgramFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'))
}

function Find-VisualStudioCppTools {
    $vswhere = Find-VsWhere
    if (-not $vswhere) {
        return $null
    }
    $installationOutput = @(& $vswhere -latest -products '*' -version '[17.0,18.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null)
    $queryExitCode = $LASTEXITCODE
    $installation = $installationOutput | Select-Object -First 1
    if ($queryExitCode -ne 0 -or -not $installation) {
        return $null
    }
    $vcvars = Join-Path $installation.Trim() 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
        return $null
    }
    return $installation.Trim()
}

function Find-WindowsSdk {
    $kitsRoot = Join-Path $knownProgramFilesX86 'Windows Kits\10'
    $includeRoot = Join-Path $kitsRoot 'Include'
    $libraryRoot = Join-Path $kitsRoot 'Lib'
    if (-not (Test-Path -LiteralPath $includeRoot -PathType Container) -or
        -not (Test-Path -LiteralPath $libraryRoot -PathType Container)) {
        return $null
    }
    foreach ($versionDirectory in @(Get-ChildItem -LiteralPath $includeRoot -Directory `
            -ErrorAction SilentlyContinue | Sort-Object -Property Name -Descending)) {
        $windowsHeader = Join-Path $versionDirectory.FullName 'um\Windows.h'
        $kernelLibrary = Join-Path $libraryRoot (Join-Path $versionDirectory.Name 'um\x64\kernel32.lib')
        $sdkBin = Join-Path $kitsRoot (Join-Path 'bin' `
            (Join-Path $versionDirectory.Name 'x64'))
        $resourceCompiler = Join-Path $sdkBin 'rc.exe'
        $manifestTool = Join-Path $sdkBin 'mt.exe'
        if ((Test-Path -LiteralPath $windowsHeader -PathType Leaf) -and
            (Test-Path -LiteralPath $kernelLibrary -PathType Leaf) -and
            (Test-Path -LiteralPath $resourceCompiler -PathType Leaf) -and
            (Test-Path -LiteralPath $manifestTool -PathType Leaf)) {
            return $versionDirectory.Name
        }
    }
    return $null
}

function Get-ExecutableVersion {
    param(
        [Parameter(Mandatory)][string] $Executable,
        [Parameter(Mandatory)][string[]] $Arguments
    )

    $previousPreference = $ErrorActionPreference
    try {
        # Some legitimate version tools (notably aqt) write their version
        # banner to stderr. Capture both streams without allowing PowerShell
        # 5.1's NativeCommandError wrapper to terminate this bounded probe.
        $ErrorActionPreference = 'Continue'
        $output = @(& $Executable @Arguments 2>&1)
        $queryExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    $text = ($output | Select-Object -First 3) -join ' '
    if ($queryExitCode -ne 0 -or $text -notmatch '(\d+\.\d+(?:\.\d+)?)') {
        return $null
    }
    try {
        return [Version] $Matches[1]
    } catch {
        return $null
    }
}

function Test-InnoCompiler {
    param([string] $Executable)

    if (-not $Executable) {
        return $false
    }
    $previousPreference = $ErrorActionPreference
    try {
        # ISCC writes its help banner to stderr and intentionally exits 1.
        # PowerShell 5.1 must not promote that bounded probe to a terminating
        # NativeCommandError while the bootstrap itself uses Stop semantics.
        $ErrorActionPreference = 'Continue'
        $output = @(& $Executable '/?' 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    $text = $output -join "`n"
    # ISCC intentionally returns 1 for its help screen. Its exact banner is
    # still stronger live evidence than file-version fields, which are zeroed
    # in current Inno Setup builds.
    return $exitCode -in @(0, 1) -and
        $text -match '(?im)^Inno Setup 6 Command-Line Compiler\s*$'
}

function Add-QtCandidate {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][Collections.Generic.List[string]] $List,
        [string] $Path
    )
    if (-not $Path) {
        return
    }
    try {
        $fullPath = [IO.Path]::GetFullPath($Path)
    } catch {
        return
    }
    if (-not $List.Contains($fullPath)) {
        $List.Add($fullPath)
    }
}

function Test-PeX64Executable {
    param([Parameter(Mandatory)][string] $Path)

    try {
        [byte[]]$bytes = [IO.File]::ReadAllBytes($Path)
        if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
            return $false
        }
        $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
        if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length -or
            $bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or
            $bytes[$peOffset + 2] -ne 0 -or $bytes[$peOffset + 3] -ne 0) {
            return $false
        }
        return [BitConverter]::ToUInt16($bytes, $peOffset + 4) -eq 0x8664
    } catch {
        return $false
    }
}

function Find-CompatibleQt {
    $candidates = New-Object 'Collections.Generic.List[string]'
    Add-QtCandidate -List $candidates -Path $env:QT_ROOT_DIR

    if ($env:CMAKE_PREFIX_PATH) {
        foreach ($entry in $env:CMAKE_PREFIX_PATH.Split(';')) {
            Add-QtCandidate -List $candidates -Path $entry
        }
    }
    $qmakeOnPath = Find-Executable -Name 'qmake.exe'
    if ($qmakeOnPath) {
        Add-QtCandidate -List $candidates -Path (Split-Path -Parent (Split-Path -Parent $qmakeOnPath))
    }

    $searchRoots = @($QtInstallRoot, 'C:\Qt')
    foreach ($searchRoot in $searchRoots) {
        if (-not $searchRoot -or -not (Test-Path -LiteralPath $searchRoot -PathType Container)) {
            continue
        }
        foreach ($versionDirectory in @(Get-ChildItem -LiteralPath $searchRoot -Directory -ErrorAction SilentlyContinue)) {
            foreach ($kitDirectory in @(Get-ChildItem -LiteralPath $versionDirectory.FullName `
                    -Directory -ErrorAction SilentlyContinue)) {
                if ($kitDirectory.Name -match '^msvc.*_64$') {
                    Add-QtCandidate -List $candidates -Path $kitDirectory.FullName
                }
            }
        }
    }

    $compatible = @()
    foreach ($candidate in $candidates) {
        $qmake = Join-Path $candidate 'bin\qmake.exe'
        $deploy = Join-Path $candidate 'bin\windeployqt.exe'
        $qtConfig = Join-Path $candidate 'lib\cmake\Qt6\Qt6Config.cmake'
        if (-not (Test-Path -LiteralPath $qmake -PathType Leaf) -or
            -not (Test-Path -LiteralPath $deploy -PathType Leaf) -or
            -not (Test-Path -LiteralPath $qtConfig -PathType Leaf)) {
            continue
        }
        $requiredComponents = @(
            'Qt6Core', 'Qt6Gui', 'Qt6Qml', 'Qt6Quick',
            'Qt6QuickControls2', 'Qt6QuickDialogs2'
        )
        $missingComponent = $requiredComponents | Where-Object {
            -not (Test-Path -LiteralPath (Join-Path $candidate `
                ("lib\cmake\{0}\{0}Config.cmake" -f $_)) -PathType Leaf)
        } | Select-Object -First 1
        if ($missingComponent) {
            continue
        }
        $versionOutput = @(& $qmake -query QT_VERSION 2>$null)
        $queryExitCode = $LASTEXITCODE
        $versionText = $versionOutput | Select-Object -First 1
        if ($queryExitCode -ne 0) {
            continue
        }
        $specOutput = @(& $qmake -query QMAKE_XSPEC 2>$null)
        $specExitCode = $LASTEXITCODE
        $specText = [string]($specOutput | Select-Object -First 1)
        if ($specExitCode -ne 0 -or $specText.Trim() -ne 'win32-msvc' -or
            -not (Test-PeX64Executable -Path $qmake)) {
            continue
        }
        try {
            $candidateVersion = [Version]$versionText
        } catch {
            continue
        }
        if ($candidateVersion -ge $minimumQtVersion) {
            $compatible += [pscustomobject]@{
                Root = $candidate
                Version = $candidateVersion
                QMake = $qmake
                WinDeployQt = $deploy
            }
        }
    }
    return $compatible | Sort-Object -Property Version -Descending | Select-Object -First 1
}

function Get-DependencyState {
    $git = Find-Git
    $gitVersion = if ($git) {
        Get-ExecutableVersion -Executable $git -Arguments @('--version')
    } else { $null }
    $cmake = Find-CMake
    $cmakeVersion = $null
    if ($cmake) {
        $cmakeVersion = Get-ExecutableVersion -Executable $cmake -Arguments @('--version')
    }
    $ninja = Find-Ninja
    $ninjaVersion = if ($ninja) {
        Get-ExecutableVersion -Executable $ninja -Arguments @('--version')
    } else { $null }
    $qt = Find-CompatibleQt
    $aqt = Find-Aqt
    $aqtVersion = if ($aqt) {
        Get-ExecutableVersion -Executable $aqt -Arguments @('version')
    } else { $null }
    $inno = Find-InnoCompiler
    $innoCompatible = Test-InnoCompiler -Executable $inno
    return [pscustomobject]@{
        Git = $git
        GitVersion = $gitVersion
        CMake = $cmake
        CMakeVersion = $cmakeVersion
        CMakeCompatible = [bool]($cmakeVersion -and $cmakeVersion -ge $minimumCMakeVersion)
        Ninja = $ninja
        NinjaVersion = $ninjaVersion
        VisualStudio = Find-VisualStudioCppTools
        WindowsSdk = Find-WindowsSdk
        Qt = $qt
        InnoSetup = $inno
        InnoCompatible = $innoCompatible
        Aqt = $aqt
        AqtVersion = $aqtVersion
    }
}

function Get-DependencyRepairPlan {
    param([Parameter(Mandatory)] $Dependencies)

    $packages = New-Object 'Collections.Generic.List[string]'
    if (-not $Dependencies.GitVersion) {
        $packages.Add('Git.Git')
    }
    if (-not $Dependencies.CMakeCompatible) {
        $packages.Add('Kitware.CMake')
    }
    if (-not $Dependencies.VisualStudio -or -not $Dependencies.WindowsSdk) {
        $packages.Add('Microsoft.VisualStudio.2022.BuildTools')
    }
    if (-not $Dependencies.InnoCompatible) {
        $packages.Add('JRSoftware.InnoSetup')
    }
    return $packages.ToArray()
}

function Get-UserDependencyRepairPlan {
    param([Parameter(Mandatory)] $Dependencies)

    $packages = New-Object 'Collections.Generic.List[string]'
    if (-not $Dependencies.NinjaVersion) {
        $packages.Add('Ninja-build.Ninja')
    }
    if (-not $Dependencies.Qt -and -not $Dependencies.AqtVersion) {
        $packages.Add('miurahr.aqtinstall')
    }
    return $packages.ToArray()
}

function Refresh-ProcessPath {
    $parts = New-Object 'Collections.Generic.List[string]'
    foreach ($scope in @('Machine', 'User')) {
        $value = [Environment]::GetEnvironmentVariable('Path', $scope)
        if ($value) {
            foreach ($entry in $value.Split(';')) {
                if ($entry -and -not $parts.Contains($entry)) {
                    $parts.Add($entry)
                }
            }
        }
    }
    foreach ($entry in @(
        (Join-Path $knownLocalAppData 'Microsoft\WinGet\Links'),
        (Join-Path $knownProgramFiles 'CMake\bin'),
        (Join-Path $knownProgramFiles 'Git\cmd'),
        (Join-Path $knownProgramFilesX86 'Inno Setup 6'),
        $env:Path
    )) {
        if (-not $entry) {
            continue
        }
        foreach ($piece in $entry.Split(';')) {
            if ($piece -and -not $parts.Contains($piece)) {
                $parts.Add($piece)
            }
        }
    }
    $env:Path = $parts -join ';'
}

function Test-TrustedWingetCandidate {
    param([string] $Path)

    if (-not $Path) {
        return $null
    }
    $windowsApps = [IO.Path]::GetFullPath((Join-Path $knownProgramFiles 'WindowsApps'))
    $windowsAppsPrefix = $windowsApps.TrimEnd([char[]]@('\', '/')) + `
        [IO.Path]::DirectorySeparatorChar
    $candidate = [IO.Path]::GetFullPath($Path)
    if (-not $candidate.StartsWith($windowsAppsPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        return $null
    }
    Assert-NoReparsePath -Path $candidate
    $signature = Microsoft.PowerShell.Security\Get-AuthenticodeSignature `
        -LiteralPath $candidate
    if ($signature.Status -ne [Management.Automation.SignatureStatus]::Valid -or
        -not $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch '(?i)^CN=Microsoft Corporation(?:,|$)') {
        return $null
    }
    return $candidate
}

function Find-Winget {
    # Never elevate through the per-user app-execution alias or PATH. The
    # protected child revalidates the exact signed path supplied by its parent,
    # which also supports over-the-shoulder UAC with another admin identity.
    if ($DependencyRepairOnly -and $TrustedWingetPath) {
        return Test-TrustedWingetCandidate -Path $TrustedWingetPath
    }

    # Resolve the invoking user's App Installer registration under the normal
    # token, then validate its protected location and Microsoft signature.
    $trustedPowerShellHome = Join-Path ([Environment]::SystemDirectory) `
        'WindowsPowerShell\v1.0'
    $appxManifest = Join-Path $trustedPowerShellHome 'Modules\Appx\Appx.psd1'
    if (-not (Test-Path -LiteralPath $appxManifest -PathType Leaf)) {
        return $null
    }
    Microsoft.PowerShell.Core\Import-Module -Name $appxManifest -Force -ErrorAction Stop
    $package = Appx\Get-AppxPackage -Name 'Microsoft.DesktopAppInstaller' `
        -ErrorAction SilentlyContinue | Sort-Object -Property Version -Descending |
        Select-Object -First 1
    if (-not $package -or -not $package.InstallLocation) {
        return $null
    }
    $installRoot = [IO.Path]::GetFullPath([string]$package.InstallLocation)
    $candidate = Join-Path $installRoot 'winget.exe'
    return Test-TrustedWingetCandidate -Path $candidate
}

function Assert-DependencyRepairAuthority {
    if (-not (Test-IsAdministrator)) {
        throw ('A dependency changed or disappeared after the elevated repair phase. ' +
               'Rerun the bootstrap so it can repair dependencies before the unelevated build; ' +
               'the build phase will not silently request additional elevation.')
    }
}

function Install-WingetPackage {
    param(
        [Parameter(Mandatory)][string] $Id,
        [string] $Override
    )

    Assert-DependencyRepairAuthority
    $winget = Find-Winget
    $wingetVersion = if ($winget) {
        Get-ExecutableVersion -Executable $winget -Arguments @('--version')
    } else { $null }
    if (-not $wingetVersion) {
        throw ('Windows Package Manager (winget) is unavailable. Install or update Microsoft App ' +
               'Installer, then rerun this command. The executable did not pass a live version ' +
               'probe and no remote script fallback is used.')
    }
    $arguments = @(
        'install', '--id', $Id, '--exact', '--source', 'winget', '--silent',
        '--scope', 'machine', '--force', '--disable-interactivity', '--accept-source-agreements',
        '--accept-package-agreements'
    )
    if ($Override) {
        $arguments += @('--override', $Override)
    }
    Write-Host "> winget install --id $Id --exact --source winget --scope machine --force (noninteractive)"
    & $winget @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "WinGet could not install $Id (exit code $LASTEXITCODE)."
    }
    if (-not $DependencyRepairOnly) {
        Refresh-ProcessPath
    }
}

function Install-UserWingetPackage {
    param(
        [Parameter(Mandatory)]
        [ValidateSet('Ninja-build.Ninja', 'miurahr.aqtinstall')]
        [string] $Id
    )

    if (Test-IsAdministrator) {
        throw "Refusing to install a user-profile package with an administrator token: $Id"
    }
    $winget = Find-Winget
    $wingetVersion = if ($winget) {
        Get-ExecutableVersion -Executable $winget -Arguments @('--version')
    } else { $null }
    if (-not $wingetVersion) {
        throw ('Windows Package Manager (winget) is unavailable. Install or update Microsoft App ' +
               'Installer, then rerun this command. No remote script fallback is used.')
    }
    $arguments = @(
        'install', '--id', $Id, '--exact', '--source', 'winget', '--silent',
        '--scope', 'user', '--force', '--disable-interactivity',
        '--accept-source-agreements', '--accept-package-agreements'
    )
    Write-Host "> winget install --id $Id --exact --source winget --scope user --force (noninteractive)"
    & $winget @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "WinGet could not install user package $Id (exit code $LASTEXITCODE)."
    }
    Refresh-ProcessPath
}

function Invoke-ElevatedDependencyRepair {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]] $PackageIds
    )

    $allowed = @(
        'Git.Git',
        'Kitware.CMake',
        'Microsoft.VisualStudio.2022.BuildTools',
        'JRSoftware.InnoSetup'
    )
    foreach ($packageId in @($PackageIds | Select-Object -Unique)) {
        if ($packageId -notin $allowed) {
            throw "The elevated repair plan contains a package outside the allowlist: $packageId"
        }
        if ($packageId -eq 'Microsoft.VisualStudio.2022.BuildTools') {
            $vsOverride = ('--wait --quiet --norestart ' +
                '--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
            Install-WingetPackage -Id $packageId -Override $vsOverride
        } else {
            Install-WingetPackage -Id $packageId
        }
    }
}

function Ensure-GitDependency {
    Refresh-ProcessPath
    $git = Find-Git
    if (-not $git -or -not (Get-ExecutableVersion -Executable $git -Arguments @('--version'))) {
        Install-WingetPackage -Id 'Git.Git'
    }
    $git = Find-Git
    $gitVersion = if ($git) {
        Get-ExecutableVersion -Executable $git -Arguments @('--version')
    } else { $null }
    if (-not $gitVersion) {
        throw 'Git.Git completed installation, but git.exe did not pass live version verification.'
    }
    return [pscustomobject]@{ Path = $git; Version = $gitVersion }
}

function Ensure-Dependencies {
    $script:stage = 'dependency repair'
    Refresh-ProcessPath

    $gitDependency = Ensure-GitDependency
    $git = $gitDependency.Path
    $gitVersion = $gitDependency.Version

    $cmake = Find-CMake
    $cmakeVersion = $null
    if ($cmake) {
        $cmakeVersion = Get-ExecutableVersion -Executable $cmake -Arguments @('--version')
    }
    if (-not $cmakeVersion -or $cmakeVersion -lt $minimumCMakeVersion) {
        Install-WingetPackage -Id 'Kitware.CMake'
    }
    $cmake = Find-CMake
    if (-not $cmake -or
        (Get-ExecutableVersion -Executable $cmake -Arguments @('--version')) -lt $minimumCMakeVersion) {
        throw 'CMake 3.24 or newer is required but was not available after repair.'
    }

    $ninja = Find-Ninja
    if (-not $ninja -or -not (Get-ExecutableVersion -Executable $ninja -Arguments @('--version'))) {
        Install-WingetPackage -Id 'Ninja-build.Ninja'
    }
    $ninja = Find-Ninja
    $ninjaVersion = if ($ninja) {
        Get-ExecutableVersion -Executable $ninja -Arguments @('--version')
    } else { $null }
    if (-not $ninjaVersion) {
        throw 'Ninja-build.Ninja completed installation, but ninja.exe did not pass live version verification.'
    }

    if (-not (Find-VisualStudioCppTools) -or -not (Find-WindowsSdk)) {
        $vsOverride = ('--wait --quiet --norestart ' +
            '--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
        Install-WingetPackage -Id 'Microsoft.VisualStudio.2022.BuildTools' `
            -Override $vsOverride
    }
    $visualStudio = Find-VisualStudioCppTools
    $windowsSdk = Find-WindowsSdk
    if (-not $visualStudio -or -not $windowsSdk) {
        throw ('Visual Studio 2022 Build Tools repair completed, but the MSVC x64 workload ' +
               'or a usable x64 Windows SDK is still incomplete. Use Visual Studio Installer ' +
               'to repair Desktop development with C++, then rerun the bootstrap.')
    }

    $qt = Find-CompatibleQt
    if (-not $qt) {
        $aqt = Find-Aqt
        if (-not $aqt -or
            -not (Get-ExecutableVersion -Executable $aqt -Arguments @('version'))) {
            # The elevated phase is responsible for WinGet package repair.
            # Never silently re-elevate in the normal-token build phase.
            Assert-DependencyRepairAuthority
        }
        $qtTarget = Join-Path (Resolve-FullPath -Path $QtInstallRoot) `
            (Join-Path $QtVersion 'msvc2022_64')
        Write-Host "> aqt install-qt windows desktop $QtVersion win64_msvc2022_64"
        Write-Host '  Qt archive hashes remain enabled; unsafe hash bypass is never used.'
        & $aqt install-qt windows desktop $QtVersion win64_msvc2022_64 `
            --outputdir (Resolve-FullPath -Path $QtInstallRoot)
        if ($LASTEXITCODE -ne 0) {
            throw "aqt could not install Qt $QtVersion (exit code $LASTEXITCODE)."
        }
        $qt = Find-CompatibleQt
        if (-not $qt -and (Test-Path -LiteralPath $qtTarget -PathType Container)) {
            throw "Qt was downloaded but the required MSVC x64 kit is incomplete: $qtTarget"
        }
    }
    if (-not $qt) {
        throw ('Qt 6.8 or newer with the MSVC x64 Core, Gui, Qml, Quick, ' +
               'QuickControls2, and QuickDialogs2 components could not be discovered.')
    }
    $env:QT_ROOT_DIR = $qt.Root
    if ($env:CMAKE_PREFIX_PATH) {
        $env:CMAKE_PREFIX_PATH = $qt.Root + ';' + $env:CMAKE_PREFIX_PATH
    } else {
        $env:CMAKE_PREFIX_PATH = $qt.Root
    }
    $env:Path = (Join-Path $qt.Root 'bin') + ';' + $env:Path

    $inno = Find-InnoCompiler
    if (-not (Test-InnoCompiler -Executable $inno)) {
        Install-WingetPackage -Id 'JRSoftware.InnoSetup'
    }
    $inno = Find-InnoCompiler
    if (-not (Test-InnoCompiler -Executable $inno)) {
        throw 'JRSoftware.InnoSetup completed installation, but ISCC.exe did not pass live version verification.'
    }

    Write-Host 'Verified build dependencies:'
    Write-Host "  Git ${gitVersion}: $git"
    Write-Host "  CMake $(Get-ExecutableVersion -Executable $cmake -Arguments @('--version')): $cmake"
    Write-Host "  Ninja ${ninjaVersion}: $ninja"
    Write-Host "  Visual Studio 2022 C++ tools: $visualStudio"
    Write-Host "  Windows SDK $windowsSdk"
    Write-Host "  Qt $($qt.Version): $($qt.Root)"
    Write-Host "  Inno Setup 6: $inno"
}

function Set-VerifiedBuildEnvironment {
    $git = Find-Git
    $cmake = Find-CMake
    $ninja = Find-Ninja
    $qt = Find-CompatibleQt
    if (-not $git -or -not $cmake -or -not $ninja -or -not $qt) {
        throw 'Verified tool paths disappeared before the build environment was finalized.'
    }
    $directories = New-Object 'Collections.Generic.List[string]'
    foreach ($directory in @(
        (Join-Path $qt.Root 'bin'),
        (Split-Path -Parent $cmake),
        (Split-Path -Parent $git),
        (Split-Path -Parent $ninja),
        [Environment]::SystemDirectory
    )) {
        if ($directory -and -not $directories.Contains($directory)) {
            $directories.Add($directory)
        }
    }
    foreach ($directory in $env:Path.Split(';')) {
        if ($directory -and -not $directories.Contains($directory)) {
            $directories.Add($directory)
        }
    }
    $env:Path = $directories -join ';'
    $env:ComSpec = Join-Path ([Environment]::SystemDirectory) 'cmd.exe'
    $env:ProgramFiles = $knownProgramFiles
    ${env:ProgramFiles(x86)} = $knownProgramFilesX86
    $env:LOCALAPPDATA = $knownLocalAppData
}

function Assert-CloneTargetSafe {
    param([Parameter(Mandatory)][string] $Path)

    Assert-NoReparsePath -Path $Path
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "RepositoryPath exists but is not a directory: $Path"
    }
    $entries = @(Get-ChildItem -LiteralPath $Path -Force -ErrorAction Stop)
    if ($entries.Count -ne 0) {
        throw ("Refusing to clone into a non-empty directory: $Path. " +
               'Choose an empty -RepositoryPath; this script never cleans an existing directory.')
    }
}

function Ensure-Repository {
    param([Parameter(Mandatory)] $Decision)

    if ($Decision.Exists) {
        Assert-NoReparsePath -Path $Decision.Path
        return $Decision.Path
    }
    $script:stage = 'repository clone'
    Assert-CloneTargetSafe -Path $Decision.Path
    $parent = Split-Path -Parent $Decision.Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    Assert-NoReparsePath -Path $Decision.Path
    $git = Find-Git
    if (-not $git) {
        throw 'git.exe is required before cloning the repository.'
    }
    Write-Host "> git clone --depth 1 --no-tags -- $cloneUri $($Decision.Path)"
    & $git -c 'core.hooksPath=NUL' -c 'init.templateDir=' clone `
        --depth 1 --no-tags -- $cloneUri $Decision.Path
    if ($LASTEXITCODE -ne 0 -or -not (Test-WimForgeCheckout -Path $Decision.Path)) {
        throw "Git did not produce a valid WimForge checkout at $($Decision.Path)."
    }
    [string]$origin = @(& $git -C $Decision.Path remote get-url origin 2>$null) |
        Select-Object -First 1
    if ($LASTEXITCODE -ne 0 -or
        $origin.Trim() -ne $cloneUri) {
        throw 'The cloned checkout did not retain the expected canonical origin URL.'
    }
    return $Decision.Path
}

function Get-RepositoryIntegrityState {
    param(
        [Parameter(Mandatory)][string] $Git,
        [Parameter(Mandatory)][string] $RepositoryRoot,
        [switch] $TrackedOnly
    )

    $untrackedMode = if ($TrackedOnly) { 'no' } else { 'all' }
    $status = @(& $Git -c 'status.showUntrackedFiles=all' -C $RepositoryRoot `
        status --porcelain=v1 "--untracked-files=$untrackedMode" 2>$null)
    $statusExitCode = $LASTEXITCODE
    $indexFlags = @(& $Git -C $RepositoryRoot ls-files -v 2>$null)
    $flagsExitCode = $LASTEXITCODE
    if ($statusExitCode -ne 0 -or $flagsExitCode -ne 0) {
        throw "Unable to inspect repository integrity without changing it: $RepositoryRoot"
    }
    # Lowercase ls-files -v tags mean assume-unchanged; uppercase S is
    # skip-worktree. Either can hide bytes from an ordinary status check.
    $hiddenIndexState = @($indexFlags | Where-Object { $_ -cmatch '^(?:[a-z]|S) ' })
    return [pscustomobject]@{
        Status = $status
        HiddenIndexState = $hiddenIndexState
        Clean = ($status.Count -eq 0 -and $hiddenIndexState.Count -eq 0)
    }
}

function New-VerifiedSourceSnapshot {
    param(
        [Parameter(Mandatory)][string] $Git,
        [Parameter(Mandatory)][string] $RepositoryRoot,
        [Parameter(Mandatory)][string] $SourceCommit,
        [Parameter(Mandatory)][string] $Workspace
    )

    $runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), `
        [Guid]::NewGuid().ToString('N')
    $runRoot = Join-Path $Workspace (Join-Path 'runs' $runId)
    $snapshotRoot = Join-Path $runRoot 'source'
    Assert-NoReparsePath -Path $runRoot
    Assert-ManagedChildSafe -Workspace $Workspace -Path $runRoot
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    Assert-NoReparsePath -Path $runRoot

    # A local transport clone contains committed blobs only, so ignored files,
    # untracked files, and index flags in the developer checkout cannot affect
    # the release. Disable inherited Git configuration/environment for this
    # exact-commit snapshot and prevent templates/hooks from being populated.
    foreach ($entry in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'GIT_*' })) {
        Remove-Item -LiteralPath ("Env:{0}" -f $entry.Name) -ErrorAction SilentlyContinue
    }
    try {
        $env:GIT_CONFIG_NOSYSTEM = '1'
        $env:GIT_CONFIG_GLOBAL = 'NUL'
        $env:GIT_TERMINAL_PROMPT = '0'
        Write-Host "> git clone --no-local --no-checkout --no-tags -- [verified local source] $snapshotRoot"
        & $Git -c 'core.hooksPath=NUL' -c 'init.templateDir=' `
            -c 'core.autocrlf=false' clone --no-local --no-checkout --no-tags -- `
            $RepositoryRoot $snapshotRoot
        if ($LASTEXITCODE -ne 0) {
            throw 'Git could not create the isolated source snapshot.'
        }
        & $Git -c 'core.hooksPath=NUL' -c 'core.autocrlf=false' -C $snapshotRoot `
            checkout --detach --force $SourceCommit
        if ($LASTEXITCODE -ne 0) {
            throw "Git could not check out the verified source commit in the snapshot: $SourceCommit"
        }
    } finally {
        foreach ($entry in @(Get-ChildItem Env: | Where-Object { $_.Name -like 'GIT_*' })) {
            Remove-Item -LiteralPath ("Env:{0}" -f $entry.Name) -ErrorAction SilentlyContinue
        }
        $env:GIT_TERMINAL_PROMPT = '0'
    }

    Assert-NoReparsePath -Path $snapshotRoot
    if (-not (Test-WimForgeCheckout -Path $snapshotRoot)) {
        throw "The isolated source snapshot is not a valid WimForge checkout: $snapshotRoot"
    }
    [string]$snapshotCommit = @(& $Git -C $snapshotRoot rev-parse --verify HEAD 2>$null) |
        Select-Object -First 1
    $snapshotIntegrity = Get-RepositoryIntegrityState -Git $Git `
        -RepositoryRoot $snapshotRoot
    if ($snapshotCommit -ne $SourceCommit -or -not $snapshotIntegrity.Clean) {
        $statusEvidence = @($snapshotIntegrity.Status + $snapshotIntegrity.HiddenIndexState) `
            -join '; '
        throw ("The isolated source snapshot does not exactly match the verified source commit. " +
               "head=$snapshotCommit expected=$SourceCommit state=$statusEvidence")
    }
    return [pscustomobject]@{
        RunRoot = $runRoot
        RepositoryRoot = $snapshotRoot
        BuildDirectory = 'build-bootstrap/build'
        OutputDirectory = 'build-bootstrap/dist'
        BuildPath = Join-Path $snapshotRoot 'build-bootstrap\build'
        OutputPath = Join-Path $snapshotRoot 'build-bootstrap\dist'
    }
}

function Resolve-ProductVersion {
    param([Parameter(Mandatory)][string] $RepositoryRoot)

    if ($Version) {
        return $Version
    }
    $cmakeText = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'CMakeLists.txt') -Raw
    if ($cmakeText -notmatch '(?im)project\s*\(\s*WimForge\s+VERSION\s+(\d+\.\d+\.\d+)') {
        throw 'Could not infer the product version from project(WimForge VERSION ...) in CMakeLists.txt.'
    }
    return $Matches[1]
}

function Assert-ManagedBuildWorkspace {
    param([Parameter(Mandatory)][string] $RepositoryRoot)

    Assert-NoReparsePath -Path $RepositoryRoot
    $workspace = Join-Path $RepositoryRoot 'build-bootstrap'
    $repositoryPrefix = $RepositoryRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $fullWorkspace = [IO.Path]::GetFullPath($workspace)
    if (-not $fullWorkspace.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Managed build workspace escaped the repository: $fullWorkspace"
    }
    $marker = Join-Path $fullWorkspace '.managed-by-bootstrap-build'
    if (Test-Path -LiteralPath $fullWorkspace) {
        $item = Get-Item -LiteralPath $fullWorkspace -Force
        if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing unsafe managed build workspace: $fullWorkspace"
        }
        if (-not (Test-Path -LiteralPath $marker -PathType Leaf) -or
            (Get-Content -LiteralPath $marker -Raw).Trim() -ne $managedWorkspaceMarker) {
            throw ("Refusing to clean unowned build content at $fullWorkspace. " +
                   'Move it aside or select a clean checkout; no automatic cleanup was attempted.')
        }
    } else {
        New-Item -ItemType Directory -Path $fullWorkspace -Force | Out-Null
        Set-Content -LiteralPath $marker -Value $managedWorkspaceMarker -Encoding Ascii
    }
    return $fullWorkspace
}

function Assert-ManagedChildSafe {
    param(
        [Parameter(Mandatory)][string] $Workspace,
        [Parameter(Mandatory)][string] $Path
    )
    $workspacePrefix = $Workspace.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not $fullPath.StartsWith($workspacePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Build output escaped the managed workspace: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        $item = Get-Item -LiteralPath $fullPath -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to clean a reparse-point build path: $fullPath"
        }
        try {
            $nestedReparsePoint = Get-ChildItem -LiteralPath $fullPath -Recurse -Force `
                -ErrorAction Stop | Where-Object {
                    $_.Attributes -band [IO.FileAttributes]::ReparsePoint
                } | Select-Object -First 1
        } catch {
            throw "Could not inspect the complete managed build tree safely: $fullPath"
        }
        if ($nestedReparsePoint) {
            throw "Refusing to clean a build tree containing a reparse point: $($nestedReparsePoint.FullName)"
        }
    }
}

function Invoke-ReleaseBuild {
    param(
        [Parameter(Mandatory)][string] $RepositoryRoot,
        [Parameter(Mandatory)][string] $ProductVersion,
        [Parameter(Mandatory)][string] $BuildDirectory,
        [Parameter(Mandatory)][string] $OutputDirectory,
        [Parameter(Mandatory)][string] $InnoCompiler
    )

    $releaseScript = Join-Path $RepositoryRoot 'scripts\build-release.ps1'
    if (-not (Test-Path -LiteralPath $releaseScript -PathType Leaf)) {
        throw "The supported release build entrypoint is missing: $releaseScript"
    }
    $arguments = @(
        '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $releaseScript,
        '-Version', $ProductVersion,
        '-BuildDirectory', $BuildDirectory,
        '-OutputDirectory', $OutputDirectory,
        '-InnoCompiler', $InnoCompiler
    )
    if ($SkipTests) {
        $arguments += '-SkipTests'
    }

    $lines = New-Object 'Collections.Generic.List[string]'
    $trustedPowerShell = Join-Path ([Environment]::SystemDirectory) `
        'WindowsPowerShell\v1.0\powershell.exe'
    & $trustedPowerShell @arguments 2>&1 | ForEach-Object {
        $line = [string]$_
        $lines.Add($line)
        Write-Host $line
    }
    $exitCode = $LASTEXITCODE
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $lines.ToArray()
    }
}

function Test-DeterministicCacheFailure {
    param([string[]] $Output)

    $text = $Output -join "`n"
    $patterns = @(
        'CMakeCache\.txt directory .* is different',
        'generator .* does not match the generator used previously',
        'source directory .* does not match the source directory',
        'could not load cache',
        'CMake Error:.*CMakeCache\.txt'
    )
    foreach ($pattern in $patterns) {
        if ($text -match $pattern) {
            return $true
        }
    }
    return $false
}

function Get-VerifiedArtifactSummary {
    param(
        [Parameter(Mandatory)][string] $OutputDirectory,
        [Parameter(Mandatory)][string] $ProductVersion
    )

    Assert-NoReparsePath -Path $OutputDirectory
    $expected = @(
        (Join-Path $OutputDirectory "WimForge-portable-x64-$ProductVersion.zip"),
        (Join-Path $OutputDirectory "WimForge-Setup-x64-$ProductVersion.exe")
    )
    $summary = @()
    foreach ($path in $expected) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "The release build reported success without its expected artifact: $path"
        }
        $item = Get-Item -LiteralPath $path -Force
        if ($item.Length -le 0 -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "The release artifact is empty or unsafe: $path"
        }
        $summary += [pscustomobject]@{
            Path = $item.FullName
            Bytes = $item.Length
            Sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    return $summary
}

function Show-Plan {
    param(
        [Parameter(Mandatory)] $Decision,
        [Parameter(Mandatory)] $Dependencies
    )

    Write-Host 'WimForge bootstrap plan (no changes executed)'
    Write-Host '  Elevation: skipped because -Plan was supplied'
    if ($Decision.Exists) {
        Write-Host "  Repository: use $($Decision.Path) [$($Decision.Source)]"
        if ($Dependencies.Git) {
            try {
                $integrity = Get-RepositoryIntegrityState -Git $Dependencies.Git `
                    -RepositoryRoot $Decision.Path
            } catch {
                $integrity = $null
            }
            if ($integrity -and $integrity.Clean) {
                Write-Host '  Checkout integrity: clean and commit-identifiable'
            } elseif ($integrity) {
                Write-Host '  Checkout integrity: DIRTY; a real build would stop before packaging'
            } else {
                Write-Host '  Checkout integrity: could not verify in plan mode'
            }
        } else {
            Write-Host '  Checkout integrity: requires Git installation before it can be verified'
        }
    } else {
        Write-Host "  Repository: clone $cloneUri"
        Write-Host "              to $($Decision.Path)"
    }
    Write-Host '  Dependency repair source: WinGet exact IDs, noninteractive agreements'
    $userRepairPlan = @(Get-UserDependencyRepairPlan -Dependencies $Dependencies)
    if ($userRepairPlan.Count -ne 0) {
        Write-Host "  Normal-token user package plan: $($userRepairPlan -join ', ')"
    } else {
        Write-Host '  Normal-token user package plan: none'
    }
    $repairPlan = @(Get-DependencyRepairPlan -Dependencies $Dependencies)
    if ($repairPlan.Count -ne 0) {
        Write-Host "  UAC package plan: $($repairPlan -join ', ')"
    } else {
        Write-Host '  UAC package plan: none; all system packages passed normal-token probes'
    }
    $dependencyRows = @(
        @('Git', 'Git.Git', [bool]$Dependencies.GitVersion),
        @('CMake >= 3.24', 'Kitware.CMake', [bool]$Dependencies.CMakeCompatible),
        @('Ninja', 'Ninja-build.Ninja', [bool]$Dependencies.NinjaVersion),
        @('VS 2022 C++ Build Tools', 'Microsoft.VisualStudio.2022.BuildTools', [bool]$Dependencies.VisualStudio),
        @('x64 Windows SDK', 'Visual Studio Build Tools recommended components', [bool]$Dependencies.WindowsSdk),
        @('Qt >= 6.8 MSVC x64', 'miurahr.aqtinstall', [bool]$Dependencies.Qt),
        @('Inno Setup 6', 'JRSoftware.InnoSetup', [bool]$Dependencies.InnoCompatible)
    )
    foreach ($row in $dependencyRows) {
        $state = 'would install/repair'
        if ($row[2]) {
            $state = 'present'
        }
        Write-Host ("  [{0}] {1} ({2})" -f $state, $row[0], $row[1])
    }
    if ($Dependencies.Qt) {
        Write-Host "  Qt selected: $($Dependencies.Qt.Root) ($($Dependencies.Qt.Version))"
    } else {
        Write-Host "  Qt target: $(Join-Path $QtInstallRoot (Join-Path $QtVersion 'msvc2022_64'))"
        Write-Host '  Qt archives: aqt hash verification enabled'
    }
    Write-Host '  Build entrypoint: scripts\build-release.ps1'
    if ($Version) {
        Write-Host "  Product version: $Version"
    } elseif ($Decision.Exists) {
        Write-Host "  Product version: $(Resolve-ProductVersion -RepositoryRoot $Decision.Path) (from CMakeLists.txt)"
    } else {
        Write-Host '  Product version: infer from CMakeLists.txt after clone'
    }
    Write-Host '  Build workspace: build-bootstrap (marker-owned; no git clean/reset)'
    Write-Host '  Tests: enabled unless -SkipTests is supplied explicitly'
    Write-Host '  External limits: UAC consent, WinGet/App Installer, network, vendor availability, and free disk space'
    Write-Host 'PLAN COMPLETE: no elevation, package install, clone, configure, build, or test was run.'
}

$resolvedLogPath = $null
$systemRepairPerformed = $false
$userRepairPerformed = $false
try {
    Assert-SupportedHost
    if ($DependencyRepairOnly) {
        if (-not $Elevated -or -not (Test-IsAdministrator)) {
            throw 'The internal dependency-repair phase requires the verified elevated handoff.'
        }
        Set-ElevatedRepairEnvironment
        $resolvedLogPath = Start-DeterministicTranscript
        Write-Host 'WimForge elevated dependency repair'
        Write-Host "Log: $resolvedLogPath"
        Invoke-ElevatedDependencyRepair -PackageIds @($RepairPackageIds)
        Write-Host 'DEPENDENCY REPAIR COMPLETE'
        Stop-DeterministicTranscript
        exit 0
    }

    if (Test-IsAdministrator) {
        if ($Plan) {
            throw ('Run -Plan from a normal Windows PowerShell session. Plan mode executes read-only ' +
                   'tool probes and writes a transcript, so it must not inherit an administrator token.')
        }
        throw ('Start the bootstrap from a normal Windows PowerShell session. It will request UAC ' +
               'for the bounded package-repair child; Qt installation, source checkout, build, ' +
               'tests, and packaging must not inherit an administrator token.')
    }

    Clear-GitProcessOverrides

    if (-not $Plan -and -not (Test-IsAdministrator)) {
        $resolvedLogPath = Start-DeterministicTranscript
        Write-Host 'WimForge normal-token preflight'
        Write-Host "Log: $resolvedLogPath"
        if ($Elevated) {
            throw 'Elevation handoff completed without an administrator token.'
        }
        $preElevationDecision = Get-RepositoryDecision
        if (-not $preElevationDecision.Exists) {
            Assert-CloneTargetSafe -Path $preElevationDecision.Path
        } else {
            $preElevationGit = Find-Git
            if ($preElevationGit) {
                $preElevationIntegrity = Get-RepositoryIntegrityState `
                    -Git $preElevationGit -RepositoryRoot $preElevationDecision.Path
                if (-not $preElevationIntegrity.Clean) {
                    throw ('The checkout is dirty. Commit the intended source before dependency ' +
                           'repair; no elevation or source mutation was attempted.')
                }
            }
        }
        # All executable probes happen under the normal token. The elevated
        # child receives only exact allowlisted package IDs and invokes only the
        # signed App Installer copy of WinGet from protected Program Files.
        $preElevationDependencies = Get-DependencyState
        $userRepairPackageIds = @(Get-UserDependencyRepairPlan `
            -Dependencies $preElevationDependencies)
        foreach ($userPackageId in $userRepairPackageIds) {
            Install-UserWingetPackage -Id $userPackageId
            $userRepairPerformed = $true
        }
        if ($userRepairPerformed) {
            $preElevationDependencies = Get-DependencyState
        }
        $repairPackageIds = @(Get-DependencyRepairPlan `
            -Dependencies $preElevationDependencies)
        $trustedWinget = $null
        if ($repairPackageIds.Count -ne 0) {
            $trustedWinget = Find-Winget
            if (-not $trustedWinget) {
                throw ('The signed App Installer copy of winget.exe could not be resolved from ' +
                       'protected Program Files before UAC.')
            }
        }
        Stop-DeterministicTranscript
        if ($repairPackageIds.Count -ne 0) {
            Write-Host "System package repair plan: $($repairPackageIds -join ', ')"
            $repairExitCode = Start-ElevatedCopy -PackageIds $repairPackageIds `
                -WingetPath $trustedWinget
            $systemRepairPerformed = $true
            if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
                $resolvedLogPath = [IO.Path]::GetFullPath($LogPath)
            }
            if ($repairExitCode -ne 0) {
                throw "Elevated dependency repair failed with exit code $repairExitCode."
            }
        } else {
            Write-Host 'All system packages passed normal-token probes; no UAC repair was needed.'
        }
    }

    $resolvedLogPath = Start-DeterministicTranscript
    Write-Host 'WimForge one-command bootstrap/build'
    Write-Host "Log: $resolvedLogPath"
    if ($Plan) {
        Write-Host 'Plan mode is read-only; no UAC repair or build operation will run.'
    } elseif ($systemRepairPerformed) {
        Write-Host ('System package repair completed under UAC; Qt archive installation, source ' +
                    'checkout, build, tests, and packaging are unelevated.')
    } elseif ($userRepairPerformed) {
        Write-Host ('User-profile package repair completed without elevation; Qt archive installation, ' +
                    'source checkout, build, tests, and packaging remain unelevated.')
    } else {
        Write-Host ('System packages were already verified; Qt archive installation, source ' +
                    'checkout, build, tests, and packaging remain unelevated.')
    }

    $script:stage = 'local planning'
    $decision = Get-RepositoryDecision
    $dependencies = Get-DependencyState
    if ($Plan) {
        if (-not $decision.Exists) {
            Assert-CloneTargetSafe -Path $decision.Path
        }
        Show-Plan -Decision $decision -Dependencies $dependencies
        Stop-DeterministicTranscript
        exit 0
    }

    if (-not $decision.Exists) {
        # Reject a dangerous clone destination before changing repository state.
        Assert-CloneTargetSafe -Path $decision.Path
    }
    $script:stage = 'Git dependency and repository verification'
    $gitDependency = Ensure-GitDependency
    $repositoryRoot = Ensure-Repository -Decision $decision

    $git = $gitDependency.Path
    $sourceIntegrity = Get-RepositoryIntegrityState -Git $git `
        -RepositoryRoot $repositoryRoot
    if (-not $sourceIntegrity.Clean) {
        throw ('The checkout contains tracked or untracked changes. Refusing to publish artifacts ' +
               'whose build-info commit cannot describe their source exactly. Commit the intended ' +
               'source or use scripts\build-release.ps1 manually for an explicitly local build. ' +
               'No reset, clean, stash, or checkout operation was attempted.')
    }
    [string]$sourceCommit = @(& $git -C $repositoryRoot rev-parse --verify HEAD 2>$null) |
        Select-Object -First 1
    if ($LASTEXITCODE -ne 0 -or $sourceCommit -notmatch '^[0-9a-fA-F]{40,64}$') {
        throw 'Unable to bind the clean checkout to an exact source commit.'
    }
    Write-Host "Verified clean source commit: $sourceCommit"
    Ensure-Dependencies
    Set-VerifiedBuildEnvironment

    $script:stage = 'release build'
    $workspace = Assert-ManagedBuildWorkspace -RepositoryRoot $repositoryRoot
    $snapshot = New-VerifiedSourceSnapshot -Git $git -RepositoryRoot $repositoryRoot `
        -SourceCommit $sourceCommit -Workspace $workspace
    $buildRepositoryRoot = $snapshot.RepositoryRoot
    $productVersion = Resolve-ProductVersion -RepositoryRoot $buildRepositoryRoot
    $buildPath = $snapshot.BuildPath
    $outputPath = $snapshot.OutputPath
    Assert-ManagedChildSafe -Workspace $workspace -Path $buildPath
    Assert-ManagedChildSafe -Workspace $workspace -Path $outputPath
    $inno = Find-InnoCompiler

    Write-Host "Building WimForge $productVersion from isolated commit snapshot $buildRepositoryRoot"
    $result = Invoke-ReleaseBuild -RepositoryRoot $buildRepositoryRoot `
        -ProductVersion $productVersion -BuildDirectory $snapshot.BuildDirectory `
        -OutputDirectory $snapshot.OutputDirectory -InnoCompiler $inno

    if ($result.ExitCode -ne 0 -and (Test-DeterministicCacheFailure -Output $result.Output)) {
        Write-Warning 'A deterministic CMake cache/configuration mismatch was detected; retrying once in fresh sibling paths.'
        $script:stage = 'release build cache retry'
        $retryBuildPath = Join-Path $buildRepositoryRoot 'build-bootstrap\build-retry-1'
        $retryOutputPath = Join-Path $buildRepositoryRoot 'build-bootstrap\dist-retry-1'
        Assert-ManagedChildSafe -Workspace $workspace -Path $retryBuildPath
        Assert-ManagedChildSafe -Workspace $workspace -Path $retryOutputPath
        $result = Invoke-ReleaseBuild -RepositoryRoot $buildRepositoryRoot `
            -ProductVersion $productVersion -BuildDirectory 'build-bootstrap/build-retry-1' `
            -OutputDirectory 'build-bootstrap/dist-retry-1' -InnoCompiler $inno
        if ($result.ExitCode -eq 0) {
            $outputPath = $retryOutputPath
        }
    }
    if ($result.ExitCode -ne 0) {
        $tail = @($result.Output | Select-Object -Last 12) -join "`n    "
        throw "Release build failed with exit code $($result.ExitCode). Last output:`n    $tail"
    }

    $script:stage = 'artifact verification'
    [string]$postBuildCommit = @(& $git -C $buildRepositoryRoot `
        rev-parse --verify HEAD 2>$null) | Select-Object -First 1
    $postBuildIntegrity = Get-RepositoryIntegrityState -Git $git `
        -RepositoryRoot $buildRepositoryRoot -TrackedOnly
    if ($postBuildCommit -ne $sourceCommit -or -not $postBuildIntegrity.Clean) {
        throw 'Tracked source or HEAD changed inside the isolated snapshot during the release build.'
    }
    $artifacts = Get-VerifiedArtifactSummary -OutputDirectory $outputPath `
        -ProductVersion $productVersion

    Write-Host 'BOOTSTRAP BUILD COMPLETE'
    Write-Host "  Repository: $repositoryRoot"
    Write-Host "  Isolated source commit: $sourceCommit at $buildRepositoryRoot"
    Write-Host "  Release artifacts: $outputPath"
    foreach ($artifact in $artifacts) {
        Write-Host "    $($artifact.Path)"
        Write-Host "      bytes=$($artifact.Bytes) sha256=$($artifact.Sha256)"
    }
    Write-Host "  Log: $resolvedLogPath"
    Stop-DeterministicTranscript
    exit 0
} catch {
    $failure = $_
    if (-not $script:transcriptStarted -and $resolvedLogPath) {
        try {
            $null = Start-DeterministicTranscript
        } catch {
            # Preserve the original failure if even diagnostic append is no
            # longer possible; the existing log path is still reported.
        }
    }
    Write-Host ''
    Write-Host 'BOOTSTRAP BUILD FAILED' -ForegroundColor Red
    Write-Host "  Stage: $script:stage"
    Write-Host "  Reason: $($failure.Exception.Message)"
    if ($resolvedLogPath) {
        Write-Host "  Full log: $resolvedLogPath"
    }
    Write-Host '  The checkout was not reset, cleaned, stashed, or force-updated.'
    Stop-DeterministicTranscript
    exit 1
}
