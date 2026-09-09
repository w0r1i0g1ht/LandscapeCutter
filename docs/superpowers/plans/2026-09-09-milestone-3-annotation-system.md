# Milestone 3 Annotation System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add six editable annotation tools with bounded undo/redo and one rendering path shared by preview, clipboard, PNG, and JPEG output.

**Architecture:** A new `annotation` module owns value-semantic objects, document history, QWidget-independent interaction, and deterministic rendering. `snip` locks a selected DPR-1 base image when annotation starts, routes overlay input into the annotation document, and exports immutable snapshots without changing capture or WGC responsibilities.

**Tech Stack:** Existing C++20, Qt 6.8.2 Core/Gui/Widgets, Catch2 3, CMake/MSVC; no new dependency.

**Spec:** `docs/superpowers/specs/2026-09-09-milestone-3-annotation-system-design.md`

## Global Constraints

- Document coordinates are selection-local physical pixels with origin `(0,0)` and a DPR-1 base image.
- Capture, WGC, D3D11 readback, and `SnapshotBatch` must remain annotation-unaware.
- GUI thread owns document mutation, widgets, text editing, and clipboard; workers receive immutable value snapshots.
- Preview and final output must use `AnnotationRenderer`; interaction handles and editors never enter output.
- History holds at most 100 object commands, records one completed gesture as one command, and never copies the base image.
- Cancellation, display changes, and request-ID changes reject stale preparation and export callbacks.
- Preserve the Milestone 2 direct copy/save path until annotation mode is explicitly entered.
- Text, screenshot pixels, and clipboard contents must not be logged or printed by tests.

---

### Task 1: Annotation Values, Document, and Bounded History

**Files:**
- Create: `src/annotation/AnnotationTypes.hpp`
- Create: `src/annotation/AnnotationDocument.hpp`
- Create: `src/annotation/AnnotationDocument.cpp`
- Create: `src/annotation/AnnotationHistory.hpp`
- Create: `src/annotation/AnnotationHistory.cpp`
- Create: `tests/annotation/AnnotationDocumentTests.cpp`
- Create: `tests/annotation/AnnotationHistoryTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produce `AnnotationTool`, `AnnotationId`, six annotation structs, `AnnotationObject`, `AnnotationStyle`, and `AnnotationSnapshot` in namespace `lc::annotation`.
- Produce `AnnotationDocument(QImage base)`, `addObject`, `replaceObject`, `removeObject`, `select`, `clearSelection`, `undo`, `redo`, `canUndo`, `canRedo`, `objects`, and `snapshot`.
- `AnnotationDocument` normalizes base images to `QImage::Format_RGB32`, forces DPR 1.0, assigns monotonic nonzero IDs, clips geometry to `QRectF(QPointF{}, base.size())`, and owns `AnnotationHistory`.
- Default tool properties are red, 3 physical pixels for vector strokes, 24 physical pixels for text, and 12 physical pixels for mosaic blocks.

- [ ] **Step 1: Register the annotation test target and write failing value/document tests**

Add `landscapecutter_annotation_tests` using `Catch2::Catch2WithMain`. Tests must construct a 40×30 DPR-2 source image, create each payload, and assert the document base is owned RGB32/DPR-1, IDs are unique, invalid/empty payloads are rejected, selection clears when its object is removed, and snapshots do not include the active selection.

```cpp
TEST_CASE("annotation document owns a DPR-one base and stable object ids") {
    QImage source(40, 30, QImage::Format_ARGB32);
    source.setDevicePixelRatio(2.0);
    AnnotationDocument document(source);
    const auto first = document.addObject(RectangleAnnotation{{2, 3, 10, 8}, {Qt::red, 3.0}});
    const auto second = document.addObject(EllipseAnnotation{{5, 6, 9, 7}, {Qt::blue, 2.0}});
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first != *second);
    CHECK(document.snapshot().base.devicePixelRatio() == 1.0);
    CHECK(document.snapshot().base.format() == QImage::Format_RGB32);
}
```

- [ ] **Step 2: Write failing history tests**

Cover add/update/delete round trips, selection restoration, redo invalidation after a new command, no command for unchanged values, and a 101-command sequence where only the newest 100 can be undone.

```cpp
TEST_CASE("annotation history drops redo after a new object command") {
    AnnotationDocument document(testImage());
    const auto id = document.addObject(testRectangle()).value();
    REQUIRE(document.undo());
    REQUIRE(document.addObject(testEllipse()).has_value());
    CHECK_FALSE(document.canRedo());
    CHECK(document.objects().size() == 1);
    CHECK(document.objects().front().id != id);
}
```

- [ ] **Step 3: Build and run the tests to establish RED**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target landscapecutter_annotation_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "annotation document|annotation history"
```

