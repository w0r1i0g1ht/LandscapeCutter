# Milestone 2 Static Snip Implementation Plan

> **For agentic workers:** Use executing-plans for integration, with bounded delegated selection-model work. Track steps below.

**Goal:** F2 freezes all monitors and supports an adjustable physical-pixel selection, clipboard and PNG/JPEG export.
**Architecture:** One immutable snapshot per monitor, one shared selection model, one overlay per monitor. A session controller serializes capture and asynchronous readback/export, rejects stale callbacks and owns overlay lifetime.
**Tech Stack:** Existing C++20, Qt 6.8.2 Widgets, WGC/D3D11, Catch2; no new dependency.
**Spec:** ../specs/2026-09-08-milestone-2-static-snip-design.md (approved 2026-09-08).

## Global constraints

- Physical coordinates are half-open, permit negative origins, and must not be confused with Qt logical screen geometry.
- Capture all monitors before displaying overlays. Cancel on topology/DPI changes; retry the whole batch at most once after device loss.
- GUI thread owns windows/clipboard; worker threads own readback, composition, encoding and atomic writes.
- SDR output; floating-point scRGB uses bounded sRGB conversion with highlights clipped, explicitly documented.
- Retain current pinned toolchain. Do not claim real multi-monitor/HDR acceptance from synthetic tests.

## Task 1: Selection model

Files: src/snip/SelectionModel.hpp/.cpp, tests/snip/SelectionModelTests.cpp.
Interface: QRect physical bounds/selection (Qt width/height semantics), QPoint physical input;
setBounds(QRect), press(QPoint,int handleRadius), move(QPoint), release(), clear(), rect(), hitTest(QPoint,int).
Hit enum: None, Create, Move, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight.

- [ ] Add tests first: reverse drag from (10,10) to (-10,-5) gives QRect(-10,-5,20,15), moves preserve size at limits, every handle resizes, zero drag invalid.
- [ ] Build tests; establish missing-feature RED.
- [ ] Implement normalized half-open geometry, bounded movement, minimum one-pixel resize and handle hit testing.
- [ ] Build/run selection tests; review geometry independently of UI.

## Task 2: Frozen image and export

Files: src/snip/SnapshotImage.hpp/.cpp, tests/snip/SnapshotImageTests.cpp; CMake registration.
Interfaces: FrozenMonitor { QRect geometry; QImage image; }; imageFromReadback(TextureReadback) -> QImage;
composeSelection(vector<FrozenMonitor>,QRect) -> QImage; saveImage(QImage,QString,QByteArray) -> QString (empty success).

- [ ] Tests first: padded BGRA rows, negative-origin two-screen pixel colors, black gap, gap-only rejection, malformed input, half-float 0/1/NaN, PNG/JPEG round trip and failed write preserving an existing file.
- [ ] Build for RED; implement checked sizes, copy-owned output, scRGB linear-to-sRGB clamp, QSaveFile/QImageWriter encoding.
- [ ] Run focused tests and ensure image DPR is 1.0; deploy JPEG plugin for product and image tests.

## Task 3: Snapshot batch lifecycle

Files: src/snip/SnapshotBatch.hpp/.cpp, tests/snip/SnapshotBatchTests.cpp.
Interfaces: constructor takes IMonitorCaptureService, ID3d11DeviceRecovery, injectable asynchronous readback;
start(vector<MonitorDescriptor>), cancel(); ready(vector<FrozenMonitor>), failed(CaptureNoticeCode).

- [ ] Tests first with controlled completions: all-monitor barrier, one failure discards batch, busy start ignored, cancellation ignores late callbacks, stale frame rejected, one device recovery restarts from screen zero.
- [ ] Implement sequential capture and readback with request identity, QPointer lifetime and per-batch retry bound.
- [ ] Run focused tests; real readback adapter uses worker pool and catches all native/allocation exceptions.

## Task 4: Overlay and application session

Files: src/snip/SnipOverlay.hpp/.cpp, src/snip/SnipSession.hpp/.cpp, tests/snip/SnipSessionTests.cpp;
src/main.cpp, src/app/AppController.cpp, src/CMakeLists.txt, tests/CMakeLists.txt.

- [ ] Tests first: empty/cancelled session creates no windows; synthetic two-screen session creates two overlays only after ready; close destroys all; output shortcuts share selection data.
- [ ] Implement physical monitor placement via HWND, native cursor input, shared selection, frozen background/mask/handles/dimensions, copy/save/cancel toolbar and shortcuts.
- [ ] Worker composition and atomic save; cancelled file dialog restores selection, failed save preserves session, successful copy/save ends it.
- [ ] Connect F2/tray through session; keep existing availability/conflict policies, invalidate immediately on native display change, join workers before device destruction.
- [ ] Build/run focused UI and existing process tests.

## Task 5: Acceptance

Files: tests/integration/windows/DesktopCaptureIntegrationTests.cpp, README.md,
docs/superpowers/progress/2026-09-08-milestone-2-progress.md.

- [ ] Build `cmake --build --preset windows-msvc-debug`, fix any regressions.
- [ ] Run `ctest --preset windows-msvc-debug --output-on-failure` and desktop preset.
- [ ] Independent review of lifecycle and coordinate boundaries; fix actionable findings and rerun affected checks.
- [ ] Record actual environment and uncovered hardware matrix, user manual steps, all test counts; `git diff --check`.
- [ ] Deliver verified code and instructions; do not mark manual acceptance complete without evidence.
