# Milestone 0 C++ Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve the final Python prototype in Git, replace the active runtime with a reproducible C++20/Qt 6.8 application shell, and leave the repository building and testing cleanly on Windows x64.

**Architecture:** This milestone establishes only the application shell and engineering foundation. A Qt Widgets tray process is built through CMake and a repository-local, tag-pinned vcpkg checkout; feature modules for capture, annotation, and pinning remain outside this plan and will be added in later milestone plans.

**Tech Stack:** Visual Studio 2022 MSVC v143, C++20, Windows SDK 10.0.19041+, Qt 6.8.2 dynamic libraries, CMake 3.28+, vcpkg 2025.02.14, WIL, spdlog, Catch2.

**Spec:** `docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md`

## Global Constraints

- Target Windows 10 1903 and later, including Windows 11.
- Publish and test x64 only in the first release.
- Use Qt 6.8 series through LGPL-compatible dynamic linking.
- Use MSVC 2022, the C++20 language standard, and Windows SDK 10.0.19041 or newer.
- Keep Windows-specific APIs behind focused modules; this milestone contains no capture implementation.
- Never log screenshots, window content, clipboard content, or annotation text.
- Preserve the Python prototype through the annotated Git tag `python-prototype-final` before deleting tracked Python artifacts.
- Keep the product name `LandscapeCutter`; describe Snipaste only as inspiration and do not imply affiliation.
- Use physical pixels for future capture data, but introduce no coordinate model in this foundation milestone.

## Plan Scope

This is the first of seven implementation plans. It implements Milestone 0 only. The following independently testable plans will cover Windows graphics foundations, static snipping, annotation, static pins, live pins, and productization.

## Target File Map

```text
LandscapeCutter/
├─ CMakeLists.txt                         # Root build policy and dependencies
├─ CMakePresets.json                      # Reproducible Windows x64 presets
├─ vcpkg.json                             # Manifest dependencies
├─ .clang-format                          # C++ formatting contract
├─ .gitignore                             # Build, vcpkg, IDE, and runtime artifacts
├─ cmake/
│  └─ Warnings.cmake                      # Project-only MSVC warning policy
├─ scripts/
│  └─ bootstrap.ps1                       # Pin and bootstrap vcpkg 2025.02.14
├─ src/
│  ├─ CMakeLists.txt                      # Application targets
│  ├─ main.cpp                            # QApplication entry point
│  └─ app/
│     ├─ AppController.hpp                # Tray application ownership boundary
│     ├─ AppController.cpp                # Tray menu and lifecycle behavior
│     ├─ AppMetadata.hpp                  # Stable product metadata
│     ├─ LaunchOptions.hpp                # Launch-mode public contract
│     └─ LaunchOptions.cpp                # Command-line parsing
├─ tests/
│  ├─ CMakeLists.txt                      # Catch2 and process smoke tests
│  └─ app/
│     ├─ AppMetadataTests.cpp             # Product metadata contract
│     └─ LaunchOptionsTests.cpp           # Launch parsing contract
├─ resources/
│  └─ resources.qrc                       # Qt icon resources
└─ legacy/
   └─ README.md                           # Python tag and migration record
```

---

### Task 1: Preserve the Python prototype

**Files:**
- Create: `legacy/README.md`
- Verify: `docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md`

**Interfaces:**
- Consumes: current clean Git `HEAD` containing the approved C++ rewrite design.
- Produces: annotated tag `python-prototype-final` and a permanent recovery note.

- [ ] **Step 1: Verify that the prototype can be tagged safely**

Run:

```powershell
git status --porcelain
git tag --list python-prototype-final
```

Expected: both commands print no output. If the worktree is dirty, stop and identify every path before continuing. If the tag already exists, run `git show --stat python-prototype-final` and reuse it only when it resolves to the current Python prototype.

- [ ] **Step 2: Create the annotated recovery tag**

Run:

```powershell
git tag -a python-prototype-final -m "Final Python prototype before the C++ rewrite"
```

Expected: exit code 0.

- [ ] **Step 3: Verify the tag target and tracked prototype files**

Run:

```powershell
git show --no-patch --decorate python-prototype-final
git ls-tree -r --name-only python-prototype-final -- python requirements.txt config.json
```

Expected: the tag resolves to the approved design-era `HEAD`; the second command lists the Python sources, requirements, configuration, tracked bytecode, and the legacy `.pyd` binary.

