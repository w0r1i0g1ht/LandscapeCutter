# SDD ledger — plan: docs/superpowers/plans/2026-09-09-milestone-3-annotation-system.md

Spec: `docs/superpowers/specs/2026-09-09-milestone-3-annotation-system-design.md`
Branch: `codex/milestone-3-annotation-system`
Plan commit: `0580d27`

## Preflight task and interface scan

| Tasks | Producer / consumer or internal check | Finding |
|---|---|---|
| Task 1 | Tests require all six value payloads, document normalization, ID/selection behavior, invalid rejection, history round trips, redo invalidation, unchanged suppression, and the 100-command cap; implementation step names every required mechanism. | Consistent. |
| Task 2 | Tests require deterministic geometry, fixed-image output, clipping, order, base immutability, and preview/final observation; implementation uses one painter path with fixed antialiasing and geometry rules from the spec. | Consistent. |
| Task 3 | Tests require capture/preparation state transitions, locked selection, request/cancel suppression, failure recovery, and the direct Milestone 2 path; implementation supplies the state and preparation boundary. | Consistent. |
| Task 4 | Tests require creation/edit gestures, hit testing, property commands, selection/input routing, unique controls, shortcuts, cross-screen clipping, and draft cancellation; implementation keeps draft values outside committed history and routes through the shared renderer. | Consistent after plan self-review added explicit property-command coverage. |
| Task 5 | Tests require normalized text, deterministic font/layout, shortcut priority, one-command commit, and re-edit behavior; implementation owns one host editor and destroys it on every invalidation path. | Consistent. |
| Task 6 | Tests require exact immutable-base mosaic math, origin alignment, partial blocks, cross-screen identity, visual-layer rendering, and reverse visual-layer hit order; implementation does not cache a reduced image. | Consistent after plan self-review added explicit hit-layer coverage. |
| Task 7 | Tests require exact state recovery, request suppression, RGB32/PNG/clipboard identity, JPEG tolerance, lifecycle cleanup, and Milestone 2 compatibility; implementation snapshots on the GUI thread and dispatches immutable values. | Consistent. |
| Tasks 1 → 2 | Task 1 produces `AnnotationSnapshot` and value types; Task 2 consumes them in `AnnotationRenderer`. | Compatible. Snapshot excludes interaction selection and owns/shares only immutable base plus object values. |
| Tasks 1 → 3 | Task 1 produces `AnnotationDocument` and `AnnotationTool`; Task 3 creates the document after preparation and exposes it from `SnipSession`. | Compatible. Document base is already normalized DPR-1. |
| Tasks 1 → 4 | Task 1 produces document mutations/history; Task 4 commits completed interaction gestures into that API. | Compatible. `replaceObject` supplies before/after command semantics. |
| Tasks 1 → 5/6 | Task 1 defines text and mosaic payloads; Tasks 5 and 6 add their UI, editing, and rendering behavior. | Compatible. Invalid payload checks exist before later tool integration. |
| Tasks 2 → 4 | Task 2 produces shared renderer entry points; Task 4 uses them for committed objects and drafts in overlays. | Compatible. Draft rendering can use a temporary snapshot without entering document history. |
| Tasks 2 → 5/6 | Tasks 5 and 6 extend the renderer created in Task 2 with text and mosaic while preserving the single painter/rendering boundary. | Compatible. Later tasks own only the new payload behavior. |
| Tasks 2 → 7 | Task 2 produces `composeAnnotations`; Task 7 calls it for annotated export. | Compatible. Direct export continues to call existing `composeSelection`. |
| Tasks 3 → 4 | Task 3 produces annotation session states and prepared document; Task 4 routes overlay input based on those states. | Compatible. Selection becomes immutable before interaction begins. |
| Tasks 3 → 7 | Task 3 defines exact source states; Task 7 adds save-path/export transitions and restores the recorded source state on failure or dialog cancellation. | Compatible. |
| Tasks 4 → 5/6 | Task 4 produces interaction and toolbar/overlay routing; Tasks 5 and 6 extend those same files for text and mosaic. | Compatible. Work is sequential and every task commits before the next starts. |
| Tasks 4/5/6 → 7 | Tasks 4–6 own the annotation UI/editor/rendering surfaces; Task 7 integrates output and lifecycle cleanup in the same overlay/session files. | Compatible. Task 7 tests display invalidation and complete cleanup across all earlier surfaces. |