Expected: configuration or compilation fails because the annotation interfaces do not exist.

- [ ] **Step 4: Implement the value model and history**

Use `std::variant` for payloads and `std::optional<AnnotationObject>` before/after values for commands. Store the base image only in `AnnotationDocument`; history commands store object values and selected IDs. Reject non-finite coordinates, empty text after tab normalization/whitespace checking, zero-area shapes, zero-length arrows, and freehand paths with fewer than two different points.

```cpp
struct ObjectCommand {
    std::optional<AnnotationObject> before;
    std::optional<AnnotationObject> after;
    std::optional<AnnotationId> selectionBefore;
    std::optional<AnnotationId> selectionAfter;
};
```

- [ ] **Step 5: Run focused tests and commit**

Run the Task 1 CTest filter again; expect all discovered Task 1 cases to pass with no compiler warnings. Commit:

```powershell
git add src/annotation tests/annotation src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add annotation document history"
```

---

### Task 2: Shared Renderer for Rectangle, Ellipse, Arrow, and Freehand

**Files:**
- Create: `src/annotation/AnnotationRenderer.hpp`
- Create: `src/annotation/AnnotationRenderer.cpp`
- Create: `tests/annotation/AnnotationRendererTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consume `AnnotationSnapshot` from Task 1.
- Produce `void drawAnnotations(QPainter&, const AnnotationSnapshot&, const QTransform&, const QRectF&)` and `QImage composeAnnotations(const AnnotationSnapshot&)`.
- Produce `QPolygonF arrowHead(QPointF start, QPointF end, qreal width)` for deterministic geometry tests.

- [ ] **Step 1: Write failing fixed-image and geometry tests**

Use a 64×48 white base. Assert rectangle/ellipse bounds, arrow direction with the 28° and length formula, freehand round joins, clipping, creation order, and that the input base remains unchanged.

```cpp
TEST_CASE("annotation arrow head follows the fixed physical-pixel formula") {
    const auto head = arrowHead({10, 20}, {50, 20}, 3.0);
    REQUIRE(head.size() == 3);
    CHECK(head.front() == QPointF(50, 20));
    CHECK(std::abs(QLineF(head.front(), head[1]).length() - 12.0) < 0.01);
}
```

- [ ] **Step 2: Write a failing preview/export observation test**

Render the same snapshot once into a DPR-1 output and once into a transparent preview layer with a known translation/scale. Map the preview coverage back to document coordinates and assert non-text coverage bounds differ by at most one physical pixel and per-channel non-text differences do not exceed 2.

- [ ] **Step 3: Run the renderer filter to establish RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_annotation_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R "annotation renderer|annotation arrow"
```

Expected: compilation fails because `AnnotationRenderer` is absent.

- [ ] **Step 4: Implement one painter path**

Enable antialiasing and use round caps/joins. Apply the supplied document-to-target transform and clip. `composeAnnotations` copies the base, calls the same object drawing helpers at identity transform, and never draws selection handles.

- [ ] **Step 5: Run annotation tests and commit**

Run all `landscapecutter_annotation_tests` cases; expect green. Commit:

```powershell
git add src/annotation/AnnotationRenderer.* tests/annotation/AnnotationRendererTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: render vector annotations"
```

---

### Task 3: Snip Session Annotation State and Cancellable Base Preparation

