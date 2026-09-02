# 里程碑 0：C++ 工程基础实施计划

> **供智能体执行者使用：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 子技能，逐项实施本计划。所有步骤均使用复选框（`- [ ]`）跟踪进度。

**目标：** 在 Git 中完整保留最终 Python 原型，将当前运行实现替换为可复现的 C++20/Qt 6.8 应用空壳，并确保仓库能在 Windows x64 环境中干净地完成构建和测试。

**架构：** 本里程碑只建立应用空壳和工程基础。使用 CMake 与仓库本地、固定标签的 vcpkg 构建 Qt Widgets 托盘进程；捕获、标注和贴图等功能模块不在本计划范围内，将在后续里程碑计划中分别实现。

**技术栈：** Visual Studio 2022 MSVC v143、C++20、Windows SDK 10.0.19041+、Qt 6.8.2 动态库、CMake 3.28+、vcpkg 2025.02.14、WIL、spdlog、Catch2。

**设计文档：** `docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md`

## 全局约束

- 目标系统为 Windows 10 1903 及以上版本，包括 Windows 11。
- 首个版本只发布并测试 x64 架构。
- 使用符合 LGPL 动态链接要求的 Qt 6.8 系列。
- 使用 MSVC 2022、C++20 语言标准以及 Windows SDK 10.0.19041 或更高版本。
- Windows 专用 API 必须封装在职责明确的模块中；本里程碑不实现捕获功能。
- 日志不得记录截图、窗口内容、剪贴板内容或标注文本。
- 删除 Git 已跟踪的 Python 产物前，必须通过带注释的 Git 标签 `python-prototype-final` 保存 Python 原型。
- 保留产品名 `LandscapeCutter`；只将 Snipaste 描述为灵感来源，不得暗示任何关联关系。
- 后续捕获数据统一使用物理像素，但本工程基础里程碑不引入坐标模型。

## 计划范围

这是七份实施计划中的第一份，只实现里程碑 0。后续可独立测试的计划将依次覆盖 Windows 图形基础、静态截图、标注、静态贴图、实时贴图和产品化。

## 目标文件结构

```text
LandscapeCutter/
├─ CMakeLists.txt                         # 根构建策略和依赖
├─ CMakePresets.json                      # 可复现的 Windows x64 预设
├─ vcpkg.json                             # 清单依赖
├─ .clang-format                          # C++ 格式化约定
├─ .gitignore                             # 构建、vcpkg、IDE 和运行产物
├─ cmake/
│  └─ Warnings.cmake                      # 只作用于本项目的 MSVC 警告策略
├─ scripts/
│  └─ bootstrap.ps1                       # 固定并引导 vcpkg 2025.02.14
├─ src/
│  ├─ CMakeLists.txt                      # 应用目标
│  ├─ main.cpp                            # QApplication 入口
│  └─ app/
│     ├─ AppController.hpp                # 托盘应用所有权边界
│     ├─ AppController.cpp                # 托盘菜单和生命周期行为
│     ├─ AppMetadata.hpp                  # 稳定的产品元数据
│     ├─ LaunchOptions.hpp                # 启动模式公开约定
│     └─ LaunchOptions.cpp                # 命令行解析
├─ tests/
│  ├─ CMakeLists.txt                      # Catch2 和进程冒烟测试
│  └─ app/
│     ├─ AppMetadataTests.cpp             # 产品元数据约定
│     └─ LaunchOptionsTests.cpp           # 启动解析约定
├─ resources/
│  └─ resources.qrc                       # Qt 图标资源
└─ legacy/
   └─ README.md                           # Python 标签和迁移记录
```

---

### Task 1：保留 Python 原型

**文件：**
- 新建：`legacy/README.md`
- 验证：`docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md`

**接口：**
- 输入：包含已批准 C++ 重构设计、工作区干净的当前 Git `HEAD`。
- 输出：带注释的 `python-prototype-final` 标签和永久恢复说明。

- [x] **步骤 1：确认原型可以安全打标签**

运行：

```powershell
git status --porcelain
git tag --list python-prototype-final
```

