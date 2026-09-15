# 里程碑 4：可编辑静态贴图实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有截图和标注闭环上增加可同时存在、可继续编辑的无边框置顶静态贴图。

**Architecture:** `SnipSession` 只负责准备或移交 `AnnotationDocument`，`PinManager` 负责多个顶层窗口的所有权登记，`PinWindow` 负责单个贴图的查看、编辑和导出。截图 overlay 与贴图窗口共用提取后的 `AnnotationToolbar` 和既有标注文档、交互、渲染及历史模块；静态贴图使用 Qt Widgets 与 `QImage`，不增加新的 GPU 渲染路径。

**Tech Stack:** C++20、Qt 6.8.2 Widgets/Gui/Core、Catch2、CMake 4.4、Visual Studio 2026 MSVC、Windows 10/11。

**Spec:** `docs/superpowers/specs/2026-09-14-milestone-4-static-pins-design.md`

## Global Constraints

- 继续使用 C++20、Qt 6.8.2 和现有固定 vcpkg 工具链，不增加第三方依赖。
- 标注底图和对象几何保持物理像素、`QImage::Format_RGB32`、DPR 1；窗口几何使用 Qt 设备独立坐标。
- 每个 `AnnotationDocument` 在任一时刻只能由 `SnipSession` 或一个 `PinWindow` 独占。
- 创建成功后才结束截图会话；失败必须把原文档及编辑能力完整恢复给截图会话。
- 贴图保留矢量对象和撤销历史，隐藏工具栏不得压平文档。
- 日志不得记录图片、剪贴板、窗口画面或标注文字正文。
- 2026-09-14 当前机器发生过 `VIDEO_MEMORY_MANAGEMENT_INTERNAL (0x0000010E)`；本计划禁止运行 `landscapecutter_graphics_tests`、`landscapecutter_capture_tests`、`landscapecutter_desktop_capture_tests`、两个完整 CTest preset 或产品截图人工复测。
- 自动验证只运行明确命名的 annotation、pin、snip overlay/session 和 app-controller 离屏测试；配置和编译可以进行，但不得顺带执行图形测试。
- 不清理 `D:/Projects/LandscapeCutter/.worktrees/milestone-3-annotation-system`，它继续作为事故与代码取证现场。

---

### Task 1: Extract the Shared Annotation Toolbar

**Files:**
- Create: `src/annotation/AnnotationToolbar.hpp`
- Create: `src/annotation/AnnotationToolbar.cpp`
- Create: `tests/annotation/AnnotationToolbarTests.cpp`
- Modify: `src/snip/SnipOverlay.hpp`
- Modify: `src/snip/SnipOverlay.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/snip/SnipOverlayTests.cpp`

**Interfaces:**
- Consumes: `annotation::AnnotationDocument`, `annotation::AnnotationInteraction`, and existing toolbar object names such as `rectangleToolButton`, `lineWidthSpinBox`, `copyButton`, and `saveButton`.
- Produces: `AnnotationToolbarMode`, `AnnotationToolbar`, host action signals including `pinRequested()` and `doneRequested()`, and an unchanged SnipOverlay user flow.

- [x] **Step 0: Prepare and verify the isolated worktree with safe targets**

The worktree intentionally does not contain ignored dependencies or build output. Reuse the already verified main-checkout toolchain without downloading anything:

```powershell
if (-not (Test-Path -LiteralPath '.tools')) {
    New-Item -ItemType Junction -Path '.tools' -Target 'D:\Projects\LandscapeCutter\.tools'
}
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target landscapecutter_annotation_tests landscapecutter_app_controller_tests
$env:QT_QPA_PLATFORM = 'offscreen'
& .\out\build\windows-msvc-debug\tests\Debug\landscapecutter_annotation_tests.exe
& .\out\build\windows-msvc-debug\tests\Debug\landscapecutter_app_controller_tests.exe
```

Expected: configuration and both named targets succeed; both offscreen executables report zero failed test cases. Stop if either safe baseline fails. Do not run CTest presets or any graphics/capture target.

- [x] **Step 1: Add focused failing toolbar tests**

Create `AnnotationToolbarTests.cpp` with an offscreen `QApplication` fixture. Cover both host modes and keep existing object names stable:

