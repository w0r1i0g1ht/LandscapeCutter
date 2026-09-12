# Task 6 — Deterministic Mosaic Tool

## Delivered behavior

Task 6 adds a Mosaic tool to the shared annotation interaction and toolbar. The toolbar host owns a positive integer block-size `QSpinBox` with range 1–128 and default 12; host changes read its value from the shared interaction. Creation, move, resize, selection, and selected block-size replacement follow the existing one-command history behavior.

Rendering calculates document-origin-aligned cells directly from the immutable base image. Each cell samples and fills only its intersection with the mosaic rectangle and base image, uses RGB means rounded as `(sum + count / 2) / count`, and uses 64-bit accumulation. Invalid hand-built nonpositive block sizes are ignored safely. Mosaics draw as a visual layer below vectors/text, and hit testing follows the reverse visual-layer order.

## TDD and focused validation

The initial RED build deliberately referenced the missing interaction block-size API and toolbar controls. It failed while building `landscapecutter_annotation_tests` / `landscapecutter_app_controller_tests`, including `SnipOverlayTests.cpp(266)` where `mosaicBlockSize` did not exist.

After implementation:

```powershell
ctest --test-dir out\annotation-ninja-task6 -R "annotation mosaic" --output-on-failure
```

passed 9/9 tests. Coverage includes exact RGB means and half-up rounding, clipped and partial cells, document-origin alignment, immutable-base multi-mosaic composition, visual render/hit layers, invalid manual values, direct and two-overlay split previews, and toolbar/block-size behavior.

The complete annotation executable was also run with `QT_QPA_PLATFORM=offscreen`, `SetErrorMode(0x0003)`, redirected standard streams, and a bounded `Start-Process` wrapper. It exited zero with `All tests passed (4258 assertions in 41 test cases)` and no stderr.

## Controller-suite observation

Direct execution with inherited output previously left processes live despite the same annotation body passing under the controlled wrapper. The controlled wrapper avoids that annotation-run hang.

The full `landscapecutter_app_controller_tests.exe` process did not exit within 60 seconds in the controlled wrapper and was terminated. CTest then ran all 45 discovered controller cases in isolated processes: 29 passed and 16 failed or timed out in existing session/overlay cases. The new `annotation mosaic toolbar selects the tool and exposes its bounded block control` case passes independently (7 assertions), including a final isolated CTest run.

The focused comparison case, `annotating across monitors keeps one toolbar host while a draft is live`, failed 0/3 in the Task 6 build (one `0xc0000374` crash and timeouts) but passed 3/3 in `out\\annotation-ninja-task5` (4 assertions each). The cause is the existing test/session lifetime path: `session.cancel()` queues parentless overlays for `deleteLater()`, then the local `QApplication` exits. Task 6's changed binary layout makes that deferred-delete UB reproducible; temporary reversions of the Task 6 renderer, overlay UI, and added overlay test did not remove it. Task 7 must repair the cleanup path before a milestone-wide controller-suite acceptance run.

## Process and diff audit

The old protected PIDs (`14104`, `20732`, `24440`, `27444`) had already disappeared. I explicitly terminated only Task-6-launched test processes during diagnosis, including the full controller PID 39424. Final audit found no `landscapecutter_*` or `ctest*` processes. `git diff --check` completed without whitespace errors.

## Self-review

The renderer has no cached reduced image and samples only `snapshot.base`; geometry is aligned in document space before preview clipping. Tests cover the main visual and history contracts. The outstanding risk is the existing controller test/session cleanup lifetime behavior described above. Focused mosaic, its controller entry, and complete annotation validation are green.