预期：两条命令均不输出内容。如果工作区存在改动，停止操作并逐一确认所有路径后再继续。如果标签已经存在，运行 `git show --stat python-prototype-final`；只有当它确实指向当前 Python 原型时才允许复用。

- [x] **步骤 2：创建带注释的恢复标签**

运行：

```powershell
git tag -a python-prototype-final -m "Final Python prototype before the C++ rewrite"
```

预期：退出码为 0。

- [x] **步骤 3：验证标签目标和已跟踪的原型文件**

运行：

```powershell
git show --no-patch --decorate python-prototype-final
git ls-tree -r --name-only python-prototype-final -- python requirements.txt config.json
```

预期：标签解析到设计获批时的 `HEAD`；第二条命令列出 Python 源码、依赖文件、配置、已跟踪字节码及旧 `.pyd` 二进制文件。

- [x] **步骤 4：使用 `apply_patch` 创建迁移记录**

使用以下完整内容创建 `legacy/README.md`：

````markdown
# 旧版 Python 原型

最终 Python 实现保存在带注释的 Git 标签 `python-prototype-final` 中。

可使用以下命令查看内容，而不改变当前分支：

```powershell
git show python-prototype-final:README.md
git ls-tree -r --name-only python-prototype-final
```

如需运行或修改原型，请从该标签创建单独的分支或工作树。当前 `main` 分支只包含
C++ 实现，不维持与 Python 文件布局或 API 的兼容性。
````

- [x] **步骤 5：提交恢复说明**

运行：

```powershell
git add legacy/README.md
git commit -m "docs: preserve Python prototype recovery path"
```

预期：生成一个只包含 `legacy/README.md` 的提交；通过 `git tag --list` 仍能看到带注释标签。

---

### Task 2：建立可复现的构建工具链

**文件：**
- 新建：`CMakeLists.txt`
- 新建：`CMakePresets.json`
- 新建：`vcpkg.json`
- 新建：`.clang-format`
- 新建：`cmake/Warnings.cmake`
- 新建：`scripts/bootstrap.ps1`
- 新建：`src/CMakeLists.txt`
- 新建：`tests/CMakeLists.txt`
- 修改：`.gitignore`

**接口：**
- 输入：位于 `D:\IDE\VisualStudio\Community` 的 Visual Studio 2022，以及位于 `C:\Program Files\CMake\bin\cmake.exe` 的 CMake。
- 输出：`windows-msvc-debug` 配置、构建和测试预设，以及仓库本地的 `.tools/vcpkg` 依赖环境。

- [ ] **步骤 1：在 `.gitignore` 中补充 C++ 构建产物**

使用 `apply_patch` 追加以下完整内容：

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

- [ ] **步骤 2：定义清单依赖**

创建 `vcpkg.json`：

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

通过固定 vcpkg 检出标签而不是使用未固定的滚动安装，将 `qtbase` 版本锁定为 6.8.2。

- [ ] **步骤 3：添加 vcpkg 引导脚本**

创建 `scripts/bootstrap.ps1`：

```powershell
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
```

- [ ] **步骤 4：定义 CMake 预设**

创建 `CMakePresets.json`：

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

- [ ] **步骤 5：定义项目警告策略和格式化规则**

创建 `cmake/Warnings.cmake`：

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

创建 `.clang-format`：

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
PointerAlignment: Left
DerivePointerAlignment: false
SortIncludes: CaseSensitive
AllowShortFunctionsOnASingleLine: Empty
```

- [ ] **步骤 6：定义根构建约定**

创建 `CMakeLists.txt`：

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

创建 `src/CMakeLists.txt`：

```cmake
# Application targets are introduced after the toolchain contract is verified.
```

创建 `tests/CMakeLists.txt`：

```cmake
# Test targets are introduced with their corresponding production interfaces.
```

- [ ] **步骤 7：引导并安装依赖**

在仓库根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
```

预期：`.tools/vcpkg/vcpkg.exe` 存在；vcpkg 报告 `qtbase:x64-windows` 版本为 6.8.2，并完成清单中的四项依赖。该命令会下载并构建依赖，因此执行时需要网络访问授权。

- [ ] **步骤 8：配置并构建空工程**