- [ ] **Step 4: Create the migration record with `apply_patch`**

Create `legacy/README.md` with this exact content:

````markdown
# Legacy Python prototype

The final Python implementation is preserved by the annotated Git tag
`python-prototype-final`.

To inspect it without changing the current branch:

```powershell
git show python-prototype-final:README.md
git ls-tree -r --name-only python-prototype-final
```

To run or modify the prototype, create a separate branch or worktree from the
tag. The active `main` branch contains only the C++ implementation and does not
maintain compatibility with the Python file layout or APIs.
````

- [ ] **Step 5: Commit the recovery note**

Run:

```powershell
git add legacy/README.md
git commit -m "docs: preserve Python prototype recovery path"
```

Expected: one commit containing only `legacy/README.md`; the annotated tag remains visible through `git tag --list`.

---

### Task 2: Establish the reproducible build toolchain

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `vcpkg.json`
- Create: `.clang-format`
- Create: `cmake/Warnings.cmake`
- Create: `scripts/bootstrap.ps1`
- Create: `src/CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: Visual Studio 2022 at `D:\IDE\VisualStudio\Community` and CMake at `C:\Program Files\CMake\bin\cmake.exe`.
- Produces: `windows-msvc-debug` configure/build/test presets and repository-local `.tools/vcpkg` dependencies.

- [ ] **Step 1: Extend `.gitignore` with C++ build artifacts**

Append this exact block with `apply_patch`:

```gitignore

