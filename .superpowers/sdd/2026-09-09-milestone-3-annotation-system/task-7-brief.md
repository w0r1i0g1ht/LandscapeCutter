### Task 7: Unified Export, Regression, Documentation, and Acceptance

**Base:** `1318a76`

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

**Interfaces and required behavior:**
- Repair deterministic session/overlay cleanup first. `cancel()` must remain safe when invoked from an overlay event, while `SnipSession` destruction and local `QApplication` teardown must not leave parentless overlays queued for deferred deletion.
- `SnipSession::exportImage` captures either the locked annotation snapshot or the existing frozen monitor/selection tuple on the GUI thread before dispatching work.
- Annotated export calls `composeAnnotations`; direct export calls `composeSelection`.
- Save-dialog cancellation restores the exact source state. Direct export failure restores Selecting; annotated failure restores Annotating with document, selection, editor state as applicable, and history intact.
- Stale completions after cancellation, display invalidation, new request, or destruction are ignored.
- Successful copy/save clears document, editor, overlays, images, history, and cancellation state and reaches Idle.
- Clipboard and PNG preserve exact RGB32 pixels. JPEG uses quality 90; decoded size must match and RGB mean absolute error must be at most 12 on the fixture.
- Preserve all Milestone 2 no-annotation behavior.

- [ ] **Step 1: Establish lifecycle RED and repair cleanup**

Reproduce the Task 6 controller regression with the existing cross-monitor case. Add focused cleanup assertions covering explicit cancel, destruction without an event-loop turn, event-originated cancel, and no residual top-level overlays. Implement ownership that is safe for both event callbacks and synchronous destruction. Run the previously failing controller cases repeatedly before continuing.

- [ ] **Step 2: Write failing export state and result tests**

Cover Selecting/Annotating save-dialog cancellation, direct failure returning Selecting, annotated failure returning Annotating with history intact, stale completion rejection, successful cleanup, clipboard/PNG exact equality, and JPEG decoded size plus RGB mean absolute error at most 12.

- [ ] **Step 3: Write failing lifecycle and compatibility tests**

Cover display change during text/preparation/export, rapid F2 suppression, Alt+F4, unique toolbar host, no-annotation Milestone 2 behavior, and session destruction waiting for workers. Extend the real desktop test only with observable state supported by current hardware.

- [ ] **Step 4: Implement the unified output state machine**

Snapshot document values on the GUI thread, render/encode on the worker, and return only result/error plus request ID. Preserve or clear state exactly as specified above.

- [ ] **Step 5: Run focused and full automated verification**

Run the complete annotation and app-controller suites under a bounded offscreen wrapper, default CTest, desktop CTest, `git diff --check`, and a final product/test/helper PID audit. No controller lifecycle crash or timeout may be deferred beyond this task.

- [ ] **Step 6: Update documentation and prepare manual acceptance**

Document six tools, shortcuts, build/test counts, current monitor/DPI/HDR environment, performance observations, and uncovered hardware. Start the product for user acceptance only after automated review is Clean. Do not claim manual acceptance until the user confirms six tools, editing, properties, undo/redo, copy, PNG, and JPEG.

---