运行：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

预期：配置阶段报告使用 Qt 6.8.2；构建成功退出，且此时尚不编译应用目标。

- [ ] **步骤 9：提交构建基础**

运行：

```powershell
git add .gitignore .clang-format CMakeLists.txt CMakePresets.json vcpkg.json cmake scripts src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "build: establish C++20 Qt toolchain"
```

预期：生成一个构建系统提交；`.tools` 和 `out` 仍未被跟踪且已被忽略。

---

### Task 3：以测试驱动方式建立应用接口约定

**文件：**
- 新建：`src/app/AppMetadata.hpp`
- 新建：`src/app/LaunchOptions.hpp`
- 新建：`src/app/LaunchOptions.cpp`
- 新建：`tests/app/AppMetadataTests.cpp`
- 新建：`tests/app/LaunchOptionsTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`

**接口：**
- 输入：Qt 6 Core、C++20 构建约定和 Catch2 3。
- 输出：`lc::app::AppMetadata`、`lc::app::LaunchMode` 以及 `lc::app::parseLaunchMode(std::span<const std::string_view>)`。

- [ ] **步骤 1：编写应当失败的元数据和启动参数测试**

创建 `tests/app/AppMetadataTests.cpp`：

```cpp
#include "app/AppMetadata.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("application metadata is stable") {
    CHECK(lc::app::AppMetadata::name == "LandscapeCutter");
    CHECK(lc::app::AppMetadata::organization == "LandscapeCutter");
    CHECK(lc::app::AppMetadata::version == "0.1.0-dev");
}
```

创建 `tests/app/LaunchOptionsTests.cpp`：

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

在 `LaunchOptionsTests.cpp` 的头文件引用后立即添加 `using namespace std::string_view_literals;`，确保 `sv` 字面量可以编译。

将 `tests/CMakeLists.txt` 替换为：

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

- [ ] **步骤 2：运行构建并确认测试按预期失败**

运行：

```powershell
cmake --build --preset windows-msvc-debug
```

预期：由于 `landscapecutter_app_core`、`AppMetadata.hpp` 和 `LaunchOptions.hpp` 尚不存在，配置或构建失败。

- [ ] **步骤 3：实现最小接口约定**

创建 `src/app/AppMetadata.hpp`：

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

创建 `src/app/LaunchOptions.hpp`：

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

创建 `src/app/LaunchOptions.cpp`：

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

将 `src/CMakeLists.txt` 替换为：

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

- [ ] **步骤 4：构建并运行定向测试**

运行：

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R "application metadata|launch" 
```

预期：三个 Catch2 测试用例全部通过。

- [ ] **步骤 5：格式化代码并重新运行全部测试**

运行已确认存在的 Visual Studio LLVM 格式化程序：

```powershell
& 'D:\IDE\VisualStudio\Community\VC\Tools\Llvm\x64\bin\clang-format.exe' -i src\app\AppMetadata.hpp src\app\LaunchOptions.hpp src\app\LaunchOptions.cpp tests\app\AppMetadataTests.cpp tests\app\LaunchOptionsTests.cpp
ctest --preset windows-msvc-debug
```

预期：格式化不改变任何行为；所有已发现的测试全部通过。

- [ ] **步骤 6：提交经过测试的接口约定**

运行：

```powershell
git add src/app src/CMakeLists.txt tests/app tests/CMakeLists.txt
git commit -m "test: define application launch contracts"
```

预期：生产接口及对应测试位于同一个提交中。

---

### Task 4：构建 Qt 托盘应用空壳

**文件：**
- 新建：`src/app/AppController.hpp`
- 新建：`src/app/AppController.cpp`
- 新建：`src/main.cpp`
- 新建：`resources/resources.qrc`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`

**接口：**
- 输入：任务 3 生成的 `lc::app::AppMetadata`、`lc::app::LaunchMode` 和 `lc::app::parseLaunchMode`。
- 输出：`lc::app::AppController(QApplication&)`、`bool AppController::start()`、`LandscapeCutter.exe` GUI 目标以及 `app_process_smoke` CTest。

- [ ] **步骤 1：添加应当失败的可执行文件冒烟测试**