**Files:**
- Create: `src/annotation/AnnotationPreparation.hpp`
- Create: `src/annotation/AnnotationPreparation.cpp`
- Create: `tests/annotation/AnnotationPreparationTests.cpp`
- Modify: `src/snip/SnipSession.hpp`
- Modify: `src/snip/SnipSession.cpp`
- Modify: `tests/snip/SnipSessionTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produce `enum class SnipSessionState { Idle, PreparingCapture, Selecting, PreparingAnnotation, Annotating, ChoosingSavePath, ExportingFromSelection, ExportingAnnotated }`.
- Produce `using PrepareAnnotation = std::function<void(std::vector<FrozenMonitor>, QRect, std::shared_ptr<std::atomic_bool>, std::function<void(QImage)>)>` with a default worker implementation.
- Add `beginAnnotation(AnnotationTool)`, `state()`, and `document()` accessors to `SnipSession` for UI and tests.

- [ ] **Step 1: Write failing preparation and state tests**

Use controlled callbacks to assert Selecting only begins after frozen monitors are ready; `beginAnnotation` locks the exact selection, enters PreparingAnnotation, ignores later selection mutation, and accepts only the matching request ID. Cover empty preparation returning to Selecting, cancellation ignoring late completion, and direct copy remaining ExportingFromSelection.

```cpp
TEST_CASE("cancelled annotation preparation cannot resurrect a snip session") {
    ControlledPreparation preparation;
    auto session = readySession(preparation.function());
    session.beginAnnotation(AnnotationTool::Rectangle);
    session.cancel();
    preparation.complete(testImage());
    processEvents();
    CHECK(session.state() == SnipSessionState::Idle);
    CHECK(session.document() == nullptr);
}
```

- [ ] **Step 2: Run the focused tests to establish RED**

Run the annotation preparation and snip session filters; expect missing state/preparation interfaces.

- [ ] **Step 3: Implement state transitions and worker preparation**

Default preparation calls `composeSelection` once in the existing single-thread pool. Store the locked selection separately from mutable `SelectionModel`. On success create `AnnotationDocument`, clear selection drag state, enter Annotating, and notify overlays. On failure restore Selecting and emit the existing non-blocking error signal.

- [ ] **Step 4: Verify Milestone 2 direct output and cancellation**

Run all SnipSession tests plus annotation preparation tests. Explicitly assert direct copy/save without `beginAnnotation` still uses the existing selection composition and cleanup path.

- [ ] **Step 5: Commit**

```powershell
git add src/annotation/AnnotationPreparation.* tests/annotation/AnnotationPreparationTests.cpp src/snip/SnipSession.* tests/snip/SnipSessionTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: prepare annotation sessions"
```

---

### Task 4: Interaction State Machine and First Four Tool UI

**Files:**
- Create: `src/annotation/AnnotationInteraction.hpp`
- Create: `src/annotation/AnnotationInteraction.cpp`
- Create: `tests/annotation/AnnotationInteractionTests.cpp`
- Modify: `src/snip/SnipOverlay.hpp`
- Modify: `src/snip/SnipOverlay.cpp`
- Modify: `tests/snip/SnipOverlayTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produce `press(QPointF)`, `move(QPointF)`, `release(QPointF)`, `cancelDraft`, `deleteSelection`, `setTool`, `setStyle`, and hit-test/edit results in `AnnotationInteraction`.
- Consume `AnnotationDocument` and mutate it only on completed gestures.
- Extend the one toolbar host with Select/Rectangle/Ellipse/Arrow/Brush, color, line width, Undo, Redo, Delete, Copy, Save, Cancel.

- [ ] **Step 1: Write failing interaction tests**

Test reverse drags, document clipping, zero gestures, arrow direction, 1.5-pixel freehand point merging, one command per drag, move/resize/end-point edits, selected-object property changes as one command, topmost hit order, full-box rectangle/ellipse hits, line tolerance `max(6, width/2+3)`, and freehand whole-object movement.

- [ ] **Step 2: Write failing overlay routing tests**

