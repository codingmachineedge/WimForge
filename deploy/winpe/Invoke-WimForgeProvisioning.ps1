[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [Uri] $Server,

    [Parameter(Mandatory = $true)]
    [string] $SetupPath,

    [string] $TokenFile,

    [string] $OutputPath = 'X:\WimForge\Autounattend.xml',

    [string] $LogPath = 'X:\WimForge\Provisioning.log',

    [ValidateRange(10, 300)]
    [int] $TimeoutSeconds = 60,

    [ValidateRange(1, 10)]
    [int] $MaxAttempts = 4,

    [ValidateRange(1, 30)]
    [int] $InitialRetrySeconds = 2,

    [ValidateRange(65536, 8388608)]
    [int] $MaxResponseBytes = 8388608,

    [switch] $AllowHttp
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Write-ProvisioningLog([string] $Message) {
    try {
        $logDirectory = Split-Path -Parent $LogPath
        if ($logDirectory) {
            [void](New-Item -ItemType Directory -Path $logDirectory -Force)
        }
        $line = '{0} {1}{2}' -f [DateTime]::UtcNow.ToString('o'), $Message, [Environment]::NewLine
        [IO.File]::AppendAllText($LogPath, $line, [Text.UTF8Encoding]::new($false))
    }
    catch {
        # Logging must never change whether provisioning fails closed.
    }
}

function Resolve-ProvisioningOutputPath([string] $Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'The provisioning output path cannot be empty.'
    }
    $resolvedPath = [IO.Path]::GetFullPath($Path)
    $resolvedDirectory = [IO.Path]::GetDirectoryName($resolvedPath)
    if ([string]::IsNullOrWhiteSpace($resolvedDirectory)) {
        throw 'The provisioning output path must resolve to a file location.'
    }
    [PSCustomObject]@{
        Path = $resolvedPath
        Directory = $resolvedDirectory
    }
}

if ($Server.Scheme -ne 'https' -and -not $AllowHttp) {
    Write-ProvisioningLog 'Rejected a non-HTTPS provisioning endpoint.'
    throw 'The provisioning server must use HTTPS. Use -AllowHttp only on an isolated, trusted lab network.'
}

Write-ProvisioningLog 'Starting WimForge provisioning.'
& wpeutil.exe WaitForNetwork
if ($LASTEXITCODE -ne 0) {
    Write-ProvisioningLog 'WinPE network initialization failed.'
    throw 'WinPE could not initialize the network.'
}
$setupAvailable = $false
for ($setupAttempt = 1; $setupAttempt -le $MaxAttempts; ++$setupAttempt) {
    if (Test-Path -LiteralPath $SetupPath -PathType Leaf) {
        $setupAvailable = $true
        break
    }
    if ($setupAttempt -lt $MaxAttempts) {
        $setupDelay = [Math]::Min(
            30,
            [int]($InitialRetrySeconds * [Math]::Pow(2, $setupAttempt - 1)))
        Write-ProvisioningLog "Windows Setup path unavailable; retrying after $setupDelay seconds."
        Start-Sleep -Seconds $setupDelay
    }
}
if (-not $setupAvailable) {
    Write-ProvisioningLog 'The configured Windows Setup path remained unavailable.'
    throw 'Windows Setup was not found at the configured path.'
}

$placeholderIds = @(
    '', '0', '00000000-0000-0000-0000-000000000000', 'Default String',
    'None', 'Not Applicable', 'System Serial Number', 'To Be Filled By O.E.M.', 'Unknown'
)

function ConvertTo-UsableIdentifier([string] $Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return $null }
    $candidate = $Value.Trim().Trim('{}')
    foreach ($placeholder in $placeholderIds) {
        if ($candidate.Equals($placeholder, [StringComparison]::OrdinalIgnoreCase)) {
            return $null
        }
    }
    if ($candidate -notmatch '^[A-Za-z0-9][A-Za-z0-9._:/-]{0,127}$') {
        return $null
    }
    return $candidate
}