```cpp
TEST_CASE("annotation toolbar exposes snip actions without pin behavior wiring") {
    ApplicationFixture fixture;
    AnnotationDocument document(testImage());
    AnnotationInteraction interaction(document);
    AnnotationToolbar toolbar;
    toolbar.setMode(AnnotationToolbarMode::Snip);
    toolbar.setContentAvailable(true);
    toolbar.setContext(&document, &interaction);

    CHECK(toolbar.findChild<QToolButton*>("copyButton") != nullptr);
    CHECK(toolbar.findChild<QToolButton*>("saveButton") != nullptr);
    CHECK_FALSE(toolbar.findChild<QToolButton*>("pinButton")->isHidden());
    CHECK_FALSE(toolbar.findChild<QToolButton*>("cancelButton")->isHidden());
    CHECK(toolbar.findChild<QToolButton*>("doneButton")->isHidden());
}

TEST_CASE("annotation toolbar pin mode replaces cancel and pin with done") {
    ApplicationFixture fixture;
    AnnotationToolbar toolbar;
    toolbar.setMode(AnnotationToolbarMode::Pin);
    CHECK_FALSE(toolbar.findChild<QToolButton*>("doneButton")->isHidden());
    CHECK(toolbar.findChild<QToolButton*>("pinButton")->isHidden());
    CHECK(toolbar.findChild<QToolButton*>("cancelButton")->isHidden());
}
```

Also cover selected-object style reflection, line-width/color preservation, mosaic block-size visibility, tool checked state, undo/redo/delete enabled state, and `setBusy(true)` disabling mutating and output actions.

- [x] **Step 2: Register the test and verify RED without building unrelated targets**

Add `AnnotationToolbarTests.cpp` to `landscapecutter_annotation_tests`, then run:

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_annotation_tests
```

Expected: compilation fails because `AnnotationToolbar.hpp` and its interfaces do not exist. Do not run CTest yet.

- [x] **Step 3: Implement the shared toolbar**

Create the following public API:

```cpp
namespace lc::annotation {
enum class AnnotationToolbarMode { Snip, Pin };

class AnnotationToolbar final : public QWidget {
    Q_OBJECT
  public:
    explicit AnnotationToolbar(QWidget* parent = nullptr);
    void setMode(AnnotationToolbarMode mode);
    void setContentAvailable(bool available);
    void setContext(AnnotationDocument* document, AnnotationInteraction* interaction);
    void clearContext();
    void setBusy(bool busy);
    void refresh();
  signals:
    void toolRequested(AnnotationTool tool);
    void annotationChanged();
    void undoRequested();
    void redoRequested();
    void deleteRequested();
    void copyRequested();
    void saveRequested();
    void pinRequested();
    void cancelRequested();
    void doneRequested();
};
}
```

Move control construction, selected-object style inspection, value synchronization and button availability from `SnipOverlay` into the new widget. `AnnotationToolbar` may update `AnnotationInteraction` style and block size, but document commands and text-editor commits remain host responsibilities.

Snip mode shows Copy/Save/Pin/Cancel. Pin mode shows Copy/Save/Done. Both modes show the same tool buttons and property controls while an annotation context is present.

- [x] **Step 4: Replace SnipOverlay's inline controls**

Store one `AnnotationToolbar* toolbar_` instead of individual toolbar controls. Connect its signals to the existing SnipOverlay signals. Add:

```cpp
signals:
    void pinRequested();

private:
    void requestPinIfSelected();
```

`requestPinIfSelected()` follows Copy/Save behavior: commit nonempty text first, close empty text without adding an object, then emit exactly once if content exists and the overlay is not busy. Keep toolbar-host placement, cross-screen sharing and all existing button object names unchanged.

- [x] **Step 5: Run only safe toolbar and overlay tests**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_annotation_tests landscapecutter_app_controller_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "annotation toolbar|annotation overlay|text annotation editor|copy and save commit|switching away from text|switching from editing existing text"
```

Expected: every selected test passes. Confirm the test list contains no graphics, capture, or desktop-integration case before accepting the result.

- [x] **Step 6: Commit**

```powershell
git add src/annotation/AnnotationToolbar.* src/snip/SnipOverlay.* tests/annotation/AnnotationToolbarTests.cpp tests/snip/SnipOverlayTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "refactor: share annotation toolbar"
```

---

### Task 2: Add Deterministic Pin Geometry