# C++ / CMake
.tools/
out/
CMakeUserPresets.json
CMakeFiles/
CMakeCache.txt
cmake_install.cmake
CTestTestfile.cmake
Testing/
compile_commands.json
vcpkg_installed/
*.vcxproj
*.vcxproj.filters
*.vcxproj.user
*.ilk
*.pdb
*.obj
*.tlog
```

- [ ] **Step 2: Define manifest dependencies**

Create `vcpkg.json`:

```json
{
  "name": "landscapecutter",
  "version-string": "0.1.0-dev",
  "description": "GPU-accelerated Windows snipping, pinning, and live-region monitoring tool",
  "supports": "windows & x64",
  "dependencies": [
    "catch2",
    {
      "name": "qtbase",
      "default-features": false,
      "features": [
        "gui",
        "jpeg",
        "png",
        "widgets"
      ]
    },
    "spdlog",
    "wil"
  ]
}
```

The vcpkg checkout tag, rather than an unpinned rolling installation, fixes `qtbase` at 6.8.2.

- [ ] **Step 3: Add the vcpkg bootstrap script**

Create `scripts/bootstrap.ps1`:

```powershell
[CmdletBinding()]
param(
    [string]$VcpkgRoot = (Join-Path $PSScriptRoot "..\.tools\vcpkg")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$vcpkgTag = "2025.02.14"
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
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
```

- [ ] **Step 4: Define the CMake presets**

Create `CMakePresets.json`:

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 28,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "windows-msvc-debug",
      "displayName": "Windows x64 MSVC Debug",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/out/build/windows-msvc-debug",
      "cacheVariables": {
        "BUILD_TESTING": "ON",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "windows-msvc-debug",
      "configurePreset": "windows-msvc-debug",
      "configuration": "Debug"
    }
  ],
  "testPresets": [
    {
      "name": "windows-msvc-debug",
      "configurePreset": "windows-msvc-debug",
      "configuration": "Debug",
      "output": {
        "outputOnFailure": true
      }
    }
  ]
}
```

- [ ] **Step 5: Define project warnings and formatting**

Create `cmake/Warnings.cmake`:

```cmake
function(lc_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /EHsc
        )
    endif()
endfunction()
```

Create `.clang-format`:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
PointerAlignment: Left
DerivePointerAlignment: false
SortIncludes: CaseSensitive
AllowShortFunctionsOnASingleLine: Empty
```

- [ ] **Step 6: Define the root build contract**

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.28)

project(LandscapeCutter VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "LandscapeCutter currently supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LandscapeCutter currently supports x64 builds only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.9.0")
    message(FATAL_ERROR "Milestone 0 is pinned to the Qt 6.8 series.")
endif()
message(STATUS "Using Qt ${Qt6_VERSION}")

find_package(spdlog CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

include(cmake/Warnings.cmake)
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
```

Create `src/CMakeLists.txt`:

```cmake
# Application targets are introduced after the toolchain contract is verified.
```

Create `tests/CMakeLists.txt`:

```cmake
# Test targets are introduced with their corresponding production interfaces.
```

- [ ] **Step 7: Bootstrap dependencies**

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
```

Expected: `.tools/vcpkg/vcpkg.exe` exists; vcpkg reports `qtbase:x64-windows` version 6.8.2 and completes all four manifest dependencies. This command downloads and builds dependencies, so execution requires network approval.

- [ ] **Step 8: Configure and build the empty project**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

Expected: configure reports Qt 6.8.2; build exits successfully without compiling an application target.

- [ ] **Step 9: Commit the build foundation**

Run:

```powershell
git add .gitignore .clang-format CMakeLists.txt CMakePresets.json vcpkg.json cmake scripts src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "build: establish C++20 Qt toolchain"
```

Expected: one build-system commit; `.tools` and `out` remain untracked and ignored.

---

### Task 3: Add test-driven application contracts

**Files:**
- Create: `src/app/AppMetadata.hpp`
- Create: `src/app/LaunchOptions.hpp`
- Create: `src/app/LaunchOptions.cpp`
- Create: `tests/app/AppMetadataTests.cpp`
- Create: `tests/app/LaunchOptionsTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Qt 6 Core, the C++20 build contract, and Catch2 3.
- Produces: `lc::app::AppMetadata`, `lc::app::LaunchMode`, and `lc::app::parseLaunchMode(std::span<const std::string_view>)`.

- [ ] **Step 1: Write failing metadata and launch-option tests**

Create `tests/app/AppMetadataTests.cpp`:

```cpp
#include "app/AppMetadata.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("application metadata is stable") {
    CHECK(lc::app::AppMetadata::name == "LandscapeCutter");
    CHECK(lc::app::AppMetadata::organization == "LandscapeCutter");
    CHECK(lc::app::AppMetadata::version == "0.1.0-dev");
}
```

Create `tests/app/LaunchOptionsTests.cpp`:

```cpp
#include "app/LaunchOptions.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("normal launch is the default") {
    constexpr std::array arguments{"LandscapeCutter"sv};
    CHECK(lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::Normal);
}

TEST_CASE("smoke-test flag selects smoke mode") {
    constexpr std::array arguments{"LandscapeCutter"sv, "--smoke-test"sv};
    CHECK(lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::SmokeTest);
}

TEST_CASE("unrelated arguments do not select smoke mode") {
    constexpr std::array arguments{"LandscapeCutter"sv, "--verbose"sv};
    CHECK(lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::Normal);
}
```

Add `using namespace std::string_view_literals;` immediately after the includes in `LaunchOptionsTests.cpp` so the `sv` literals compile.

Replace `tests/CMakeLists.txt` with:

```cmake
add_executable(landscapecutter_unit_tests
    app/AppMetadataTests.cpp
    app/LaunchOptionsTests.cpp
)

target_link_libraries(landscapecutter_unit_tests PRIVATE
    Catch2::Catch2WithMain
    landscapecutter_app_core
)

lc_enable_warnings(landscapecutter_unit_tests)

include(Catch)
catch_discover_tests(landscapecutter_unit_tests)
```

- [ ] **Step 2: Run the build to verify the tests fail**

Run:

```powershell
cmake --build --preset windows-msvc-debug
```

Expected: configure or build fails because `landscapecutter_app_core`, `AppMetadata.hpp`, and `LaunchOptions.hpp` do not exist.

- [ ] **Step 3: Implement the minimal contracts**

Create `src/app/AppMetadata.hpp`:

```cpp
#pragma once

#include <string_view>

namespace lc::app {

struct AppMetadata final {
    static constexpr std::string_view name{"LandscapeCutter"};
    static constexpr std::string_view organization{"LandscapeCutter"};
    static constexpr std::string_view version{"0.1.0-dev"};
};

} // namespace lc::app
```

Create `src/app/LaunchOptions.hpp`:

```cpp
#pragma once

#include <span>
#include <string_view>

namespace lc::app {

enum class LaunchMode {
    Normal,
    SmokeTest,
};

[[nodiscard]] LaunchMode parseLaunchMode(std::span<const std::string_view> arguments);

} // namespace lc::app
```

Create `src/app/LaunchOptions.cpp`:

```cpp
#include "app/LaunchOptions.hpp"

#include <algorithm>

namespace lc::app {

LaunchMode parseLaunchMode(const std::span<const std::string_view> arguments) {
    const auto smokeTest = std::ranges::find(arguments, "--smoke-test");
    return smokeTest == arguments.end() ? LaunchMode::Normal : LaunchMode::SmokeTest;
}

} // namespace lc::app
```

Replace `src/CMakeLists.txt` with:

```cmake
add_library(landscapecutter_app_core STATIC
    app/LaunchOptions.cpp
)

target_include_directories(landscapecutter_app_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(landscapecutter_app_core PUBLIC
    Qt6::Core
)

lc_enable_warnings(landscapecutter_app_core)
```

- [ ] **Step 4: Build and run the focused tests**

Run:

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R "application metadata|launch" 
```

Expected: three Catch2 test cases pass.

- [ ] **Step 5: Format and re-run all tests**

Run the verified Visual Studio LLVM formatter:

```powershell
& 'D:\IDE\VisualStudio\Community\VC\Tools\Llvm\x64\bin\clang-format.exe' -i src\app\AppMetadata.hpp src\app\LaunchOptions.hpp src\app\LaunchOptions.cpp tests\app\AppMetadataTests.cpp tests\app\LaunchOptionsTests.cpp
ctest --preset windows-msvc-debug
```

Expected: formatting changes no behavior; all discovered tests pass.

- [ ] **Step 6: Commit the tested contracts**

Run:

```powershell
git add src/app src/CMakeLists.txt tests/app tests/CMakeLists.txt
git commit -m "test: define application launch contracts"
```

Expected: production interfaces and their tests land in the same commit.

---

### Task 4: Build the Qt tray application shell

**Files:**
- Create: `src/app/AppController.hpp`
- Create: `src/app/AppController.cpp`
- Create: `src/main.cpp`
- Create: `resources/resources.qrc`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `lc::app::AppMetadata`, `lc::app::LaunchMode`, and `lc::app::parseLaunchMode` from Task 3.
- Produces: `lc::app::AppController(QApplication&)`, `bool AppController::start()`, the `LandscapeCutter.exe` GUI target, and the `app_process_smoke` CTest.

- [ ] **Step 1: Add a failing executable smoke test**

Append to `tests/CMakeLists.txt`:

```cmake
add_test(NAME app_process_smoke COMMAND LandscapeCutter --smoke-test)
set_tests_properties(app_process_smoke PROPERTIES TIMEOUT 10)
```

- [ ] **Step 2: Run CMake to verify the smoke test cannot resolve the executable**

Run:

```powershell
cmake --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R app_process_smoke
```

Expected: the test is not runnable because the `LandscapeCutter` executable target does not exist.

- [ ] **Step 3: Add the Qt resource file**

Create `resources/resources.qrc`:

```xml
<RCC>
    <qresource prefix="/">
        <file alias="icons/LandscapeCutter.ico">../assets/LandscapeCutter.ico</file>
    </qresource>
</RCC>
```

- [ ] **Step 4: Implement the tray controller**

Create `src/app/AppController.hpp`:

```cpp
#pragma once

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class QApplication;

namespace lc::app {

class AppController final : public QObject {
public:
    explicit AppController(QApplication& application);

    [[nodiscard]] bool start();

private:
    QApplication& application_;
    QMenu trayMenu_;
    QAction quitAction_;
    QSystemTrayIcon trayIcon_;
};

} // namespace lc::app
```

Create `src/app/AppController.cpp`:

```cpp
#include "app/AppController.hpp"

#include <QApplication>
#include <QIcon>

#include <spdlog/spdlog.h>

namespace lc::app {

AppController::AppController(QApplication& application)
    : application_(application), quitAction_(tr("退出")) {
    trayMenu_.addAction(&quitAction_);
    trayIcon_.setContextMenu(&trayMenu_);

    connect(&quitAction_, &QAction::triggered, &application_, &QCoreApplication::quit);
}

bool AppController::start() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        spdlog::error("System tray is not available");
        return false;
    }

    const QIcon icon(QStringLiteral(":/icons/LandscapeCutter.ico"));
    if (icon.isNull()) {
        spdlog::error("Application icon resource is unavailable");
        return false;
    }

    trayIcon_.setIcon(icon);
    trayIcon_.setToolTip(QStringLiteral("LandscapeCutter"));
    trayIcon_.show();
    trayIcon_.showMessage(QStringLiteral("LandscapeCutter"),
                          QStringLiteral("C++ foundation is running."),
                          QSystemTrayIcon::Information,
                          2000);
    return true;
}

} // namespace lc::app
```

- [ ] **Step 5: Implement the application entry point**

Create `src/main.cpp`:

```cpp
#include "app/AppController.hpp"
#include "app/AppMetadata.hpp"
#include "app/LaunchOptions.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>

