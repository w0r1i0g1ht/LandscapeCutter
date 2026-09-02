[CmdletBinding()]
param(
    [string]$VcpkgRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$vcpkgTag = "2025.02.14"
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = Join-Path $repositoryRoot ".tools\vcpkg"
}
$resolvedRoot = [System.IO.Path]::GetFullPath($VcpkgRoot)
$gitDirectory = Join-Path $resolvedRoot ".git"
$bootstrap = Join-Path $resolvedRoot "bootstrap-vcpkg.bat"
$executable = Join-Path $resolvedRoot "vcpkg.exe"

if (-not (Test-Path -LiteralPath $resolvedRoot)) {
    git clone --branch $vcpkgTag --depth 1 https://github.com/microsoft/vcpkg.git $resolvedRoot
} elseif (-not (Test-Path -LiteralPath $gitDirectory)) {
    throw "The vcpkg directory exists but is not a Git checkout: $resolvedRoot"
}

$actualTag = git -C $resolvedRoot describe --tags --exact-match
if ($LASTEXITCODE -ne 0 -or $actualTag.Trim() -ne $vcpkgTag) {
    throw "Expected vcpkg tag $vcpkgTag at $resolvedRoot, found '$actualTag'."
}

& $bootstrap -disableMetrics
if ($LASTEXITCODE -ne 0) {
    throw "vcpkg bootstrap failed with exit code $LASTEXITCODE."
}

& $executable install --triplet x64-windows "--x-manifest-root=$repositoryRoot"
if ($LASTEXITCODE -ne 0) {
    throw "vcpkg dependency installation failed with exit code $LASTEXITCODE."
}
