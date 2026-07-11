[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ScriptPath
)

$ErrorActionPreference = 'Stop'

function Assert-True([bool] $Condition, [string] $Message) {
    if (-not $Condition) {
        throw $Message
    }
}

$tokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    $ScriptPath,
    [ref] $tokens,
    [ref] $parseErrors)
Assert-True ($parseErrors.Count -eq 0) 'The WinPE provisioning script must parse without errors.'

$resolver = $ast.Find(
    {
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq 'Resolve-ProvisioningOutputPath'
    },
    $true)
Assert-True ($null -ne $resolver) 'The WinPE script must define its output-path resolver.'

# Load only the pure resolver function from the AST. Dot-sourcing the deployment
# script would perform network and Setup operations, which a contract test must
# never trigger.
Invoke-Expression $resolver.Extent.Text

$workingDirectory = [IO.Path]::GetFullPath((Get-Location).ProviderPath)
$relative = Resolve-ProvisioningOutputPath 'Autounattend.xml'
Assert-True ([IO.Path]::IsPathRooted($relative.Path)) 'A bare output filename must become absolute.'
Assert-True ($relative.Directory -eq $workingDirectory) 'A bare filename must resolve beneath the current directory.'
Assert-True (
    $relative.Path -eq [IO.Path]::Combine($workingDirectory, 'Autounattend.xml')) `
    'A bare filename must retain its filename.'

$nested = Resolve-ProvisioningOutputPath (Join-Path 'answers' 'Autounattend.xml')
Assert-True (
    $nested.Directory -eq [IO.Path]::Combine($workingDirectory, 'answers')) `
    'A relative nested output must resolve its parent directory correctly.'

$emptyRejected = $false
try {
    [void](Resolve-ProvisioningOutputPath '   ')
}
catch {
    $emptyRejected = $true
}
Assert-True $emptyRejected 'A blank output path must fail closed.'

Write-Host 'winpe_provisioning_contract_tests: all checks passed'