Assert Selecting routes a drag only to `SelectionModel`, Annotating routes it only to `AnnotationInteraction`, toolbar controls exist on one host, Ctrl+Z/Y/Delete update enabled state, Esc cancels a draft before cancelling the session, and cross-screen previews clip without duplicate controls.

- [ ] **Step 3: Run focused filters to establish RED**

Build the annotation and app-controller test targets, then run `-R "annotation interaction|annotation overlay"`; expect missing interaction/UI interfaces.

- [ ] **Step 4: Implement pure interaction before QWidget routing**

Keep draft/edit copies outside committed objects until release. Map desktop physical input to locked-selection local coordinates in `SnipOverlay`, call interaction methods, and refresh every overlay through `SnipSession`. Use `AnnotationRenderer` for committed objects and drafts.

- [ ] **Step 5: Run annotation, overlay, and session tests and commit**

```powershell
git add src/annotation/AnnotationInteraction.* tests/annotation/AnnotationInteractionTests.cpp src/snip/SnipOverlay.* tests/snip/SnipOverlayTests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add vector annotation interaction"
```

---

### Task 5: Text Creation, Editing, and Rendering

**Files:**
- Modify: `src/annotation/AnnotationTypes.hpp`
- Modify: `src/annotation/AnnotationDocument.cpp`
- Modify: `src/annotation/AnnotationInteraction.hpp`
- Modify: `src/annotation/AnnotationInteraction.cpp`
- Modify: `src/annotation/AnnotationRenderer.hpp`
- Modify: `src/annotation/AnnotationRenderer.cpp`
- Modify: `src/snip/SnipOverlay.hpp`
- Modify: `src/snip/SnipOverlay.cpp`
- Create: `tests/annotation/AnnotationTextTests.cpp`
- Modify: `tests/snip/SnipOverlayTests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produce `resolvedAnnotationFont(int physicalPixelSize)`, `textLogicalRect`, text insert/update commands, and a child `QPlainTextEdit` owned by the toolbar-host overlay.

- [ ] **Step 1: Write failing text model/render tests**

Assert tabs become four spaces, whitespace-only text is rejected, newline layout advances by `lineSpacing`, font selection follows Segoe UI/YaHei UI/Arial/GeneralFont, logical bounds stay within one pixel, and fixed output meets the 95% pixels/per-channel-16 tolerance.

- [ ] **Step 2: Write failing text editor shortcut tests**

Assert Enter inserts a newline, Ctrl+Enter commits one history command, Esc cancels the draft, Delete and Ctrl+C edit/copy text, Ctrl+Z/Y and Ctrl+Shift+Z use editor-local history, Ctrl+S is consumed, and double-clicking a text object reopens it without triggering screenshot copy.

- [ ] **Step 3: Establish RED, then implement text end to end**

Run `-R "annotation text|text annotation editor"`; expect failure. Create the editor only on the active toolbar host, place it from the document anchor through the overlay transform, and destroy it on commit, cancel, display invalidation, or session end.

- [ ] **Step 4: Run all annotation/app-controller tests and commit**

```powershell
git add src/annotation src/snip/SnipOverlay.* tests/annotation/AnnotationTextTests.cpp tests/snip/SnipOverlayTests.cpp tests/CMakeLists.txt
git commit -m "feat: add text annotations"
```

---

### Task 6: Deterministic Mosaic Tool

**Files:**
- Modify: `src/annotation/AnnotationRenderer.hpp`
- Modify: `src/annotation/AnnotationRenderer.cpp`
- Modify: `src/annotation/AnnotationInteraction.hpp`
- Modify: `src/annotation/AnnotationInteraction.cpp`
- Modify: `src/snip/SnipOverlay.hpp`
- Modify: `src/snip/SnipOverlay.cpp`
- Create: `tests/annotation/AnnotationMosaicTests.cpp`
- Modify: `tests/snip/SnipOverlayTests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Add Mosaic tool selection and block-size control.
- Render document-origin-aligned blocks from the immutable base using rounded RGB arithmetic means, including clipped edge cells.

