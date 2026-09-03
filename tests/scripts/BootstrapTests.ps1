[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BootstrapScript,

    [Parameter(Mandatory = $true)]
    [string]$KnownGoodVcpkgRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$expectedTag = "2026.07.29"
$expectedTagObject = "c76c06644034521fb761a39f8f52d8e87d1103d5"
$expectedCommit = "9e593bb18ea69cc5095e012465dcd675a822ed0d"
$temporaryParent = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$temporaryRoot = Join-Path $temporaryParent ("LandscapeCutter-bootstrap-tests-" + [guid]::NewGuid())
$failures = [System.Collections.Generic.List[string]]::new()

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = & git -C $Repository @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "git $($Arguments -join ' ') failed in ${Repository}: $($output -join [Environment]::NewLine)"
    }
    return $output
}

function New-FakeVcpkgExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OutputPath
    )

    $source = @'
using System;
using System.IO;

public static class FakeVcpkg
{
    public static int Main(string[] args)
    {
        var marker = Environment.GetEnvironmentVariable("LC_BOOTSTRAP_TEST_VCPKG_MARKER");
        if (!String.IsNullOrEmpty(marker))
        {
            File.WriteAllText(marker, String.Join(" ", args));
        }
        return 0;
    }
}
'@

    Add-Type -TypeDefinition $source -Language CSharp -OutputAssembly $OutputPath `
        -OutputType ConsoleApplication
}

function Set-FakeProvisioningCommands {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string]$FakeVcpkgExecutable
    )

    $bootstrapPath = Join-Path $Repository "bootstrap-vcpkg.bat"
    @(
        "@echo off",
        'if not "%LC_BOOTSTRAP_TEST_BOOTSTRAP_MARKER%"=="" echo called>"%LC_BOOTSTRAP_TEST_BOOTSTRAP_MARKER%"',
        "exit /b 0"
    ) | Set-Content -LiteralPath $bootstrapPath -Encoding Ascii
    Copy-Item -LiteralPath $FakeVcpkgExecutable -Destination (Join-Path $Repository "vcpkg.exe")
}

function New-SyntheticRepository {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    New-Item -ItemType Directory -Path $Path | Out-Null
    & git -C $Path init --initial-branch=main | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "git init failed for $Path"
    }
    Invoke-Git -Repository $Path -Arguments @("config", "user.name", "Bootstrap Test") | Out-Null
    Invoke-Git -Repository $Path -Arguments @("config", "user.email", "bootstrap-test@example.invalid") | Out-Null
    "tracked" | Set-Content -LiteralPath (Join-Path $Path "tracked.txt") -Encoding Ascii
    "@echo off`r`nexit /b 0" | Set-Content -LiteralPath (Join-Path $Path "bootstrap-vcpkg.bat") -Encoding Ascii
    Invoke-Git -Repository $Path -Arguments @("add", "tracked.txt", "bootstrap-vcpkg.bat") | Out-Null
    Invoke-Git -Repository $Path -Arguments @("commit", "-m", "initial") | Out-Null
    Invoke-Git -Repository $Path -Arguments @("tag", "-a", $expectedTag, "-m", "original tag") | Out-Null
}

function Invoke-Bootstrap {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string]$MarkerPrefix
    )

    $bootstrapMarker = "$MarkerPrefix-bootstrap.txt"
    $vcpkgMarker = "$MarkerPrefix-vcpkg.txt"
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = "powershell.exe"
    $startInfo.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$BootstrapScript`" -VcpkgRoot `"$Repository`""
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.EnvironmentVariables["LC_BOOTSTRAP_TEST_BOOTSTRAP_MARKER"] = $bootstrapMarker
    $startInfo.EnvironmentVariables["LC_BOOTSTRAP_TEST_VCPKG_MARKER"] = $vcpkgMarker

    $process = [System.Diagnostics.Process]::Start($startInfo)
    $standardOutput = $process.StandardOutput.ReadToEnd()
    $standardError = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        Output = ($standardOutput + $standardError).Trim()
        BootstrapCalled = Test-Path -LiteralPath $bootstrapMarker
        VcpkgCalled = Test-Path -LiteralPath $vcpkgMarker
    }
}

function Add-Failure {
    param([string]$Message)
    $failures.Add($Message)
    Write-Error -Message $Message -ErrorAction Continue
}

New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try {
    $fakeVcpkg = Join-Path $temporaryRoot "fake-vcpkg.exe"
    New-FakeVcpkgExecutable -OutputPath $fakeVcpkg

    $knownGoodRoot = [System.IO.Path]::GetFullPath($KnownGoodVcpkgRoot)
    if ((Invoke-Git -Repository $knownGoodRoot -Arguments @("rev-parse", "refs/tags/$expectedTag")) -ne $expectedTagObject) {
        throw "Known-good vcpkg checkout does not contain the expected annotated tag object."
    }
    if ((Invoke-Git -Repository $knownGoodRoot -Arguments @("rev-parse", "refs/tags/$expectedTag^{commit}")) -ne $expectedCommit) {
        throw "Known-good vcpkg checkout does not contain the expected peeled commit."
    }

    $retaggedRepository = Join-Path $temporaryRoot "retagged"
    New-SyntheticRepository -Path $retaggedRepository
    "retagged" | Set-Content -LiteralPath (Join-Path $retaggedRepository "tracked.txt") -Encoding Ascii
    Invoke-Git -Repository $retaggedRepository -Arguments @("add", "tracked.txt") | Out-Null
    Invoke-Git -Repository $retaggedRepository -Arguments @("commit", "-m", "retag target") | Out-Null
    Invoke-Git -Repository $retaggedRepository -Arguments @(
        "tag", "-f", "-a", $expectedTag, "-m", "locally retagged"
    ) | Out-Null
    Set-FakeProvisioningCommands -Repository $retaggedRepository -FakeVcpkgExecutable $fakeVcpkg
    Invoke-Git -Repository $retaggedRepository -Arguments @("update-index", "--assume-unchanged", "bootstrap-vcpkg.bat") | Out-Null
    $retaggedResult = Invoke-Bootstrap -Repository $retaggedRepository `
        -MarkerPrefix (Join-Path $temporaryRoot "retagged")
    if ($retaggedResult.ExitCode -eq 0 -or $retaggedResult.BootstrapCalled -or $retaggedResult.VcpkgCalled) {
        Add-Failure "A locally recreated tag with the expected name was accepted and provisioning ran."
    }

    $dirtyRepository = Join-Path $temporaryRoot "dirty"
    & git clone --quiet --shared --no-checkout $knownGoodRoot $dirtyRepository
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create the isolated tracked-dirty checkout."
    }
    Invoke-Git -Repository $dirtyRepository -Arguments @(
        "sparse-checkout", "set", "--no-cone", "/bootstrap-vcpkg.bat", "/README.md"
    ) | Out-Null
    Invoke-Git -Repository $dirtyRepository -Arguments @("checkout", "--quiet", "--detach", $expectedCommit) | Out-Null
    Invoke-Git -Repository $dirtyRepository -Arguments @("update-index", "--assume-unchanged", "bootstrap-vcpkg.bat") | Out-Null
    Set-FakeProvisioningCommands -Repository $dirtyRepository -FakeVcpkgExecutable $fakeVcpkg
    "dirty tracked content" | Add-Content -LiteralPath (Join-Path $dirtyRepository "README.md") -Encoding Ascii
    if ([string]::IsNullOrWhiteSpace((Invoke-Git -Repository $dirtyRepository -Arguments @(
        "status", "--porcelain", "--untracked-files=no"
    )))) {
        throw "Tracked-dirty fixture was not dirty."
    }
    $dirtyResult = Invoke-Bootstrap -Repository $dirtyRepository `
        -MarkerPrefix (Join-Path $temporaryRoot "dirty")
    if ($dirtyResult.ExitCode -eq 0 -or $dirtyResult.BootstrapCalled -or $dirtyResult.VcpkgCalled) {
        Add-Failure "A checkout with a modified tracked file was accepted and provisioning ran."
    }

    $stagedRepository = Join-Path $temporaryRoot "staged"
    & git clone --quiet --shared --no-checkout $knownGoodRoot $stagedRepository
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create the isolated staged-dirty checkout."
    }
    Invoke-Git -Repository $stagedRepository -Arguments @(
        "sparse-checkout", "set", "--no-cone", "/bootstrap-vcpkg.bat", "/README.md"
    ) | Out-Null
    Invoke-Git -Repository $stagedRepository -Arguments @("checkout", "--quiet", "--detach", $expectedCommit) | Out-Null
    Invoke-Git -Repository $stagedRepository -Arguments @("update-index", "--assume-unchanged", "bootstrap-vcpkg.bat") | Out-Null
    Set-FakeProvisioningCommands -Repository $stagedRepository -FakeVcpkgExecutable $fakeVcpkg
    "staged tracked content" | Add-Content -LiteralPath (Join-Path $stagedRepository "README.md") -Encoding Ascii
    Invoke-Git -Repository $stagedRepository -Arguments @("add", "README.md") | Out-Null
    $stagedResult = Invoke-Bootstrap -Repository $stagedRepository `
        -MarkerPrefix (Join-Path $temporaryRoot "staged")
    if ($stagedResult.ExitCode -eq 0 -or $stagedResult.BootstrapCalled -or $stagedResult.VcpkgCalled) {
        Add-Failure "A checkout with a staged tracked file was accepted and provisioning ran."
    }

    $goodRepository = Join-Path $temporaryRoot "known-good"
    & git clone --quiet --shared --no-checkout $knownGoodRoot $goodRepository
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create the isolated known-good checkout."
    }
    Invoke-Git -Repository $goodRepository -Arguments @(
        "sparse-checkout", "set", "--no-cone", "/bootstrap-vcpkg.bat", "/README.md"
    ) | Out-Null
    Invoke-Git -Repository $goodRepository -Arguments @("checkout", "--quiet", "--detach", $expectedCommit) | Out-Null
    Invoke-Git -Repository $goodRepository -Arguments @("update-index", "--assume-unchanged", "bootstrap-vcpkg.bat") | Out-Null
    Set-FakeProvisioningCommands -Repository $goodRepository -FakeVcpkgExecutable $fakeVcpkg
    "allowed" | Set-Content -LiteralPath (Join-Path $goodRepository "untracked-is-allowed.txt") -Encoding Ascii
    $goodResult = Invoke-Bootstrap -Repository $goodRepository `
        -MarkerPrefix (Join-Path $temporaryRoot "known-good")
    if ($goodResult.ExitCode -ne 0 -or -not $goodResult.BootstrapCalled -or -not $goodResult.VcpkgCalled) {
        Add-Failure "The authentic tag object/commit checkout did not continue through both provisioning commands: $($goodResult.Output)"
    }

    if ($failures.Count -gt 0) {
        throw "$($failures.Count) bootstrap behavior test(s) failed."
    }

    Write-Output "Bootstrap identity behavior passed: retagged, unstaged-dirty, and staged-dirty checkouts rejected; authentic checkout with an untracked file continued."
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $temporaryRoot).Path)
        if (-not $resolvedTemporaryRoot.StartsWith($temporaryParent, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test directory: $resolvedTemporaryRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
