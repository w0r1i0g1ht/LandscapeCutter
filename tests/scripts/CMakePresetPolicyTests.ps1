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
$temporaryRoot = Join-Path $temporaryParent ("LandscapeCutter-preset-policy-" + [guid]::NewGuid())
$fakeToolchain = Join-Path $temporaryRoot ".tools/vcpkg/scripts/buildsystems/vcpkg.cmake"

New-Item -ItemType Directory -Path (Split-Path -Parent $fakeToolchain) -Force | Out-Null

try {
    Copy-Item -LiteralPath (Join-Path $SourceRoot "CMakePresets.json") -Destination $temporaryRoot
    Set-Content -LiteralPath $fakeToolchain -Encoding utf8 -Value @'
set(LC_PRESET_TOOLCHAIN_SENTINEL "portable-source-dir-toolchain" CACHE STRING "")
'@
    Set-Content -LiteralPath (Join-Path $temporaryRoot "CMakeLists.txt") -Encoding utf8 -Value @'
cmake_minimum_required(VERSION 4.4)
project(LandscapeCutterPresetPolicy LANGUAGES NONE)
if(NOT LC_PRESET_TOOLCHAIN_SENTINEL STREQUAL "portable-source-dir-toolchain")
    message(FATAL_ERROR "The configured toolchain did not resolve from the relocated source directory.")
endif()
'@

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $CMakeCommand
    $startInfo.WorkingDirectory = $temporaryRoot
    $startInfo.Arguments = "--preset windows-msvc-debug"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::Start($startInfo)
    $standardOutput = $process.StandardOutput.ReadToEnd()
    $standardError = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $output = $standardOutput + $standardError

    if ($process.ExitCode -ne 0) {
        throw "The Windows preset did not resolve its toolchain from a relocated checkout:`n$output"
    }

    Write-Output "The Windows preset resolved its toolchain from the relocated source directory."
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $temporaryRoot).Path)
        if (-not $resolvedTemporaryRoot.StartsWith($temporaryParent, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test directory: $resolvedTemporaryRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