向 `tests/CMakeLists.txt` 追加：

```cmake
add_test(NAME app_process_smoke COMMAND LandscapeCutter --smoke-test)
set_tests_properties(app_process_smoke PROPERTIES TIMEOUT 10)
```

- [ ] **步骤 2：运行 CMake 并确认冒烟测试无法解析可执行文件**

运行：

```powershell
cmake --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R app_process_smoke
```

预期：由于 `LandscapeCutter` 可执行目标尚不存在，测试无法运行。

- [ ] **步骤 3：添加 Qt 资源文件**

创建 `resources/resources.qrc`：

```xml
<RCC>
    <qresource prefix="/">
        <file alias="icons/LandscapeCutter.ico">../assets/LandscapeCutter.ico</file>
    </qresource>
</RCC>
```

- [ ] **步骤 4：实现托盘控制器**

创建 `src/app/AppController.hpp`：

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

创建 `src/app/AppController.cpp`：

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

- [ ] **步骤 5：实现应用入口**

创建 `src/main.cpp`：

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

- [ ] **步骤 6：添加可执行目标**

将 `src/CMakeLists.txt` 替换为：

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

- [ ] **步骤 7：构建并运行自动化测试**

运行：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

预期：单元测试和 `app_process_smoke` 均通过。进程冒烟测试在 10 秒内退出，且不会创建托盘图标。

- [ ] **步骤 8：执行交互式托盘冒烟测试**

运行：

```powershell
& .\out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe
```

预期：出现一个 LandscapeCutter 托盘图标；Windows 通知启用时显示启动通知；选择 `退出` 后托盘图标消失且进程终止。

- [ ] **步骤 9：提交可运行的应用空壳**

运行：

```powershell
git add src resources tests/CMakeLists.txt
git commit -m "feat: add Qt tray application shell"
```

预期：提交中包含可执行程序、托盘控制器、资源文件和进程冒烟测试。

---

### Task 5：移除当前 Python 运行实现并重写项目文档

**文件：**
- 删除：`python/Release/landscapecutter_core.cp310-win_amd64.pyd`
- 删除：`python/__pycache__/capture_dxgi.cpython-39.pyc`
- 删除：`python/__pycache__/capture_mss.cpython-39.pyc`
- 删除：`python/__pycache__/config_manager.cpython-39.pyc`
- 删除：`python/__pycache__/floating_window.cpython-39.pyc`
- 删除：`python/__pycache__/region_selector.cpython-39.pyc`
- 删除：`python/__pycache__/window_picker.cpython-39.pyc`
- 删除：`python/capture_mss.py`
- 删除：`python/floating_window.py`
- 删除：`python/main.py`
- 删除：`python/screen_selector.py`
- 删除：`requirements.txt`
- 删除：`config.json`
- 删除：`docs/design.md`
- 删除：`docs/technical_design.md`
- 修改：`README.md`
- 修改：`docs/后台窗口实时捕获问题与「伪最小化」解决方案说明.md`

**接口：**
- 输入：已经验证的 `python-prototype-final` 标签，以及测试通过的 C++ 托盘应用空壳。
- 输出：只保留 C++ 运行实现的当前工作树、准确的构建说明，以及明确标记为历史资料的伪最小化说明。

- [ ] **步骤 1：删除前再次确认可恢复性**

运行：

```powershell
git show --no-patch python-prototype-final
git ls-tree -r --name-only python-prototype-final -- python requirements.txt config.json
git status --porcelain
```

预期：标签中可以找到所有待删除文件，且当前工作区干净。任一条件不满足都必须停止。

- [ ] **步骤 2：删除明确列出的 Git 已跟踪 Python 运行路径**

在 PowerShell 中运行，禁止使用通配符：

```powershell
git rm -- python/Release/landscapecutter_core.cp310-win_amd64.pyd python/__pycache__/capture_dxgi.cpython-39.pyc python/__pycache__/capture_mss.cpython-39.pyc python/__pycache__/config_manager.cpython-39.pyc python/__pycache__/floating_window.cpython-39.pyc python/__pycache__/region_selector.cpython-39.pyc python/__pycache__/window_picker.cpython-39.pyc python/capture_mss.py python/floating_window.py python/main.py python/screen_selector.py requirements.txt config.json docs/design.md docs/technical_design.md
```