**Files:**
- Create: `src/pin/PinGeometryModel.hpp`
- Create: `src/pin/PinGeometryModel.cpp`
- Create: `tests/pin/PinGeometryModelTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: a positive document pixel size, Qt top-level window rectangles, wheel deltas, and available screen rectangles.
- Produces: deterministic viewing geometry and opacity rules without QWidget, D3D, WGC, or native handles.

- [x] **Step 1: Write failing scale, opacity, and recovery tests**

Create a new `landscapecutter_pin_tests` Catch2 target using `QT_QPA_PLATFORM=offscreen`, and add:

```cpp
TEST_CASE("pin geometry zoom keeps the cursor anchored") {
    PinGeometryModel model({400, 200}, {100, 100, 400, 200});
    model.zoomAt({300, 200}, 120);
    CHECK(model.windowRect() == QRect(80, 90, 440, 220));
}

TEST_CASE("pin opacity uses five-percent steps and clamps") {
    PinGeometryModel model({40, 20}, {0, 0, 40, 20});
    model.adjustOpacity(-10000);
    CHECK(model.opacity() == 0.10);
    model.adjustOpacity(10000);
    CHECK(model.opacity() == 1.00);
}

TEST_CASE("pin recovery moves only a completely hidden window") {
    PinGeometryModel model({400, 200}, {-900, 20, 400, 200});
    CHECK(model.ensureOperable({QRect{0, 0, 1920, 1040}}));
    CHECK(QRect{0, 0, 1920, 1040}.intersects(model.windowRect()));
}
```

Add cases for negative wheel deltas, the 32-DIP minimum edge, aspect-ratio preservation, zero wheel delta, reset to original size, partially visible windows remaining unchanged, and empty screen input leaving geometry unchanged.

- [x] **Step 2: Build the pin target and verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests
```

Expected: compilation fails because `PinGeometryModel` is absent.

- [x] **Step 3: Implement PinGeometryModel**

Use the fixed zoom rule `factor = pow(1.1, angleDeltaY / 120.0)`. Preserve the anchor's normalized position within the old rectangle while rounding the new size and top-left consistently. Expose:

```cpp
class PinGeometryModel final {
  public:
    PinGeometryModel(QSize documentSize, QRect windowRect);
    void moveTo(QPoint topLeft) noexcept;
    void zoomAt(QPoint desktopAnchor, int angleDeltaY) noexcept;
    void adjustOpacity(int angleDeltaY) noexcept;
    void resetSize() noexcept;
    [[nodiscard]] bool ensureOperable(const QList<QRect>& availableGeometries) noexcept;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] QRect windowRect() const noexcept;
    [[nodiscard]] qreal opacity() const noexcept;
};
```

Reject invalid constructor sizes with an invalid model rather than inventing a document size. Each 120 wheel units changes opacity by 0.05; accumulated partial deltas are not retained.

- [x] **Step 4: Run only PinGeometryModel tests**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "pin geometry"
```

Expected: all `pin geometry` cases pass and no other target executes.

- [x] **Step 5: Commit**

```powershell
git add src/pin/PinGeometryModel.* tests/pin/PinGeometryModelTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add static pin geometry model"
```

---

### Task 3: Build the Editable Pin Window

**Files:**
- Create: `src/pin/PinTypes.hpp`
- Create: `src/pin/PinWindow.hpp`
- Create: `src/pin/PinWindow.cpp`
- Create: `tests/pin/PinWindowTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `PinId`, one owned `AnnotationDocument`, `PinGeometryModel`, shared `AnnotationToolbar`, annotation renderer/interaction, and the existing save/clipboard helpers.
- Produces: one frameless topmost editable pin, normal/editing state transitions, document ownership transfer methods, and host-level copy/save/close signals.

- [x] **Step 1: Write failing viewing-state tests**

```cpp
TEST_CASE("pin window starts frameless topmost and owns its document") {
    ApplicationFixture fixture;
    PinWindow window(1);
    auto document = std::make_unique<AnnotationDocument>(testImage({80, 40}));
    REQUIRE(window.attachDocument(document, {20, 30}).isEmpty());
    CHECK(document == nullptr);
    CHECK(window.document() != nullptr);
    CHECK(window.windowFlags().testFlag(Qt::FramelessWindowHint));
    CHECK(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    CHECK(window.mode() == PinWindowMode::Viewing);
}

TEST_CASE("pin window wheel routes zoom and opacity separately") {
    ApplicationFixture fixture;
    PinWindow window(1);
    auto document = makeDocument({100, 50});
    REQUIRE(window.attachDocument(document, {0, 0}).isEmpty());
    sendWheel(window, {50, 25}, 120, Qt::NoModifier);
    CHECK(window.size() == QSize(110, 55));
    sendWheel(window, {50, 25}, -120, Qt::ControlModifier);
    CHECK(window.windowOpacity() == Approx(0.95));
}
```

