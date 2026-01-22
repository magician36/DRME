# DRME — 轻量级装配与可视化工具（基于 OpenCASCADE + Qt）

> 简短介绍
>
> DRME 是一个基于 OpenCASCADE 与 Qt 的零件导入、轴线提取与装配管理工具，支持 STEP/STL 导入、轴线提取、装配约束（滑块/棒/螺钉）、可视操纵器（Gizmo）以及装配的序列化/恢复。STL 可以作为“骨头/参考”直接加载并在 3D 视窗操纵，但默认不登记到装配数据结构（`PartGraph`）。

---

## 主要特性

- 导入：支持 `STEP` 和 `STL`（函数 `ImportStp`、`ImportStlToAIS`、`ImportStlToAISModel`）。
- 显示：扩展的 AIS 对象 `AIS_ModelWithAxis`（模型 + 轴线，可从 JSON 恢复轴线）。
- 装配数据结构：`PartGraph`（保存 `PartInfo`、`MateConstraint`、装配组 `Assembly`）。
- 装配算法：`PartAssembler`（几何对齐、螺钉“打入”逻辑）。
- 可操纵器：`OCCTWidget` 内集成 `AIS_Manipulator`，实现滑块/棒/螺钉的 DOF 约束（`ApplyDOFProjectionForActive`）。
- 序列化：`PartGraph::SaveToJson` / `LoadFromJson` 支持保存零件、约束与装配信息。

---

## 目录与核心文件（速览）

- `DRME/src/MainWindow_OSG.cpp` — 主窗口、菜单、特征树与零件库集成。
- `DRME/src/OCCTWidget.cpp` — 3D 视图、交互、操纵器事件和右键菜单逻辑。
- `Modeling/inc/AIS_ModelWithAxis.h` / `Modeling/src/AIS_ModelWithAxis.cpp` — AIS 扩展：模型 + 轴线（提取/绘制/加载）。
- `Modeling/inc/PartGraph.h` / `Modeling/src/PartGraph.cpp` — 装配数据模型与序列化（包括 assemblies）。
- `Modeling/src/PartAssembler.cpp` — 装配对齐与几何处理实现。
- `Modeling/src/BasicFunction.cpp` — 文件导入助手（STEP/STL 的读取与转 AIS）。
- `DRME/src/AssemblyDialog.cpp` — 装配对话框（A/B/C 表、孔选择与高亮）。

（更多文件可见仓库源码目录）

---

## 数据模型（核心概念）

- `PartInfo`：
  - `type`：`PartType`（`Rod` / `Slider` / `Screw` / `Bone`）。
  - `model`：`Handle(AIS_ModelWithAxis)`（用于显示与变换）。
  - `holes`：每孔由 `HoleType` + `gp_Ax1` 表示；对应 `holeRadii`。
  - `dof`、`constraint`、`connectedParts`、`sourcePath` 等元数据。

- `MateConstraint`：描述滑块与棒/螺钉之间的孔轴对应关系，用于约束计算与 DOF 投影。

- `AssemblyInfo`：装配组（成员名列表 + 可选组变换），用于整体移动/序列化。

---

## 典型工作流

1. 启动并创建新文档（`Ui_MainWindow::NewDoc`）。
2. 导入模型：
   - `STEP`：使用 `ImportStp`，导入后会询问零件类型并加入 `PartGraph`。
   - `STL`：使用 `ImportStlToAISModel` 或 `ImportStlToAIS`，作为参考/骨头显示（树上标注为“骨头 (参考)”），默认不登记到 `PartGraph`。
3. 交互：在 3D 视图中可右键弹出菜单（操纵 / 装配 / 更改颜色 / 更改透明度），右键支持对检测到的对象（hover）直接弹菜单。
4. 装配：在 `AssemblyDialog` 中按 A/B/C 表选择并调用 `ConstraintManager` / `PartAssembler` 完成拼接并记录 `MateConstraint`。
5. 操纵器行为：
   - 在拖拽过程中，`OCCTWidget::ApplyDOFProjectionForActive` 将操纵器增量投影到允许的 DOF（例如：棒仅轴向平移，滑块可沿轴平移并绕轴旋转）。
   - 操作结束后会将变换写回 `PartGraph`（`UpdatePartTransform`），并可序列化保存。

---

## 装配组（Assembly）与整体移动 — 设计说明

两种实现策略：

- 方案 A（快速原型，低改动）
  - 在 `PartGraph` 中实现 `MoveAssembly(name, delta, ctx)`：对组成员逐个进行 `Lnew = delta * Lold` 并 `SetLocalTransformation(Lnew)`。
  - UI：通过树或右键创建组后，操纵组内任意零件时，把操纵器的 world 增量传递给 `MoveAssembly` 完成整体移动。
  - 优点：实现快，复用现有操纵器；缺点：缺少独立组代理对象、撤销/序列化支持较弱。

- 方案 B（推荐，结构化实现）
  - `PartGraph` 保存每组 `assembly.transform` 与每成员相对于组的 `memberLocal`，显示与导出时合成 `world = assembly.transform * memberLocal`。
  - 为组创建一个 `AIS` 代理（可见或不可见），操纵器 Attach 到该代理，操纵仅修改 `assembly.transform`。
  - 支持序列化、嵌套、撤销与更好的 UX。

仓库目前已经包含 `AssemblyInfo` 的数据结构，并提供新增接口位置。可根据优先级先实现方案 A 作为演示，再按需升级到方案 B。

---

## 构建与依赖

- OpenCASCADE（示例版本：7.7.0）
- Qt5（示例版本：5.x）
- 支持 C++17 的编译器（MSVC / clang / gcc）
- `nlohmann::json`（单头文件，仓库中已有引用）

在 Windows + Visual Studio 下：打开解决方案并构建；确保 OpenCASCADE 的 include/lib 路径与 Qt 的路径在项目属性中正确配置。

---

## 常见问题与排查建议

- 模型不显示：确认 `OCCTWidget::initializeInteractiveContext` 被执行；`Display(...)` 后应调用 `view->FitAll()` 与 `view->MustBeResized()`。
- 操纵器不可用：确认 `AIS_Manipulator` 已 `Attach(...)` 到对象，且鼠标事件流程中 `StartTransform/Transform/StopTransform` 被正确调用。
- 装配孔未对齐：检查 `PartAssembler` 中轴线转换到世界坐标时是否考虑了 `LocalTransformation()`。
- STL 作为骨头不在 `PartGraph`：这是设计决策。如需登记到 `PartGraph`，可修改 `OCCModeling::LoadModelToWidget` 中的分支逻辑。

---

## 贡献指南

1. Fork 仓库 → 新建分支
2. 实现功能并编写验证步骤
3. 提交 PR，说明改动内容和手工测试方法

---

*此 README 为项目源码概览与上手指南，更多细节请参阅代码注释与各模块头文件。*