预期：Git 只暂存列出的删除操作。所有内容仍可通过 `python-prototype-final` 恢复。

- [ ] **步骤 3：使用当前产品约定替换 `README.md`**

使用 `apply_patch` 将文件替换为以下英文内容。README 面向公开 GitHub 受众，因此保留英文以提高国际用户的可读性和搜索可发现性：

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

- [ ] **步骤 4：将伪最小化文档标记为历史资料**

在标题正下方插入以下内容：

```markdown

> [!NOTE]
> 本文记录 Python 原型阶段对后台窗口捕获的探索，不是当前 C++ 路线的实现规范。
> 首版 C++ 实现不承诺真实最小化窗口持续更新，也不实现“伪最小化”。当前规范以
> `docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md` 为准。
```

- [ ] **步骤 5：确认当前工作树不再包含 Python 运行实现**

运行：

```powershell
git ls-files -- python requirements.txt config.json
git diff --check
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

预期：第一条命令不输出内容；差异检查无问题；C++ 构建及全部测试通过。

- [ ] **步骤 6：提交只保留 C++ 运行实现的当前工作树**

运行：

```powershell
git add README.md docs/后台窗口实时捕获问题与「伪最小化」解决方案说明.md
git commit -m "refactor: retire Python runtime from main"
```

预期：提交中包含已暂存的删除操作、当前 README 以及历史文档免责声明。

---

### Task 6：从干净构建目录验证里程碑 0

**文件：**
- 验证：任务 1 至任务 5 中创建或修改的全部文件。

**接口：**
- 输入：已经完成的里程碑 0 各项提交。
- 输出：证明全新的依赖、构建、测试和运行周期能够成功，且 Python 原型仍可恢复的验证证据。

- [ ] **步骤 1：验证仓库与标签状态**

运行：

```powershell
git status --short --branch
git tag --list python-prototype-final
git log --oneline --decorate -6
```

预期：工作区干净、标签存在，并且各里程碑提交按顺序可见。

- [ ] **步骤 2：只删除已经验证的生成构建目录**

删除前解析并验证准确目标：

```powershell
$buildPath = (Resolve-Path -LiteralPath .\out\build\windows-msvc-debug).Path
$repositoryPath = (Resolve-Path -LiteralPath .).Path
if (-not $buildPath.StartsWith((Join-Path $repositoryPath "out\build"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove unexpected build directory: $buildPath"
}
Remove-Item -LiteralPath $buildPath -Recurse -Force
```

预期：只有 `out/build/windows-msvc-debug` 被删除；`.tools/vcpkg` 和源文件保持不变。

- [ ] **步骤 3：从零重新配置、构建和测试**

运行：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

预期：配置选择 Qt 6.8.2；完整构建成功；元数据、启动参数和进程冒烟测试全部通过。

- [ ] **步骤 4：再次执行交互式验收检查**

运行：

```powershell
& .\out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe
```

预期：托盘图标和启动通知出现；选择 `退出` 后进程关闭，不残留失效托盘图标。

- [ ] **步骤 5：记录最终验证结果，不创建无内容提交**

运行：

```powershell
git status --porcelain
git diff --check
```

预期：两条命令均不输出内容。前述所有预期结果均满足后，里程碑 0 即告完成，无需额外提交。

## 里程碑 0 退出条件

- `python-prototype-final` 可以恢复完整 Python 原型。
- 当前分支不包含 Python 运行实现、依赖文件、过期 JSON 运行配置、已跟踪 `.pyc` 或已跟踪 `.pyd`。
- vcpkg 固定到标签 2025.02.14，并为 `x64-windows` 动态解析 Qt 6.8.2。
- 在干净构建目录中，`cmake --preset windows-msvc-debug`、构建和 CTest 全部通过。
- `LandscapeCutter.exe` 以系统托盘应用启动，并能通过托盘菜单退出。
- README、旧版说明和历史伪最小化说明与已批准的 C++ 设计一致。