Also assert left drag moves only in Viewing mode, double-click enters Editing, the context menu actions exist with exact object names, reset actions update geometry/opacity, and closing emits one `closed(PinId)`.

- [x] **Step 2: Write failing editing and document-history tests**

```cpp
TEST_CASE("pin editing preserves pre-pin objects and history") {
    ApplicationFixture fixture;
    auto document = makeDocument({100, 60});
    REQUIRE(document->addObject(RectangleAnnotation{{5, 5, 20, 10}, {Qt::red, 3}}).has_value());
    PinWindow window(7);
    REQUIRE(window.attachDocument(document, {0, 0}).isEmpty());
    window.enterEditing();
    REQUIRE(window.document()->undo());
    CHECK(window.document()->objects().empty());
}

TEST_CASE("pin done commits text and keeps vectors editable") {
    ApplicationFixture fixture;
    PinWindow window(1);
    auto document = makeDocument({120, 80});
    REQUIRE(window.attachDocument(document, {0, 0}).isEmpty());
    window.enterEditing();
    chooseTool(window, "textToolButton");
    createText(window, {10, 10}, "editable");
    click(window, "doneButton");
    CHECK(window.mode() == PinWindowMode::Viewing);
    REQUIRE(window.document()->objects().size() == 1);
    window.enterEditing();
    CHECK(window.document()->objects().size() == 1);
}
```

Cover rectangle creation, selecting/moving an existing object, Delete, Ctrl+Z/Y, Esc draft-first routing, double-clicking existing text, double-clicking blank space to finish, and no window movement while Editing.

- [x] **Step 3: Verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests
```

Expected: compilation fails because `PinWindow` and `PinTypes` do not exist.

- [x] **Step 4: Implement PinTypes and viewing behavior**

Define:

```cpp
namespace lc::pin {
using PinId = std::uint64_t;
enum class PinWindowMode { Viewing, Editing, ChoosingSavePath, Exporting };
}
```

`PinWindow` begins without a document so `PinManager` can allocate a window before surrendering document ownership. `attachDocument(std::unique_ptr<AnnotationDocument>&, QPoint)` accepts exactly once, validates and initializes geometry before moving the caller's pointer, then creates `AnnotationInteraction` and shows the window. An error leaves the caller's pointer unchanged. Set `WA_DeleteOnClose`, `WA_QuitOnClose` false, strong focus, mouse tracking and the flags from the spec.

Painting uses the document snapshot plus the current interaction draft and `drawAnnotations()` with a document-to-window transform. Viewing hides object selection handles and toolbar. Implement left-drag movement, anchored wheel zoom, Ctrl-wheel opacity, reset actions, context menu and single close notification.

- [x] **Step 5: Implement editing behavior and text lifecycle**

Add:

```cpp
class PinWindow final : public QWidget {
    Q_OBJECT
  public:
    explicit PinWindow(PinId id, QWidget* parent = nullptr);
    QString attachDocument(std::unique_ptr<annotation::AnnotationDocument>&,
                           QPoint preferredTopLeft);
    [[nodiscard]] annotation::AnnotationDocument* document() const noexcept;
    [[nodiscard]] PinWindowMode mode() const noexcept;
    void enterEditing();
    void finishEditing();
  signals:
    void copyRequested(PinId);
    void saveRequested(PinId);
    void closed(PinId);
};
```

Reuse `AnnotationToolbar` in Pin mode. Route mouse coordinates through `documentToWindowTransform().inverted()`. Port the proven text-editor lifecycle from `SnipOverlay`: one editor, physical-pixel font mapping, commit nonempty text on tool/output/done transitions, close empty text without an object, restore edited text on cancel, and block global copy/save shortcuts while the editor has focus.

`finishEditing()` commits nonempty text, cancels any noncommitted gesture, clears document selection, returns to Viewing and hides the toolbar. Esc cancels editor/draft first and only finishes when neither remains.

- [x] **Step 6: Run safe pin-window and annotation regressions**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests landscapecutter_annotation_tests landscapecutter_app_controller_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "pin window|annotation document|annotation history|annotation interaction|annotation renderer|annotation text|annotation toolbar|text annotation editor"
```

Expected: all selected tests pass. Inspect `ctest -N -R` first and reject the command if any selected case belongs to graphics, capture or desktop integration.

- [x] **Step 7: Commit**