$request = [ordered]@{}
try {
    $systemProduct = Get-CimInstance -ClassName Win32_ComputerSystemProduct -ErrorAction Stop
    $uuid = ConvertTo-UsableIdentifier ([string]$systemProduct.UUID)
    if ($uuid) { $request.uuid = $uuid }
}
catch {
    Write-ProvisioningLog 'The SMBIOS UUID probe was unavailable; continuing with other identifiers.'
}
try {
    $bios = Get-CimInstance -ClassName Win32_BIOS -ErrorAction Stop
    $serial = ConvertTo-UsableIdentifier ([string]$bios.SerialNumber)
    if ($serial) { $request.serial = $serial }
}
catch {
    Write-ProvisioningLog 'The BIOS serial probe was unavailable; continuing with other identifiers.'
}
try {
    $macs = @(
        Get-CimInstance -ClassName Win32_NetworkAdapter -ErrorAction Stop |
            Where-Object {
                $_.PhysicalAdapter -and
                $_.MACAddress -match '^(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}$'
            } |
            ForEach-Object { $_.MACAddress.Trim() } |
            Sort-Object -Unique |
            Select-Object -First 32
    )
    if ($macs.Count -gt 0) { $request.macs = $macs }
}
catch {
    Write-ProvisioningLog 'The physical MAC probe was unavailable; continuing with other identifiers.'
}
if ($request.Count -eq 0) {
    Write-ProvisioningLog 'No usable hardware identifier was available.'
    throw 'No usable hardware UUID, BIOS serial number, or physical MAC address was found.'
}

$token = $null
if ($TokenFile) {
    if (-not (Test-Path -LiteralPath $TokenFile -PathType Leaf)) {
        Write-ProvisioningLog 'The configured token file was unavailable.'
        throw 'The configured provisioning token file does not exist.'
    }
    $token = (Get-Content -LiteralPath $TokenFile -Raw).Trim()
    if ($token.Length -lt 16) {
        Write-ProvisioningLog 'The configured token file was invalid.'
        throw 'The provisioning token file is invalid.'
    }
}

$endpoint = [Uri]::new($Server, '/v1/unattend')
try {
    $outputTarget = Resolve-ProvisioningOutputPath $OutputPath
    $OutputPath = $outputTarget.Path
    [void][IO.Directory]::CreateDirectory($outputTarget.Directory)
}
catch {
    Write-ProvisioningLog 'The configured answer-file output path was invalid or unavailable.'
    throw
}
$partialPath = "$OutputPath.partial"
$payload = $request | ConvertTo-Json -Compress
$completed = $false

Add-Type -AssemblyName System.Net.Http
$handler = [Net.Http.HttpClientHandler]::new()
$handler.AllowAutoRedirect = $false
$client = [Net.Http.HttpClient]::new($handler)
$client.Timeout = [Threading.Timeout]::InfiniteTimeSpan

