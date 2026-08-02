[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$DestinationRoot =
        (Join-Path $env:ProgramFiles 'Adobe\Common\Plug-ins\7.0\MediaCore')
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\windows-adobe'
}
$artifact = Get-ChildItem -LiteralPath $BuildDirectory -Filter 'Linear.aex' -File -Recurse |
    Where-Object { $_.DirectoryName -match '\\Release$' } |
    Select-Object -First 1
if ($null -eq $artifact) {
    throw "Linear.aex was not found under '$BuildDirectory'. Build the Adobe Release target first."
}
$destination = Join-Path $DestinationRoot 'Linear'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $artifact.FullName -Destination (Join-Path $destination 'Linear.aex') -Force
Write-Output "Installed Linear to '$destination'. Restart Adobe hosts to load it."
