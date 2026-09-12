# Task 7 — Unified Export, Regression, Documentation, and Acceptance

## Delivered behavior

`SnipSession` now selects the export source from its GUI-thread state. Selecting snapshots the frozen monitors and physical selection for `composeSelection`; Annotating snapshots the immutable annotation document for `composeAnnotations`. Rendering, encoding, and atomic image saving run in the session worker pool. Worker completion captures a `QPointer` rather than a raw session pointer and is accepted only while the session and request identity are current.

Save-dialog cancellation and invalid extensions restore the exact source state. Direct failures restore Selecting with the selection intact. Annotated failures restore Annotating with document objects and history intact. Successful copy or save reaches Idle and clears overlays, images, document, interaction, and history. PNG and clipboard paths preserve RGB32 pixels; JPEG explicitly uses quality 90.

An independent review found that cancellation could arrive after the worker's final flag check but before `QSaveFile::commit()`. Each session now owns one export-finalization mutex. `cancel()` holds it while publishing cancellation, and `saveImage()` holds it across the final flag check and atomic commit. This gives cancellation and commit one linear order: cancellation that acquires the boundary first prevents replacement, while an already-finalizing commit completes before cancellation becomes visible. The regression test blocks the save worker at this boundary, publishes cancellation, and verifies that no destination is committed.

The fix is recorded in `b036274 fix: serialize export cancellation and commit`. Independent re-review found no Critical or Important findings: the mutex/token pair is captured per request, request identity still rejects stale completion, destructor cancellation converges with the worker, and the optional low-level mutex parameter has no production call path that omits it.

## Lifecycle repair

Task 6 exposed a latent teardown race. Parentless overlays were released to `deleteLater()`, while local test `QApplication` instances could end before deferred deletes ran. The session now tracks deferred overlays and, during destruction, waits for export/preparation workers before delivering each remaining deferred-delete event. Event-originated cancellation remains deferred so the sender can return safely.

Offscreen overlays also no longer schedule the Windows-only delayed physical placement verification. The earlier timer could interleave with queued annotation preparation and cancel a valid offscreen session. Real Windows platform overlays retain the native placement and post-show verification path. The previously failing cross-monitor live-draft case passed five consecutive direct runs after the repair.

## Automated evidence

- All-target MSVC Debug Ninja build in `out/annotation-ninja-task5-runtime-red`: passed.
- Default CTest excluding `desktop-integration`: 212/212 passed in 99.96 seconds.
- Desktop integration under desktop-session permission: 4/4 passed in 1.28 seconds.
- Focused cancellation/commit regression: a RED check failed without the mutex, then five consecutive runs passed with the mutex restored.
- Hardware evidence: one 1920 × 1080, 96 × 96 DPI, SDR monitor; representative WGC capture completed in 109 ms.
- `git diff --check`: passed.

The export/lifecycle coverage includes direct and annotated failures, Selecting and Annotating save-dialog cancellation, exact annotated clipboard and PNG output, JPEG decoded size and mean RGB error at most 12, successful cleanup, stale completion rejection, destruction during an active worker, display invalidation during preparation/text/export, rapid duplicate session begin suppression, overlay close cancellation, unique cross-monitor toolbar ownership, and existing no-annotation Milestone 2 copy/save behavior.

An initial attempt incorrectly forced `QT_QPA_PLATFORM=offscreen` over the entire CTest run. The 207 non-desktop cases passed, while real WGC/window tests and product process tests failed because their native platform was suppressed. The authoritative runs separated default and desktop tests as the presets do; both passed in their required environments.

## Process and environment audit

Final validation introduced no new `LandscapeCutter`, helper, or `ctest` residue. Two controller-test processes from earlier Task 6/7 diagnostics, PIDs `35352` and `41260`, remain access-protected. Their executable paths could not be read, so they were treated as protected rather than force-terminated or reused. They did not lock the final `runtime-red` build or affect its 212/212 plus 4/4 results.

## Remaining acceptance

Automated implementation is complete. Manual acceptance remains pending for all six tools, geometry/property editing, text editing, undo/redo, clipboard, PNG, and JPEG. The current hardware cannot validate real multi-monitor, negative-coordinate, mixed-DPI, or HDR behavior; those gaps remain explicit and are not replaced by synthetic claims.
