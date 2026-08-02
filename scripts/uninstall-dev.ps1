[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$DestinationRoot =
        (Join-Path $env:ProgramFiles 'Adobe\Common\Plug-ins\7.0\MediaCore')
)

$ErrorActionPreference = 'Stop'
$resolvedRoot = [System.IO.Path]::GetFullPath($DestinationRoot)
$target = [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot 'Linear'))
if (-not $target.StartsWith($resolvedRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove a path outside '$resolvedRoot'."
}
if (Test-Path -LiteralPath $target) {
    if ($PSCmdlet.ShouldProcess($target, 'Remove Linear development plug-in')) {
        Remove-Item -LiteralPath $target -Recurse -Force
        Write-Output "Removed '$target'."
    }
} else {
    Write-Output "Linear is not installed at '$target'."
}