- [ ] **Step 1: Write failing pixel-exact mosaic tests**

Use a small coordinate-coded RGB image. Assert block averages, `(sum + count/2)/count` rounding, partial edge cells, clipping, document-origin alignment for a non-origin region, multiple mosaic creation order, vectors rendering above mosaic, and reverse visual-layer hit testing choosing vectors before mosaics.

- [ ] **Step 2: Write failing cross-screen preview test**

Split one document mosaic across two synthetic overlay clips and assert their assembled preview equals the single document render exactly, with no block seam at the monitor boundary.

- [ ] **Step 3: Establish RED and implement mosaic rendering/interaction**

Run `-R "annotation mosaic"`; expect failure. Never cache a low-resolution image in the object or history. Compute cells in document coordinates and sample only the immutable base.

- [ ] **Step 4: Run all annotation and overlay tests and commit**

```powershell
git add src/annotation src/snip/SnipOverlay.* tests/annotation/AnnotationMosaicTests.cpp tests/snip/SnipOverlayTests.cpp tests/CMakeLists.txt
git commit -m "feat: add mosaic annotations"
```

---

### Task 7: Unified Export, Regression, Documentation, and Acceptance

**Files:**
- Modify: `src/snip/SnipSession.hpp`
- Modify: `src/snip/SnipSession.cpp`
- Modify: `src/snip/SnipOverlay.hpp`
- Modify: `src/snip/SnipOverlay.cpp`
- Modify: `src/snip/SnapshotImage.cpp`
- Modify: `tests/snip/SnipSessionTests.cpp`
- Modify: `tests/snip/SnapshotImageTests.cpp`
- Modify: `tests/integration/windows/DesktopCaptureIntegrationTests.cpp`
- Modify: `README.md`
- Create: `docs/superpowers/progress/2026-09-09-milestone-3-progress.md`

**Interfaces:**
- `SnipSession::exportImage` captures either the locked annotation snapshot or the existing frozen monitor/selection tuple before dispatching work.
- Annotated export calls `composeAnnotations`; direct export calls `composeSelection`.
- JPEG writer uses quality 90; PNG and clipboard preserve exact RGB32 pixels.

- [ ] **Step 1: Write failing export state and result tests**

Cover Selecting/Annotating save-dialog cancellation, direct failure returning Selecting, annotated failure returning Annotating with history intact, stale completion rejection, successful cleanup, clipboard/PNG exact equality, and JPEG decoded size plus RGB mean absolute error at most 12.

- [ ] **Step 2: Write failing lifecycle and compatibility tests**

Cover display change during text/preparation/export, rapid F2 suppression, Alt+F4, unique toolbar host, no-annotation Milestone 2 behavior, and session destruction waiting for workers. Extend the real desktop test only with observable state that does not require synthetic claims about unavailable multi-monitor/HDR hardware.

- [ ] **Step 3: Establish RED and implement the unified output state machine**

Snapshot document values on the GUI thread, render/encode on the worker, and return only result/error plus request ID. Restore the exact source state on failure or dialog cancellation; success and cancellation clear document, editor, overlays, images, history, and cancellation state.

- [ ] **Step 4: Run focused and full automated verification**

Run:

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
ctest --preset windows-msvc-debug-desktop --output-on-failure
git diff --check
```

Expected: build exit 0, every default and desktop test passes, and diff check emits no whitespace errors.

- [ ] **Step 5: Update documentation and record actual evidence**

Document the six tools, shortcuts, build/test counts, current monitor/DPI/HDR environment, performance observations, and uncovered hardware matrix. Do not mark manual acceptance complete until the user confirms all six tools, editing, properties, undo/redo, copy, PNG, and JPEG.

- [ ] **Step 6: Run manual acceptance and commit**

Start `out/build/windows-msvc-debug/src/Debug/LandscapeCutter.exe`, complete the checklist, confirm product/test/helper process cleanup, then commit:

```powershell
git add src tests README.md docs/superpowers/progress/2026-09-09-milestone-3-progress.md
git commit -m "feat: complete milestone 3 annotations"
```
