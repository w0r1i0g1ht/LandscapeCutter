# 里程碑 4 执行进度

- 设计：[里程碑 4：可编辑静态贴图详细设计](../specs/2026-09-14-milestone-4-static-pins-design.md)
- 计划：[里程碑 4：可编辑静态贴图实施计划](../plans/2026-09-14-milestone-4-static-pins.md)
- 执行分支：`codex/milestone-4-static-pins`
- 状态：功能实现和限定的安全自动验证已完成；真实桌面与 GPU 相关人工验收暂缓。

## 已实现

- 截图工具栏可把普通选区或带标注的文档转为无边框置顶贴图；只有创建成功才关闭截图会话。
- 多个贴图由 `PinManager` 独立管理，支持移动、滚轮锚点缩放、透明度、单独关闭和托盘关闭全部。
- 双击贴图可重新进入编辑，沿用矩形、椭圆、箭头、画笔、文字、马赛克、编辑、删除及撤销重做。
- 贴图持有未压平的 `AnnotationDocument` 和原撤销历史；退出编辑只隐藏工具栏，不丢失矢量对象。
- 复制与 PNG/JPEG 保存从 GUI 线程取得不可变快照，再由单线程池合成或编码；输出不关闭贴图。
- 创建失败恢复同一个文档、选区、历史、工具和样式；取消、显示失效和过期完成不会创建贴图。
- 显示配置刷新成功后，只把完全不可见的贴图移回可用屏幕；应用退出按截图、贴图、捕获、热键顺序清理。

## 提交记录

- `fa932eb`：批准可编辑静态贴图设计。
- `99cc675`：建立任务化实施计划与安全验证边界。
- `30d0acb`：提取截图与贴图共用的标注工具栏。
- `42f0abc`：增加贴图缩放、透明度和屏幕恢复几何模型。
- `e427875`：增加可重新编辑的静态贴图窗口。
- `bf38975`：增加贴图复制与 PNG/JPEG 保存。
- `c969c87`：增加多贴图管理和显示器变化恢复。
- `8f057cf`：打通截图会话到贴图的文档所有权转移。
- `9218619`：接入托盘动作、保存路径、显示刷新与退出生命周期。

## 自动验证（2026-09-15）

测试前使用 `ctest -N -R` 审查了 125 个命中用例，确认它们只来自 annotation、pin、
app-controller 和普通单元测试目标，不包含 graphics、capture 或 desktop-capture 可执行目标。

以下目标由 MSVC Debug 构建成功，MSBuild 退出码为 0；`LandscapeCutter.exe` 只编译和链接，未启动：

- `landscapecutter_annotation_tests`
- `landscapecutter_pin_tests`
- `landscapecutter_app_controller_tests`
- `landscapecutter_unit_tests`
- `LandscapeCutter`

审查后的离屏集合一次运行 `125/125` 通过，总耗时约 7.23 秒。Task 7 的完整目标回归另有：
app-controller `72/72`（452 个断言），pin `32/32`（175 个断言）。最终差异执行
`git diff main...HEAD --check` 未发现空白错误。

## 退出条件核对

1. 普通选区与标注文档转贴图、成功关闭截图会话：由 selection/annotated pin 会话测试覆盖。
2. 多贴图、移动、缩放、透明度和独立关闭：由 PinManager、PinWindow 和 PinGeometryModel 测试覆盖。
3. 六类工具、对象编辑、删除和撤销重做：由共享工具栏、PinWindow 交互与标注回归覆盖。
4. 历史和矢量对象在贴图前后保留：由 annotated history、editing history 和文字再编辑测试覆盖。
5. 预览、剪贴板、PNG 与 JPEG 输出：共用 `composeAnnotations`，并有复制及双格式保存测试覆盖。
6. 创建失败、取消、显示失效和过期回调：由恢复、重复请求及迟到完成测试覆盖。
7. 单个关闭、关闭全部、析构和导出取消：由窗口关闭、管理器析构和迟到剪贴板测试覆盖。
8. 本里程碑限定的纯逻辑与 Qt offscreen 集合：`125/125` 通过。
9. 真实桌面验收的暂缓原因和待测范围记录如下。

## 暂缓的真实桌面验收

2026-09-14，当前开发机在先前合并后的默认 CTest 运行期间发生 Windows
`VIDEO_MEMORY_MANAGEMENT_INTERNAL (0x0000010E)` 蓝屏重启。现有证据不能确定根因，也不能证明
某个 LandscapeCutter 测试直接导致该故障。在独立转储分析明确风险前，本里程碑没有启动产品，
没有运行完整 CTest preset，也没有运行 `landscapecutter_graphics_tests`、
`landscapecutter_capture_tests` 或 `landscapecutter_desktop_capture_tests`。

风险解除后需单独人工检查：多个贴图共存和置顶、跨屏拖动、100%/150%/200% 混合 DPI、滚轮
锚点缩放、透明度、编辑工具栏、复制和保存、显示器移除恢复以及退出无残留。该人工验证状态不写作
已通过，也不与捕获压力测试混跑。