```powershell
git add src/pin/PinTypes.hpp src/pin/PinWindow.* tests/pin/PinWindowTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add editable static pin window"
```

---

### Task 4: Add Pin Copy and Save Without Closing

**Files:**
- Modify: `src/pin/PinWindow.hpp`
- Modify: `src/pin/PinWindow.cpp`
- Modify: `tests/pin/PinWindowTests.cpp`
- Test: `tests/snip/SnapshotImageTests.cpp`

**Interfaces:**
- Consumes: immutable `AnnotationSnapshot`, `composeAnnotations`, clipboard access, existing `saveImage`, and injected save-path selection.
- Produces: non-destructive asynchronous copy/save from a pin and safe cancellation on close.

- [x] **Step 1: Write failing output and lifecycle tests**

```cpp
TEST_CASE("pin copy exports current annotations and stays open") {
    ApplicationFixture fixture;
    PinWindow window(1, immediateSaveChooser());
    auto document = annotatedDocument();
    REQUIRE(window.attachDocument(document, {0, 0}).isEmpty());
    clickContextAction(window, "pinCopyAction");
    waitFor([&] { return window.mode() != PinWindowMode::Exporting; });
    CHECK(window.isVisible());
    CHECK(QApplication::clipboard()->image() == composeAnnotations(window.document()->snapshot()));
}

TEST_CASE("pin save cancellation restores the exact prior mode") {
    ApplicationFixture fixture;
    PinWindow window(1, cancellingSaveChooser());
    auto document = annotatedDocument();
    REQUIRE(window.attachDocument(document, {0, 0}).isEmpty());
    window.enterEditing();
    click(window, "saveButton");
    CHECK(window.mode() == PinWindowMode::Editing);
    CHECK(window.document()->canUndo());
}
```

Add PNG exact round-trip, JPEG decoded-size/average-error threshold, save failure retaining document/history, close during export rejecting stale clipboard/file completion, and destruction waiting for the finalization boundary.

- [ ] **Step 2: Verify RED with the pin target only**

> Audit note: the new output tests were added during implementation, but this historical RED build was not recorded separately. It remains unchecked rather than being reconstructed after the fact.

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "pin copy|pin save|pin export"
```

Expected: new cases fail because output handlers are not implemented.

- [x] **Step 3: Implement immutable snapshot output**

Add a save-path injection equivalent to `SnipSession`:

```cpp
using ChoosePinSavePath =
    std::function<void(std::function<void(QString)>, std::function<void()>)>;

explicit PinWindow(PinId id, ChoosePinSavePath chooser = {}, QWidget* parent = nullptr);
```

Also add a single-thread `QThreadPool`, cancellation flag, finalization mutex, monotonically increasing request ID and prior mode. Before output, commit nonempty text and capture `AnnotationSnapshot` on the GUI thread. The worker may call only `composeAnnotations`, `saveImage` or prepare the clipboard image; it must not access widgets or the live document.

Clipboard commit returns to the GUI thread and checks `QPointer<PinWindow>`, cancellation and request ID before calling `QApplication::clipboard()->setImage`. Save selection cancellation and encoding failure restore the precise prior Viewing/Editing state and leave history untouched. Closing invalidates the request and synchronizes with final commit before releasing state.

- [x] **Step 4: Run safe output tests**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests landscapecutter_unit_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "pin copy|pin save|pin export|snapshot image"
```

Expected: selected output tests pass; no capture or graphics case is listed or run.

- [x] **Step 5: Commit**

```powershell
git add src/pin/PinWindow.* tests/pin/PinWindowTests.cpp tests/snip/SnapshotImageTests.cpp
git commit -m "feat: export editable static pins"
```

---

### Task 5: Manage Multiple Pins and Display Changes

**Files:**
- Create: `src/pin/PinManager.hpp`
- Create: `src/pin/PinManager.cpp`
- Create: `tests/pin/PinManagerTests.cpp`
- Modify: `src/pin/PinTypes.hpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: an owned document, preferred top-left point, a window factory for tests, and current available screen geometries.
- Produces: `PinCreateResult`, `PinManager::create`, `closeAll`, `recoverVisibility`, count tracking and exact document return on failure.

- [x] **Step 1: Write failing ownership and multi-window tests**

```cpp
TEST_CASE("pin manager returns the document when window creation fails") {
    ApplicationFixture fixture;
    PinManager manager([](PinId) -> PinWindow* { return nullptr; });
    auto document = makeDocument({40, 20});
    auto* original = document.get();
    auto result = manager.create(std::move(document), {10, 10});
    CHECK_FALSE(result.id.has_value());
    CHECK(result.rejectedDocument.get() == original);
    CHECK_FALSE(result.error.isEmpty());
    CHECK(manager.count() == 0);
}

