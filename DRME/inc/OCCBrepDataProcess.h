#pragma once
#include "OCCInclude.h"
#include <AIS_InteractiveObject.hxx>
#include <AIS_InteractiveContext.hxx>
#include <V3d_Viewer.hxx>

TopoDS_Shape ImportStl(string sFileName);
TopoDS_Shape ImportStp(string sFileName);
TopoDS_Shape ImportShape(const std::string& sFileName); // 统一导入接口
void OutputColorShape(string sFileName, TopoDS_Shape pShape);

// 显示与材质灯光相关的统一封装接口
void SetupViewerDisplay(const Handle(V3d_Viewer)& viewer, const Handle(AIS_InteractiveContext)& ctx);
void ApplyDisplayAttributes(const Handle(AIS_InteractiveContext)& ctx, const Handle(AIS_InteractiveObject)& obj, Standard_Boolean immediate = Standard_False);