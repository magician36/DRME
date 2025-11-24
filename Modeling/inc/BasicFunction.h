#pragma once

#include "OCCInclude.h"
#include <AIS_Shape.hxx>
#include <AIS_InteractiveContext.hxx>
#include <string>
#include <vector>
#include "AIS_ModelWithAxis.h"


TopoDS_Shape ImportStp(std::string sFileName);
void ExtractCylinderAxes(const TopoDS_Shape& shape, const std::string& savePath);
std::vector<gp_Ax1> LoadSavedAxes(const std::string& filePath);

// 读取 STL 并直接在给定的 AIS_InteractiveContext 中显示（不加入 PartGraph 数据结构）。
// 返回创建的 AIS_Shape 句柄，调用者负责后续从 context 中移除或释放。
Handle(AIS_Shape) ImportStlToAIS(const std::string& sFileName, const Handle(AIS_InteractiveContext)& context);

// 读取 STL 并构造为 AIS_ModelWithAxis（包含操纵杆支持），返回 handle 并在 context 中显示。
// 调用者可以随后调用 OCCTWidget::RememberAttachedModel(model) 并使用操纵器。
Handle(AIS_ModelWithAxis) ImportStlToAISModel(const std::string& sFileName, const Handle(AIS_InteractiveContext)& context);
