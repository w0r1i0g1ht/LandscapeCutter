# LandscapeCutter

LandscapeCutter is an open-source Windows snipping, pinning, and live-region
monitoring tool. It is inspired by the efficient workflow of Snipaste while
remaining an independent project with its own implementation and brand.

## Status

The project is being rewritten in C++20 and Qt 6.8. Milestone 0 provides the
build system, tests, and tray application shell. Static capture, annotation,
static pins, and live window-relative pins are delivered in subsequent
milestones.

LandscapeCutter is not affiliated with or endorsed by Snipaste.

## Requirements

- Windows 10 1903 or later, including Windows 11
- x64 processor and operating system
- Visual Studio 2026 with the MSVC 14.5x C++ workload
- Windows SDK 10.0.19041 or newer
- CMake 4.4 or newer
- Git and PowerShell

Qt 6.8.2, Catch2, spdlog, and WIL are installed through the pinned vcpkg
bootstrap script.

## Configure and build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

## Test

```powershell
ctest --preset windows-msvc-debug
```

## Run

```powershell
& .\out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe
```

The foundation build starts as a system-tray application. Select `退出` from
the tray menu to close it.

## Design and roadmap

The approved C++ product and architecture design is in
[`docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md`](docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md).

The original Python prototype is preserved by the annotated Git tag
`python-prototype-final`. See [`legacy/README.md`](legacy/README.md) for safe
inspection instructions.

## License

The repository retains its existing license status until a dedicated license
file is added as part of release preparation. Do not redistribute binaries as
an official release before that decision is recorded.
