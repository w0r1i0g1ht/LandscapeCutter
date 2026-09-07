[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Application,
    [Parameter(Mandatory = $true)][string]$HotkeyOccupier
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (Get-Process LandscapeCutter -ErrorAction SilentlyContinue) {
    Write-Output 'SKIP: an existing LandscapeCutter instance belongs to the user.'
    exit 125
}

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LandscapeProcessTestNative {
    private delegate bool WindowCallback(IntPtr hwnd, IntPtr state);
    [DllImport("user32.dll")] private static extern bool EnumWindows(WindowCallback callback, IntPtr state);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder name, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern IntPtr OpenMutex(uint access, bool inherit, string name);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern IntPtr OpenEvent(uint access, bool inherit, string name);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
    public static IntPtr FindWindow(int processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hwnd, state) => {
            uint owner; GetWindowThreadProcessId(hwnd, out owner);
            if (owner != processId) return true;
            var name = new StringBuilder(256); GetClassName(hwnd, name, name.Capacity);
            if (name.ToString() != "LandscapeCutter.NativeMessageWindow") return true;
            found = hwnd; return false;
        }, IntPtr.Zero);
        return found;
    }
    public static bool ProductObjectsAbsent() {
        const string prefix = @"Local\LandscapeCutter.4F4E6D0D-8C33-4B79-984A-3E44F6A62D11";
        IntPtr mutex = OpenMutex(0x100000, false, prefix + ".Instance");
        int mutexError = Marshal.GetLastWin32Error();
        IntPtr signal = OpenEvent(0x100000, false, prefix + ".Activate");
        int eventError = Marshal.GetLastWin32Error();
        if (mutex != IntPtr.Zero) CloseHandle(mutex);
        if (signal != IntPtr.Zero) CloseHandle(signal);
        return mutex == IntPtr.Zero && signal == IntPtr.Zero && mutexError == 2 && eventError == 2;
    }
}
'@

function Start-TestProduct {
    Start-Process -FilePath $Application -WindowStyle Hidden -PassThru
}
function Wait-TestWindow([Diagnostics.Process]$Product) {
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        if ($Product.HasExited) { throw "Product exited early: $($Product.ExitCode)" }
        $window = [LandscapeProcessTestNative]::FindWindow($Product.Id)
        if ($window -ne [IntPtr]::Zero) { return $window }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Product did not create its native message window.'
}
function Close-TestProduct([Diagnostics.Process]$Product) {
    if ($null -eq $Product) { return }
    if (-not $Product.HasExited) {
        $window = [LandscapeProcessTestNative]::FindWindow($Product.Id)
        if ($window -ne [IntPtr]::Zero) {
            [void][LandscapeProcessTestNative]::PostMessage($window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        if (-not $Product.WaitForExit(5000)) {
            # Only this test-created Process object is eligible for fallback cleanup.
            $Product.Kill()
            $Product.WaitForExit()
            throw 'Test product failed graceful shutdown.'
        }
    }
    if ($Product.ExitCode -ne 0) { throw "Product exited with $($Product.ExitCode)" }
    if ([LandscapeProcessTestNative]::FindWindow($Product.Id) -ne [IntPtr]::Zero) {
        throw 'Test native message window remains after shutdown.'
    }
}

$primary = $null
$secondary = $null
$helper = $null
$ready = $null
$stop = $null
try {
    $primary = Start-TestProduct
    [void](Wait-TestWindow $primary)
    $secondary = Start-TestProduct
    if (-not $secondary.WaitForExit(3000)) { throw 'Secondary product did not exit within 3 seconds.' }
    if ($secondary.ExitCode -ne 0 -or $primary.HasExited) { throw 'Single-instance protocol failed.' }
    Close-TestProduct $primary
    if (-not [LandscapeProcessTestNative]::ProductObjectsAbsent()) { throw 'Product named objects remain.' }

    $testId = [Guid]::NewGuid().ToString('N')
    $readyName = "Local\LandscapeCutter.Test.$testId.Ready"
    $stopName = "Local\LandscapeCutter.Test.$testId.Stop"
    $ready = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset, $readyName)
    $stop = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset, $stopName)
    $helper = Start-Process -FilePath $HotkeyOccupier -ArgumentList @($readyName, $stopName) -WindowStyle Hidden -PassThru
    if (-not $ready.WaitOne(5000)) {
        if ($helper.HasExited -and $helper.ExitCode -eq 125) { Write-Output 'SKIP: F2 already occupied.'; exit 125 }
        throw 'Hotkey helper did not become ready.'
    }
    $primary = Start-TestProduct
    [void](Wait-TestWindow $primary)
    if ($primary.WaitForExit(3000)) { throw 'Product must remain running despite F2 conflict.' }
    Close-TestProduct $primary
    if (-not [LandscapeProcessTestNative]::ProductObjectsAbsent()) { throw 'Product objects remain after conflict test.' }
    [void]$stop.Set()
    if (-not $helper.WaitForExit(5000) -or $helper.ExitCode -ne 0) { throw 'Hotkey helper did not exit normally.' }
    Write-Output 'PASS: secondary exits, F2 conflict preserves process, owned windows and named objects are released.'
} finally {
    if ($stop) { [void]$stop.Set() }
    try { Close-TestProduct $secondary } finally {
        try { Close-TestProduct $primary } finally {
            if ($helper -and -not $helper.HasExited -and -not $helper.WaitForExit(5000)) { $helper.Kill(); $helper.WaitForExit() }
            if ($ready) { $ready.Dispose() }
            if ($stop) { $stop.Dispose() }
        }
    }
}
