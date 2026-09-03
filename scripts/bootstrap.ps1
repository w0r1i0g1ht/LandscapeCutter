[CmdletBinding()]
param(
    [string]$VcpkgRoot,

    [switch]$VerifyCheckoutOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$vcpkgTag = "2026.07.29"
$vcpkgTagObject = "c76c06644034521fb761a39f8f52d8e87d1103d5"
$vcpkgCommit = "9e593bb18ea69cc5095e012465dcd675a822ed0d"
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = Join-Path $repositoryRoot ".tools\vcpkg"
}
$resolvedRoot = [System.IO.Path]::GetFullPath($VcpkgRoot)
$gitDirectory = Join-Path $resolvedRoot ".git"
$bootstrap = Join-Path $resolvedRoot "bootstrap-vcpkg.bat"
$executable = Join-Path $resolvedRoot "vcpkg.exe"

if (-not (Test-Path -LiteralPath $resolvedRoot)) {
    git clone --branch $vcpkgTag https://github.com/microsoft/vcpkg.git $resolvedRoot
} elseif (-not (Test-Path -LiteralPath $gitDirectory)) {
    throw "The vcpkg directory exists but is not a Git checkout: $resolvedRoot"
}

function Get-GitValue {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $output = @(& git --no-replace-objects -C $resolvedRoot @Arguments)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "Could not read $Description from the vcpkg checkout at $resolvedRoot."
    }
    return ($output -join [Environment]::NewLine).Trim()
}

function Get-GitLines {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $output = @(& git --no-replace-objects -C $resolvedRoot @Arguments)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "Could not read $Description from the vcpkg checkout at $resolvedRoot."
    }
    return $output
}

$actualTag = Get-GitValue -Arguments @("describe", "--tags", "--exact-match", "HEAD") `
    -Description "the exact tag at HEAD"
if ($actualTag -ne $vcpkgTag) {
    throw "Expected exact vcpkg tag $vcpkgTag at $resolvedRoot, found '$actualTag'."
}

$actualTagType = Get-GitValue -Arguments @("cat-file", "-t", "refs/tags/$vcpkgTag") `
    -Description "tag object type"
if ($actualTagType -ne "tag") {
    throw "Expected $vcpkgTag to be an annotated tag object, found '$actualTagType'."
}

$actualTagObject = Get-GitValue -Arguments @("rev-parse", "refs/tags/$vcpkgTag") `
    -Description "tag object ID"
if ($actualTagObject -ne $vcpkgTagObject) {
    throw "Expected vcpkg tag object $vcpkgTagObject, found '$actualTagObject'. Recreate the checkout from the official tag."
}

$actualPeeledCommit = Get-GitValue -Arguments @("rev-parse", "refs/tags/$vcpkgTag^{commit}") `
    -Description "peeled tag commit"
if ($actualPeeledCommit -ne $vcpkgCommit) {
    throw "Expected vcpkg tag $vcpkgTag to peel to $vcpkgCommit, found '$actualPeeledCommit'."
}

$actualHead = Get-GitValue -Arguments @("rev-parse", "HEAD") -Description "HEAD"
if ($actualHead -ne $vcpkgCommit) {
    throw "Expected vcpkg HEAD $vcpkgCommit, found '$actualHead'."
}

$trackedChanges = Get-GitValue -Arguments @("status", "--porcelain", "--untracked-files=no") `
    -Description "tracked working-tree status"
if (-not [string]::IsNullOrWhiteSpace($trackedChanges)) {
    throw "The vcpkg checkout has modified or staged tracked files. Restore the checkout before bootstrapping: $trackedChanges"
}

$indexEntries = @(Get-GitLines -Arguments @("ls-files", "-v") -Description "tracked index flags")
foreach ($entry in $indexEntries) {
    if ($entry.Length -lt 3 -or $entry[1] -ne ' ') {
        throw "Could not parse tracked index flags from the vcpkg checkout at $resolvedRoot."
    }

    $indexTag = $entry[0]
    if ($indexTag -ceq 'h' -or $indexTag -ceq 'S' -or $indexTag -ceq 's') {
        throw "The vcpkg checkout has assume-unchanged or skip-worktree tracked files. Restore the checkout before bootstrapping: $entry"
    }
}

if ($VerifyCheckoutOnly) {
    return
}

& $bootstrap -disableMetrics
if ($LASTEXITCODE -ne 0) {
    throw "vcpkg bootstrap failed with exit code $LASTEXITCODE."
}

& $executable install --triplet x64-windows "--x-manifest-root=$repositoryRoot"
if ($LASTEXITCODE -ne 0) {
    throw "vcpkg dependency installation failed with exit code $LASTEXITCODE."
}