try {
    for ($attempt = 1; $attempt -le $MaxAttempts; ++$attempt) {
        Remove-Item -LiteralPath $partialPath -Force -ErrorAction SilentlyContinue
        $retryable = $true
        $message = $null
        $response = $null
        $responseStream = $null
        $fileStream = $null
        $cancellation = [Threading.CancellationTokenSource]::new(
            [TimeSpan]::FromSeconds($TimeoutSeconds))
        try {
            Write-ProvisioningLog "Request attempt $attempt of $MaxAttempts."
            $message = [Net.Http.HttpRequestMessage]::new(
                [Net.Http.HttpMethod]::Post, $endpoint)
            $message.Headers.Accept.Add(
                [Net.Http.Headers.MediaTypeWithQualityHeaderValue]::new('application/xml'))
            if ($token) {
                $message.Headers.Authorization =
                    [Net.Http.Headers.AuthenticationHeaderValue]::new('Bearer', $token)
            }
            $message.Content = [Net.Http.StringContent]::new(
                $payload, [Text.Encoding]::UTF8, 'application/json')

            $response = $client.SendAsync(
                $message,
                [Net.Http.HttpCompletionOption]::ResponseHeadersRead,
                $cancellation.Token).GetAwaiter().GetResult()
            $statusCode = [int]$response.StatusCode
            if ($statusCode -ge 300 -and $statusCode -lt 400) {
                $retryable = $false
                throw "The provisioning endpoint returned a redirect (HTTP $statusCode)."
            }
            if (-not $response.IsSuccessStatusCode) {
                $retryable = $statusCode -eq 408 -or $statusCode -eq 429 -or $statusCode -ge 500
                throw "The provisioning endpoint returned HTTP $statusCode."
            }
            if ($response.RequestMessage.RequestUri.Scheme -ne 'https' -and -not $AllowHttp) {
                $retryable = $false
                throw 'The provisioning response did not come from HTTPS.'
            }
            if ($response.Content.Headers.ContentType.MediaType -ne 'application/xml') {
                throw 'The provisioning response did not use application/xml.'
            }
            $contentLength = $response.Content.Headers.ContentLength
            if ($null -ne $contentLength -and $contentLength -gt $MaxResponseBytes) {
                $retryable = $false
                throw 'The provisioning response exceeded the configured size limit.'
            }

            $expectedDigest = [string](
                $response.Headers.GetValues('X-WimForge-SHA256') |
                    Select-Object -First 1)
            if ($expectedDigest -notmatch '^[0-9A-Fa-f]{64}$') {
                throw 'The provisioning response did not include a valid SHA-256 digest.'
            }

            $responseStream = $response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
            $fileStream = [IO.File]::Open(
                $partialPath,
                [IO.FileMode]::CreateNew,
                [IO.FileAccess]::Write,
                [IO.FileShare]::None)
            $buffer = [byte[]]::new(65536)
            [long]$total = 0
            while ($true) {
                $read = $responseStream.ReadAsync(
                    $buffer, 0, $buffer.Length, $cancellation.Token).GetAwaiter().GetResult()
                if ($read -le 0) { break }
                $total += $read
                if ($total -gt $MaxResponseBytes) {
                    $retryable = $false
                    throw 'The provisioning response exceeded the configured size limit.'
                }
                $fileStream.Write($buffer, 0, $read)
            }
            $fileStream.Flush($true)
            $fileStream.Dispose()
            $fileStream = $null
            $responseStream.Dispose()
            $responseStream = $null

            $actualDigest = (Get-FileHash -LiteralPath $partialPath -Algorithm SHA256).Hash
            if (-not $actualDigest.Equals(
                    $expectedDigest,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw 'The provisioning response digest did not match its body.'
            }

            $xmlSettings = [Xml.XmlReaderSettings]::new()
            $xmlSettings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
            $xmlSettings.XmlResolver = $null
            $xmlReader = [Xml.XmlReader]::Create($partialPath, $xmlSettings)
            try {
                $answerFile = [Xml.XmlDocument]::new()
                $answerFile.XmlResolver = $null
                $answerFile.Load($xmlReader)
            }
            finally {
                $xmlReader.Dispose()
            }
            if ($answerFile.DocumentElement.LocalName -ne 'unattend' -or
                $answerFile.DocumentElement.NamespaceURI -ne
                    'urn:schemas-microsoft-com:unattend') {
                throw 'The provisioning response is not a Windows unattended answer file.'
            }

            Move-Item -LiteralPath $partialPath -Destination $OutputPath -Force
            $completed = $true
            Write-ProvisioningLog 'A validated answer file was downloaded.'
            break
        }
        catch {
            Write-ProvisioningLog "Request attempt $attempt failed: $($_.Exception.Message)"
            if (-not $retryable -or $attempt -ge $MaxAttempts) {
                throw
            }
            $delay = [Math]::Min(
                30,
                [int]($InitialRetrySeconds * [Math]::Pow(2, $attempt - 1)))
            if ($response -and $response.Headers.RetryAfter -and
                $response.Headers.RetryAfter.Delta) {
                $delay = [Math]::Min(
                    60,
                    [Math]::Max(1, [int]$response.Headers.RetryAfter.Delta.TotalSeconds))
            }
            Write-ProvisioningLog "Retrying after $delay seconds."
            Start-Sleep -Seconds $delay
        }
        finally {
            if ($fileStream) { $fileStream.Dispose() }
            if ($responseStream) { $responseStream.Dispose() }
            if ($response) { $response.Dispose() }
            if ($message) { $message.Dispose() }
            $cancellation.Dispose()
            Remove-Item -LiteralPath $partialPath -Force -ErrorAction SilentlyContinue
        }
    }
}
finally {
    $client.Dispose()
    $handler.Dispose()
    Remove-Variable token -ErrorAction SilentlyContinue
    Remove-Variable payload -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $partialPath -Force -ErrorAction SilentlyContinue
}

if (-not $completed) {
    Write-ProvisioningLog 'Provisioning exhausted its retry budget.'
    throw 'WimForge provisioning did not produce an answer file.'
}

$setupArguments = "/Unattend:`"$OutputPath`""
Write-ProvisioningLog 'Launching Windows Setup with the validated answer file.'
$process = Start-Process -FilePath $SetupPath -ArgumentList $setupArguments -Wait -PassThru
Write-ProvisioningLog "Windows Setup exited with code $($process.ExitCode)."
exit $process.ExitCode
