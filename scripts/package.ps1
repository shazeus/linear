[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$OutputDirectory = '',
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\windows-adobe'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot "dist\Linear-$Version-windows-x64"
}
$artifact = Get-ChildItem -LiteralPath $BuildDirectory -Filter 'Linear.aex' -File -Recurse |
    Where-Object { $_.DirectoryName -match '\\Release$' } |
    Select-Object -First 1
if ($null -eq $artifact) {
    throw "Release artifact Linear.aex was not found under '$BuildDirectory'."
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Copy-Item -LiteralPath $artifact.FullName -Destination $OutputDirectory -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'README.md') -Destination $OutputDirectory -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs\BUILDING.md') -Destination $OutputDirectory -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs\USER_GUIDE.md') -Destination $OutputDirectory -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs\THIRD_PARTY_NOTICES.md') -Destination $OutputDirectory -Force
$licensePath = Join-Path $repositoryRoot 'LICENSE'
if (Test-Path -LiteralPath $licensePath) {
    Copy-Item -LiteralPath $licensePath -Destination $OutputDirectory -Force
}
$archivePath = "$OutputDirectory.zip"
Compress-Archive -Path (Join-Path $OutputDirectory '*') -DestinationPath $archivePath -Force
$archiveHash = Get-FileHash -LiteralPath $archivePath -Algorithm SHA256
$checksumPath = "$archivePath.sha256"
Set-Content -LiteralPath $checksumPath -Encoding ascii -NoNewline -Value (
    "$($archiveHash.Hash.ToLowerInvariant())  $([System.IO.Path]::GetFileName($archivePath))")
Write-Output "Prepared unsigned release package at '$archivePath'."
Write-Output "SHA-256: $($archiveHash.Hash)"