Preflight result: no unresolved contradictions between tasks, the plan's global constraints, or the binding specification.

## Task 1: Annotation Values, Document, and Bounded History

- Base: `0580d27bc048e73749ee53b39ede86e5b893c694`
- Implementer: `/root/m3_task1_impl` (laborer / gpt-5.6-terra medium)
- Brief: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-1-brief.md`
- Implementation commit: `bdb592e feat: add annotation document history`
- GREEN: alternate MSVC Debug Ninja build; 9/9 annotation tests passed. The standard Visual Studio preset is blocked before compilation by duplicate `PATH`/`Path` variables in the host process.
- Review round 1: production implementation compliant; fixes required for direct selection-clearing assertion and per-step redo round-trip assertions. Minor adjacent gaps: explicit nonzero/monotonic ID checks and out-of-bounds clipping cases for arrow, freehand, text, and mosaic.
- Fix commit: `29dd113 test: strengthen annotation history coverage`
- Re-review round 1: all four findings addressed; no new Critical/Important breakage.
- Task 1: complete

## Task 2: Shared Renderer for Rectangle, Ellipse, Arrow, and Freehand

- Base: `29dd113f45cb82bc29448abdc1288b64f0fbf475`
- Implementer: `/root/m3_task2_impl` (laborer / gpt-5.6-terra medium)
- Brief: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-2-brief.md`
- Ruling: use the existing MSVC Debug Ninja build for TDD because the Visual Studio preset is blocked before compilation by the host's duplicate `Path`/`PATH` environment. Cost if wrong: preset-specific integration is deferred until the host environment can be sanitized; compiler, Qt, flags, and dependency set remain MSVC Debug-equivalent.
- Implementation commit: `0829293 feat: render vector annotations`
- GREEN: genuine missing-header RED; focused renderer 4/4 and full annotation 13/13 passed on MSVC Debug Ninja.
- Review round 1: fixes required for transform-scaled document widths, arrow-head topology, arbitrary target clipping, full preview/final pixel observation, fixed-image bounds/z-order/cap coverage, explicit output DPR 1, and compose-input immutability coverage.
- Fix round 1 commit: `d76f513 fix: correct annotation renderer transforms`; focused 6/6 and full annotation 15/15 passed.
- Re-review round 1: all original findings addressed; new Important breakage found because a manually constructed zero-length arrow snapshot can make `drawObject` index a one-point head as three points.
- Fix round 2 commit: `4b4f3a0 fix: guard degenerate annotation arrows`; focused 7/7 and full annotation 16/16 passed.
- Re-review round 2: degenerate arrow snapshot is safely ignored; no new breakage.
- Task 2: complete

## Task 3: Snip Session Annotation State and Cancellable Base Preparation