#include <string_view>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QString::fromUtf8(lc::app::AppMetadata::name.data(),
                          static_cast<qsizetype>(lc::app::AppMetadata::name.size())));
    QCoreApplication::setOrganizationName(
        QString::fromUtf8(lc::app::AppMetadata::organization.data(),
                          static_cast<qsizetype>(lc::app::AppMetadata::organization.size())));
    QCoreApplication::setApplicationVersion(
        QString::fromUtf8(lc::app::AppMetadata::version.data(),
                          static_cast<qsizetype>(lc::app::AppMetadata::version.size())));
    application.setQuitOnLastWindowClosed(false);

    if (lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::SmokeTest) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
        return application.exec();
    }

    lc::app::AppController controller(application);
    if (!controller.start()) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("LandscapeCutter"),
                              QStringLiteral("系统托盘不可用，程序无法启动。"));
        return 1;
    }

    return application.exec();
}
```

- [ ] **Step 6: Add the executable target**

Replace `src/CMakeLists.txt` with:

```cmake
add_library(landscapecutter_app_core STATIC
    app/AppController.cpp
    app/LaunchOptions.cpp
)

target_include_directories(landscapecutter_app_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(landscapecutter_app_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    spdlog::spdlog
    WIL::WIL
)

target_compile_definitions(landscapecutter_app_core PUBLIC
    NOMINMAX
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

lc_enable_warnings(landscapecutter_app_core)

qt_add_executable(LandscapeCutter WIN32
    main.cpp
    ../resources/resources.qrc
)

target_link_libraries(LandscapeCutter PRIVATE
    landscapecutter_app_core
)

lc_enable_warnings(LandscapeCutter)
```

- [ ] **Step 7: Build and run automated tests**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: the unit tests and `app_process_smoke` pass. The process smoke test exits within 10 seconds without creating a tray icon.

- [ ] **Step 8: Perform the interactive tray smoke test**

Run:

```powershell
& .\out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe
```

Expected: one LandscapeCutter tray icon appears; a startup notification is shown when Windows notifications are enabled; selecting `退出` removes the tray icon and terminates the process.

- [ ] **Step 9: Commit the running shell**

Run:

```powershell
git add src resources tests/CMakeLists.txt
git commit -m "feat: add Qt tray application shell"
```

Expected: the commit contains the executable, tray controller, resource file, and process smoke test.

---

### Task 5: Remove the active Python runtime and rewrite project documentation

**Files:**
- Delete: `python/Release/landscapecutter_core.cp310-win_amd64.pyd`
- Delete: `python/__pycache__/capture_dxgi.cpython-39.pyc`
- Delete: `python/__pycache__/capture_mss.cpython-39.pyc`
- Delete: `python/__pycache__/config_manager.cpython-39.pyc`
- Delete: `python/__pycache__/floating_window.cpython-39.pyc`
- Delete: `python/__pycache__/region_selector.cpython-39.pyc`
- Delete: `python/__pycache__/window_picker.cpython-39.pyc`
- Delete: `python/capture_mss.py`
- Delete: `python/floating_window.py`
- Delete: `python/main.py`
- Delete: `python/screen_selector.py`
- Delete: `requirements.txt`
- Delete: `config.json`
- Delete: `docs/design.md`
- Delete: `docs/technical_design.md`
- Modify: `README.md`
- Modify: `docs/后台窗口实时捕获问题与「伪最小化」解决方案说明.md`

**Interfaces:**
- Consumes: verified `python-prototype-final` tag and the passing C++ tray shell.
- Produces: a C++-only active tree, accurate build instructions, and a clearly marked historical pseudo-minimize note.

- [ ] **Step 1: Reconfirm recoverability before deletion**

Run:

```powershell
git show --no-patch python-prototype-final
git ls-tree -r --name-only python-prototype-final -- python requirements.txt config.json
git status --porcelain
```

Expected: the tag exposes every file listed for deletion and the current worktree is clean. Stop if either condition is false.

- [ ] **Step 2: Remove the exact tracked Python runtime paths**

Run from PowerShell without globs:

```powershell
git rm -- python/Release/landscapecutter_core.cp310-win_amd64.pyd python/__pycache__/capture_dxgi.cpython-39.pyc python/__pycache__/capture_mss.cpython-39.pyc python/__pycache__/config_manager.cpython-39.pyc python/__pycache__/floating_window.cpython-39.pyc python/__pycache__/region_selector.cpython-39.pyc python/__pycache__/window_picker.cpython-39.pyc python/capture_mss.py python/floating_window.py python/main.py python/screen_selector.py requirements.txt config.json docs/design.md docs/technical_design.md
```

Expected: Git stages only the listed deletions. Recovery remains available through `python-prototype-final`.

- [ ] **Step 3: Replace `README.md` with the current product contract**

Use `apply_patch` to replace the file with:

````markdown
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
- Visual Studio 2022 with the MSVC v143 C++ workload
- Windows SDK 10.0.19041 or newer
- CMake 3.28 or newer
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
````

- [ ] **Step 4: Mark the pseudo-minimize document as historical**

Insert this block immediately below its title:

```markdown

> [!NOTE]
> 本文记录 Python 原型阶段对后台窗口捕获的探索，不是当前 C++ 路线的实现规范。
> 首版 C++ 实现不承诺真实最小化窗口持续更新，也不实现“伪最小化”。当前规范以
> `docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md` 为准。
```

- [ ] **Step 5: Verify that the active tree contains no Python runtime**

Run:

```powershell
git ls-files -- python requirements.txt config.json
git diff --check
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: the first command prints nothing; diff validation is clean; the C++ build and all tests pass.

- [ ] **Step 6: Commit the C++-only active tree**

Run:

```powershell
git add README.md docs/后台窗口实时捕获问题与「伪最小化」解决方案说明.md
git commit -m "refactor: retire Python runtime from main"
```

Expected: the commit includes the staged deletions, current README, and historical-document disclaimer.

---

### Task 6: Verify Milestone 0 from a clean build directory

**Files:**
- Verify: all files created or modified in Tasks 1–5.

**Interfaces:**
- Consumes: completed Milestone 0 commits.
- Produces: evidence that a fresh dependency/build/test/run cycle succeeds and that the prototype remains recoverable.

- [ ] **Step 1: Verify repository and tag state**

Run:

```powershell
git status --short --branch
git tag --list python-prototype-final
git log --oneline --decorate -6
```

Expected: the worktree is clean, the tag exists, and the milestone commits are visible in order.

- [ ] **Step 2: Remove only the verified generated build directory**

Resolve and verify the exact target before deletion:

```powershell
$buildPath = (Resolve-Path -LiteralPath .\out\build\windows-msvc-debug).Path
$repositoryPath = (Resolve-Path -LiteralPath .).Path
if (-not $buildPath.StartsWith((Join-Path $repositoryPath "out\build"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove unexpected build directory: $buildPath"
}
Remove-Item -LiteralPath $buildPath -Recurse -Force
```

Expected: only `out/build/windows-msvc-debug` is removed. `.tools/vcpkg` and source files remain untouched.

- [ ] **Step 3: Reconfigure, rebuild, and test from scratch**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: configuration selects Qt 6.8.2; the full build succeeds; metadata, launch-option, and process-smoke tests pass.

- [ ] **Step 4: Repeat the interactive acceptance check**

Run:

```powershell
& .\out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe
```

Expected: the tray icon and startup notification appear; `退出` closes the process without leaving a tray ghost.

- [ ] **Step 5: Record final verification without creating a no-op commit**

Run:

```powershell
git status --porcelain
git diff --check
```

Expected: both commands print no output. Milestone 0 is complete when all earlier expected results are satisfied; no additional commit is needed.

## Milestone 0 Exit Criteria

- `python-prototype-final` recovers the complete Python prototype.
- The active branch contains no Python runtime, requirements file, stale JSON runtime configuration, tracked `.pyc`, or tracked `.pyd`.
- vcpkg is pinned to tag 2025.02.14 and resolves Qt 6.8.2 dynamically for `x64-windows`.
- `cmake --preset windows-msvc-debug`, build, and CTest all pass from a clean build directory.
- `LandscapeCutter.exe` starts as a system-tray application and exits from its tray menu.
- README, legacy note, and historical pseudo-minimize note match the approved C++ design.