TEST_CASE("pin manager keeps several windows independent") {
    ApplicationFixture fixture;
    PinManager manager;
    const auto first = manager.create(makeDocument({40, 20}), {0, 0});
    const auto second = manager.create(makeDocument({60, 30}), {100, 0});
    REQUIRE(first.id.has_value());
    REQUIRE(second.id.has_value());
    CHECK(*first.id != *second.id);
    CHECK(manager.count() == 2);
    manager.close(*first.id);
    CHECK(manager.count() == 1);
}
```

Cover invalid/empty documents, monotonic nonzero IDs, closing the same ID twice, `closeAll()` idempotence, countChanged values, user-close removal, display recovery, and manager destruction leaving no top-level pin widgets.

- [x] **Step 2: Verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests
```

Expected: compilation fails because `PinManager` does not exist.

- [x] **Step 3: Implement exact ownership semantics**

Define:

```cpp
struct PinCreateResult {
    std::optional<PinId> id;
    std::unique_ptr<annotation::AnnotationDocument> rejectedDocument;
    QString error;
};

using CreatePinWindow = std::function<PinWindow*(PinId)>;

class PinManager final : public QObject {
    Q_OBJECT
  public:
    explicit PinManager(CreatePinWindow factory = {}, QObject* parent = nullptr);
    PinCreateResult create(std::unique_ptr<annotation::AnnotationDocument>, QPoint preferredTopLeft);
    void close(PinId);
    void closeAll();
    void recoverVisibility(const QList<QRect>& availableGeometries);
    [[nodiscard]] std::size_t count() const noexcept;
  signals:
    void countChanged(std::size_t count);
    void errorOccurred(QString message);
};
```

Validate the document before allocating a window. Allocate an empty `PinWindow` through the factory while the manager still owns the document; only then call `attachDocument(document, preferredTopLeft)`. The attach function moves `document` only after every fallible validation and initialization step succeeds. Failure returns the same unique pointer in `rejectedDocument`. Store `QPointer<PinWindow>` by ID, let the window use `WA_DeleteOnClose`, and remove its entry exactly once from `destroyed`/`closed` handling.

- [x] **Step 4: Run safe manager and window tests**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_pin_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "pin manager|pin window|pin geometry"
```

Expected: all selected pin tests pass.

- [x] **Step 5: Commit**

```powershell
git add src/pin/PinManager.* src/pin/PinTypes.hpp tests/pin/PinManagerTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: manage multiple static pins"
```

---

### Task 6: Transfer Snip Sessions Into Pins

**Files:**
- Modify: `src/snip/SnipSession.hpp`
- Modify: `src/snip/SnipSession.cpp`
- Modify: `src/snip/SnipOverlay.hpp`
- Modify: `src/snip/SnipOverlay.cpp`
- Modify: `tests/snip/SnipSessionTests.cpp`
- Modify: `tests/snip/SnipOverlayTests.cpp`
- Modify: `src/pin/PinTypes.hpp`

**Interfaces:**
- Consumes: SnipOverlay `pinRequested`, existing `PrepareAnnotation`, and a testable `CreatePin` callback.
- Produces: selection-to-pin preparation, annotated-document transfer, exact failure restoration, and stale-request rejection.

- [x] **Step 1: Write failing annotated-transfer tests**

```cpp
TEST_CASE("annotated pin transfers document and history then closes the snip session") {
    SessionFixture fixture;
    CapturedPin captured;
    SnipSession session(fixture.batch, fixture.preparation(), fixture.saveChooser(),
                        captured.callback());
    openAnnotatedSession(session, fixture);
    auto* original = session.document();
    REQUIRE(original->addObject(RectangleAnnotation{{2, 2, 8, 6}, {Qt::red, 2}}).has_value());

    session.pin();

    CHECK(session.state() == SnipSessionState::Idle);
    CHECK(session.overlayCount() == 0);
    CHECK(captured.document.get() == original);
    REQUIRE(captured.document->undo());
    CHECK(captured.document->objects().empty());
}
```

Add failure returning the same document, tool/style/block-size restoration, pending text commit before transfer, empty text omission, active draft cancellation, duplicate request suppression, and callback exceptions converting to a retryable error.

- [x] **Step 2: Write failing direct-selection and stale-completion tests**

```cpp
TEST_CASE("direct selection pin prepares one owned RGB32 document") {
    SessionFixture fixture;
    CapturedPin captured;
    SnipSession session(fixture.batch, fixture.preparation(), fixture.saveChooser(),
                        captured.callback());
    openSelectedSession(session, fixture, {2, 3, 20, 10});
    session.pin();
    CHECK(session.state() == SnipSessionState::PreparingPinFromSelection);
    fixture.completePreparation();
    REQUIRE(captured.document != nullptr);
    CHECK(captured.document->snapshot().base.size() == QSize(20, 10));
    CHECK(captured.document->snapshot().base.devicePixelRatio() == 1.0);
    CHECK(session.state() == SnipSessionState::Idle);
}
```

Cover empty preparation restoring Selecting, cancellation ignoring late completion, display invalidation rejecting completion, repeated F2 suppression, and successful pin creation occurring once.

- [ ] **Step 3: Verify RED using only app-controller tests**

> Audit note: the Task 6 transfer and stale-completion tests were written before the
> implementation, but the complete historical RED output was not retained. This step
> remains unchecked rather than being reconstructed after the fact.

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_app_controller_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "snip session.*pin|overlay.*pin"
```

