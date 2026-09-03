[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CMakeCommand,

    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$temporaryParent = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$temporaryRoot = Join-Path $temporaryParent ("LandscapeCutter-sdk-floor-test-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null

try {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $CMakeCommand
    $startInfo.Arguments = "-S `"$SourceRoot`" -B `"$temporaryRoot`" `"-DLC_MIN_WINDOWS_SDK_VERSION=10.0.10240.0`""
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::Start($startInfo)
    $standardOutput = $process.StandardOutput.ReadToEnd()
    $standardError = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $output = $standardOutput + $standardError

    if ($process.ExitCode -eq 0) {
        throw "CMake accepted an SDK floor below 10.0.19041.0."
    }
    if ($output -notmatch [regex]::Escape("cannot be lowered below 10.0.19041.0")) {
        throw "CMake failed for an unrelated reason instead of rejecting the lowered SDK floor:`n$output"
    }

    Write-Output "CMake rejected an attempt to lower the Windows SDK floor below 10.0.19041.0."
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $temporaryRoot).Path)
        if (-not $resolvedTemporaryRoot.StartsWith($temporaryParent, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test directory: $resolvedTemporaryRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