- Base: `4b4f3a057dfd2ae4526e701832093bf4e94120b4`
- Implementer: `/root/m3_task3_impl` (worker / gpt-5.6-terra medium)
- Brief: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-3-brief.md`
- Implementation commit: `10a13df feat: prepare annotation sessions`
- GREEN: focused genuine missing-header RED; focused 9/9 and annotation/session 24/24 passed.
- Review round 1: persist the exact locked selection, use a dedicated identity per preparation attempt, convert synchronous/worker exceptions to the normal failure completion, and strengthen direct-save, error-signal, same-session retry, and PreparingCapture assertions.
- Fix round 1 commit: `d1b310b fix: harden annotation preparation sessions`.
- GREEN: focused 10/10, annotation 18/18, app-controller/session 26/26 in fresh MSVC Debug Ninja build; no new test process beyond the four pre-existing protected crash artifacts.
- Re-review round 1: all findings addressed; injected chooser removed the headless QFileDialog crash path while preserving production behavior; no new breakage.
- Task 3: complete

## Task 4: Interaction State Machine and First Four Tool UI

- Base: `d1b310b950d003f5e399fac9176a8972bf682c95`
- Brief: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-4-brief.md`
- Ruling: Task 4 may also modify `src/snip/SnipSession.hpp/.cpp` and `tests/snip/SnipSessionTests.cpp`. The plan requires one shared interaction state across all overlays and routing through `SnipSession`, but its file list omitted the owner/wiring files. Cost if wrong: the task diff grows by three tightly scoped files; avoiding them would duplicate draft state per monitor and violate cross-screen interaction.
- Ruling: use a fresh `out/annotation-ninja-task4` build and compare process IDs against the disclosed protected baseline `14104,20732,24440,27444`. Cost if wrong: additional build disk/time; it avoids replacing an executable still locked by prior elevated crash artifacts.
- Implementer: `/root/m3_task4_impl` (worker / gpt-5.6-terra high)
- Implementation commit: `b6eafa5 feat: add vector annotation interaction` (created by controller after the implementer's commit call was blocked by an account-usage approval state).
- GREEN: focused 12/12; annotation 26 cases/4167 assertions; app-controller 32 cases/146 assertions; no new residual test PID beyond the protected baseline.
- Review round 1: fix freehand rigid-move bounds, nearest overlapping resize/endpoint handles, edit-preview ghosting, and deferred-overlay raw pointer lifetime; strengthen all affected interaction/toolbar/cross-screen tests.
- Ruling: annotated Copy/Save composition remains Task 7, whose explicit interface changes `SnipSession::exportImage` and owns `ExportingAnnotated`. Task 4 must verify toolbar buttons emit the correct requests in Annotating but must not duplicate Task 7's state/output implementation. Cost if wrong: Copy/Save remain nonfunctional between intermediate commits, but no milestone build is released from this branch before Task 7.
- Fix round 1 commit: `eb56db8 fix: harden annotation interaction review findings`.
- GREEN: focused 16/16; annotation 29 cases/4193 assertions; app-controller 36 cases/164 assertions; no new residual PID.
- Re-review round 1: all findings addressed; no new breakage.
- Task 4: complete

## Task 5: Text Creation, Editing, and Rendering

- Base: `eb56db8967b8a5db594eb2773c5ddb7a6fd1a025`
- Implementer: `/root/m3_task5_impl` (worker / gpt-5.6-terra high)
- Brief: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-5-brief.md`
- WIP checkpoint: `65f31c5 wip: checkpoint text annotation development`.
- Root cause: text tests instantiate `QApplication` under `QT_QPA_PLATFORM=offscreen`, but the annotation target had not deployed `platforms/qoffscreend.dll`; startup failed before the body and blocked in a VC Runtime dialog. Catch2 discovery did not execute the test body.
- Fixes: deployed `Qt6::QOffscreenIntegrationPlugin`; corrected test fixtures to use actual tab/newline characters; added host-only cross-monitor text-editor routing through `SnipSession`.
- GREEN: focused text/editor/session routing 7/7; full annotation 33 cases/4212 assertions; full app-controller 40 cases/197 assertions in the fresh MSVC Debug Ninja build.
- Process audit: only protected baseline PIDs `14104`, `20732`, `24440`, and `27444` remained.
- Report: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-5-report.md`.
- Review fix round 1: text hit testing now expands the logical bounds by 3 physical pixels; logical-bound pixel comparison proves a missing-text image fails; true Text/Select and non-host double-click sequences clear drafts/grabs and replace an empty host editor.
- Review GREEN: focused 5/5; full annotation 34 cases/4219 assertions; full app-controller 43 cases/226 assertions; only protected baseline PIDs remained.
- Status: complete
- Review round 1: three Important findings required the specified 3px text hit margin, a non-diluted logical-bound pixel tolerance, and real Qt double-click sequence handling.
- Fix commit: `1171129 fix: address text annotation review findings`.
- Re-review round 1: all three findings addressed; no new Critical/Important breakage.
- Task 5: complete (commits `eb56db8..1171129`, review clean)

## Task 3: Snip Session Annotation State and Cancellable Base Preparation

- Base: `4b4f3a057dfd2ae4526e701832093bf4e94120b4`
- Implementer: `/root/m3_task3_impl` (worker / gpt-5.6-terra medium)
- Brief: `.superpowers/sdd/2026-09-09-milestone-3-annotation-system/task-3-brief.md`
- Status: implementation in progress