Expected: compilation or assertions fail because pin session APIs and states are absent.

- [x] **Step 4: Implement the pin state machine**

Add:

```cpp
using CreatePin = std::function<pin::PinCreateResult(
    std::unique_ptr<annotation::AnnotationDocument>&, QPoint preferredTopLeft)>;

enum class SnipSessionState {
    Idle, PreparingCapture, Selecting, PreparingAnnotation, Annotating,
    PreparingPinFromSelection, CreatingPin, ChoosingSavePath,
    ExportingFromSelection, ExportingAnnotated
};

void SnipSession::pin();
```

Add a constructor overload accepting `PrepareAnnotation`, `ChooseSavePath` and `CreatePin`; existing overloads delegate with an empty callback. Connect every overlay's `pinRequested` to `SnipSession::pin`.

The callback receives the document by reference so it consumes ownership only after
successful pin creation. This preserves the exact document when the callback rejects
the request or throws before consuming it.

For Selecting, lock the exact selection, enter `PreparingPinFromSelection` and call the existing preparation boundary with a distinct request ID. For Annotating, snapshot current interaction tool/style/block size, cancel draft, clear selection, destroy the referencing interaction, enter `CreatingPin`, and invoke `CreatePin` on the GUI thread.

On success, destroy overlays and frozen images and return Idle. On failure, recover `rejectedDocument`, rebuild `AnnotationInteraction`, restore tool/style/block size, refresh all overlays, return to the precise source state and emit the returned error. Cancel, display invalidation and destruction invalidate late preparation completions.

- [x] **Step 5: Run safe session, overlay, and preparation tests**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_app_controller_tests landscapecutter_annotation_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "snip session|overlay|annotation preparation|annotation toolbar"
```

Expected: all selected cases pass. Use `ctest -N -R` first to confirm selection excludes every graphics/capture/desktop test.

- [x] **Step 6: Commit**

```powershell
git add src/snip/SnipSession.* src/snip/SnipOverlay.* src/pin/PinTypes.hpp tests/snip/SnipSessionTests.cpp tests/snip/SnipOverlayTests.cpp
git commit -m "feat: create pins from snip sessions"
```

---

### Task 7: Integrate Pins With the Application Lifecycle

**Files:**
- Modify: `src/app/AppController.hpp`
- Modify: `src/app/AppController.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/app/AppControllerTests.cpp`
- Modify: `tests/pin/PinManagerTests.cpp`

**Interfaces:**
- Consumes: `PinManager::create`, `closeAll`, `recoverVisibility`, `countChanged`, SnipSession CreatePin callback, display catalog refresh, and application shutdown.
- Produces: tray “关闭全部贴图”, production pin creation wiring, display recovery and deterministic shutdown order.

- [x] **Step 1: Write failing tray and lifecycle tests**

```cpp
TEST_CASE("app controller enables close-all only while pins exist") {
    ApplicationFixture fixture;
    AppController controller(*QApplication::instance());
    REQUIRE(controller.start());
    auto* action = controller.findChild<QAction*>("closeAllPinsAction");
    REQUIRE(action != nullptr);
    CHECK_FALSE(action->isEnabled());
    controller.setPinCount(2);
    CHECK(action->isEnabled());
    controller.setPinCount(0);
    CHECK_FALSE(action->isEnabled());
}
```

Assert the action emits `closeAllPinsRequested()` once, `PinManager::countChanged` drives it, application shutdown closes pins before capture resources, and a display refresh sends current available geometries to the manager after catalog refresh succeeds.

- [x] **Step 2: Verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_app_controller_tests landscapecutter_pin_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "app controller.*pin|pin manager.*lifecycle|pin manager.*display"
```

