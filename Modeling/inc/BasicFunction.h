#pragma once

#include "OCCInclude.h"
#include <AIS_Shape.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_InteractiveContext.hxx>
#include <string>
#include <vector>
#include "AIS_ModelWithAxis.h"

#include <Bnd_Box.hxx>
#include <gp_Trsf.hxx>
#include <unordered_map>

TopoDS_Shape ImportStp(std::string sFileName);
void ExtractCylinderAxes(const TopoDS_Shape& shape, const std::string& savePath);
std::vector<gp_Ax1> LoadSavedAxes(const std::string& filePath);

// 读取 STL 并直接在给定的 AIS_InteractiveContext 中显示（不加入 PartGraph 数据结构）。
// 返回创建的 AIS_Shape 句柄，调用者负责后续从 context 中移除或释放。
Handle(AIS_Shape) ImportStlToAIS(const std::string& sFileName, const Handle(AIS_InteractiveContext)& context);

// 读取 STL 并构造为 AIS_ModelWithAxis（包含操纵杆支持），返回 handle 并在 context 中显示。
// 调用者可以随后调用 OCCTWidget::RememberAttachedModel(model) 并使用操纵器。
Handle(AIS_ModelWithAxis) ImportStlToAISModel(const std::string& sFileName, const Handle(AIS_InteractiveContext)& context);

// 新增：轻量 STL 加载（不进 PartGraph，只显示）
Handle(AIS_InteractiveObject)LoadStlLightweight(const std::string& file,const Handle(AIS_InteractiveContext)& ctx);

// ===== Scene management for aligning STEP to STL (bones) =====
struct SceneState {
    Handle(AIS_InteractiveContext) Ctx;

    std::vector<Handle(AIS_InteractiveObject)> Bones; // STL models
    std::vector<Handle(AIS_InteractiveObject)> Parts; // STEP models

    bool HasBoneRef = false;          // 是否已经导入过骨骼（即世界坐标是否确定）
    bool HasPartToBoneTrsf = false;   // 是否已经计算过“STEP → 骨骼”的整体平移
    gp_Trsf PartToBoneTrsf;           // 保存零件整体平移变换

    // 缓存非 TopoDS 对象（例如 AIS_Triangulation）的几何中心
    std::unordered_map<size_t, gp_Pnt> ObjectCenters;
    // 缓存非 TopoDS 对象的半对角长度（用于估算分离距离）
    std::unordered_map<size_t, double> ObjectHalfDiagonal;
};

// 计算一组对象的包围盒中心，会自动考虑 LocalTransformation
gp_Pnt ComputeGroupCenter(
    const Handle(AIS_InteractiveContext)& ctx,
    const std::vector<Handle(AIS_InteractiveObject)>& group);

// 计算并应用 “零件组整体平移到骨骼组”
void ComputeAndApplyTranslation(
    const Handle(AIS_InteractiveContext)& ctx,
    std::vector<Handle(AIS_InteractiveObject)>& parts,
    const std::vector<Handle(AIS_InteractiveObject)>& bones,
    gp_Trsf& outTrsf);

// 导入 STL 的逻辑（骨骼永远是世界坐标）
void ImportSTL(SceneState& S, const Handle(AIS_InteractiveObject)& boneObj);

// 导入 STEP 的逻辑
void ImportSTEP(SceneState& S, const Handle(AIS_InteractiveObject)& partObj);

// 全局场景状态（在 BasicFunction.cpp 中定义）
extern SceneState gSceneState;