Expected: new tests fail because tray and production wiring do not exist.

- [x] **Step 3: Add the tray contract**

Extend `AppController` with:

```cpp
void setPinCount(std::size_t count);
signals:
    void closeAllPinsRequested();
```

Create `closeAllPinsAction_`, object name `closeAllPinsAction`, text “关闭全部贴图”, initially disabled, placed between capture and quit. Clicking emits only the signal; AppController does not own pin windows.

- [x] **Step 4: Wire PinManager in main**

Create `PinManager` before `SnipSession`, inject a lambda that calls `PinManager::create`, and connect errors to `showErrorMessage`. Connect close-all and count signals. After a successful display catalog refresh, build a `QList<QRect>` from `QGuiApplication::screens()` available geometries and call `recoverVisibility`.

Update `aboutToQuit` order:

```cpp
snipSession.cancel();
pinManager.closeAll();
coordinator.shutdown();
hotkey.unregister();
```

- [x] **Step 5: Run safe controller and pin tests**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_app_controller_tests landscapecutter_pin_tests LandscapeCutter
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "app controller|pin manager|pin window|snip session.*pin"
```

Expected: selected tests pass. Do not start `LandscapeCutter.exe` in this task.

- [x] **Step 6: Commit**

```powershell
git add src/app/AppController.* src/main.cpp tests/app/AppControllerTests.cpp tests/pin/PinManagerTests.cpp
git commit -m "feat: integrate static pins with the app"
```

---

### Task 8: Close Milestone 4 With Safe Verification and Documentation

**Files:**
- Create: `docs/superpowers/progress/2026-09-14-milestone-4-progress.md`
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-09-14-milestone-4-static-pins.md`
- Test: all non-GPU targets named below

**Interfaces:**
- Consumes: the completed Tasks 1–7 and their targeted test evidence.
- Produces: an auditable milestone status that distinguishes automatic offscreen validation from deferred real-desktop/GPU validation.

- [ ] **Step 1: Inspect the exact safe test inventory**

```powershell
ctest --test-dir out/build/windows-msvc-debug -C Debug -N -R "annotation|pin|snip session|overlay|app controller|snapshot image"
```

Read every listed test name. If any case comes from `landscapecutter_graphics_tests`, `landscapecutter_capture_tests` or `landscapecutter_desktop_capture_tests`, narrow the regular expression before execution.

- [ ] **Step 2: Build only required targets**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_annotation_tests landscapecutter_pin_tests landscapecutter_app_controller_tests landscapecutter_unit_tests LandscapeCutter
```

Expected: MSBuild exits 0. This is a compile/link check and must not start the product.

- [ ] **Step 3: Run the reviewed safe test set once**

```powershell
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "annotation|pin|snip session|overlay|app controller|snapshot image"
```

Expected: all reviewed cases pass. Do not broaden, repeat, or invoke either preset after success.

- [ ] **Step 4: Review milestone requirements against the spec**

Check each item in spec section 10 against tests and code. Record any deferred real-desktop item without claiming it passed. Inspect:

```powershell
git diff main...HEAD --check
git status --short
git log --oneline main..HEAD
```

Expected: no whitespace errors, no uncommitted implementation, and one ordered commit per completed task.

- [ ] **Step 5: Update public and progress documentation**

Create the progress document with task commits, selected test counts, build evidence, manual-test deferral and the `0x10E` safety boundary. Update README to describe the new Pin button, normal/editing gestures, tray close-all action and manual-validation status. Mark Milestone 4 complete only after every non-deferred exit condition has evidence; otherwise state the exact remaining condition.

- [ ] **Step 6: Commit documentation**

```powershell
git add README.md docs/superpowers/progress/2026-09-14-milestone-4-progress.md docs/superpowers/plans/2026-09-14-milestone-4-static-pins.md
git commit -m "docs: record milestone 4 verification"
```

- [ ] **Step 7: Stop before merge or push**

Report the branch, worktree, commit list, safe build/test evidence, deferred real-desktop validation and remaining risks. Do not merge or push until the user explicitly requests integration after reviewing the result.
