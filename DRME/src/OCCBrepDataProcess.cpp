#pragma once
#include "OCCBrepDataProcess.h"
#include <StlAPI_Reader.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <Prs3d_Drawer.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <V3d_Viewer.hxx>
#include <AIS_InteractiveContext.hxx>

// ================== 导入相关 ==================
static TopoDS_Shape _ReadStl(const std::string& sFileName)
{
    TopoDS_Shape aShape;
    if (!std::filesystem::exists(sFileName)) {
        std::cerr << "[ImportStl] file not exist: " << sFileName << std::endl;
        return aShape; // Null
    }
    StlAPI_Reader reader;
    if (reader.Read(aShape, sFileName.c_str()) != IFSelect_RetDone || aShape.IsNull()) {
        std::cerr << "[ImportStl] read failed: " << sFileName << std::endl;
        aShape.Nullify();
    }
    return aShape;
}

TopoDS_Shape ImportStl(string sFileName)
{
    return _ReadStl(sFileName);
}

TopoDS_Shape ImportStp(string sFileName)
{
    TopoDS_Shape singleShape;            // 若只有一个自由形状
    TopoDS_Compound compoundMaker;       // 若多个则合并
    BRep_Builder brepBuilder; brepBuilder.MakeCompound(compoundMaker);

    STEPCAFControl_Reader stepReader;    // 读 STEP (含颜色/名称)
    stepReader.SetColorMode(true);
    stepReader.SetNameMode(true);
    if (stepReader.ReadFile(sFileName.c_str()) != IFSelect_RetDone) {
        return singleShape; // 空
    }

    Handle(XCAFApp_Application) anApp = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc; anApp->NewDocument("MDTV-XCAF", doc);
    if (!stepReader.Transfer(doc)) {
        return singleShape; // 传输失败
    }

    TDF_Label mainLabel = doc->Main();
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(mainLabel);
    TDF_LabelSequence freeShapes; shapeTool->GetFreeShapes(freeShapes);

    const Standard_Integer n = freeShapes.Size();
    if (n == 0) {
        return singleShape; // 没有形状
    }
    if (n == 1) {
        singleShape = shapeTool->GetShape(freeShapes.Value(1));
        return singleShape;
    }

    // 多形状 -> 合并
    for (Standard_Integer i = 1; i <= n; ++i) {
        const TDF_Label lbl = freeShapes.Value(i);
        TopoDS_Shape shp = shapeTool->GetShape(lbl);
        if (!shp.IsNull()) {
            brepBuilder.Add(compoundMaker, shp);
        }
    }
    return compoundMaker;
}

TopoDS_Shape ImportShape(const std::string& sFileName)
{
    std::string ext; auto pos = sFileName.find_last_of('.');
    if (pos != std::string::npos) { ext = sFileName.substr(pos + 1); std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); }); }
    if (ext == "stl") return _ReadStl(sFileName);
    return ImportStp(sFileName); // 默认 STEP
}

void OutputColorShape(string sFileName, TopoDS_Shape pShape)
{
    if (pShape.IsNull() || sFileName.empty()) return;
    STEPCAFControl_Writer writer;
    Handle(XCAFApp_Application) anApp = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc; anApp->NewDocument("MDTV-XCAF", doc);
    TDF_Label mainLabel = doc->Main();
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(mainLabel);
    TDF_Label partLabel = shapeTool->NewShape(); shapeTool->SetShape(partLabel, pShape);
    writer.SetColorMode(true);
    writer.Perform(doc, sFileName.c_str()); // 忽略返回
}

// ================== 显示与材质灯光 ==================
void SetupViewerDisplay(const Handle(V3d_Viewer)& viewer, const Handle(AIS_InteractiveContext)& ctx)
{
    if (!viewer.IsNull()) {
        viewer->SetDefaultLights();
        viewer->SetLightOn();
    }
    if (!ctx.IsNull()) {
        ctx->SetDisplayMode(AIS_Shaded, Standard_True); // 全局着色模式
        Handle(Prs3d_Drawer) drw = ctx->DefaultDrawer();
        if (!drw.IsNull()) {
            drw->SetFaceBoundaryDraw(Standard_False); // 关闭面边界
        }
    }
}

void ApplyDisplayAttributes(const Handle(AIS_InteractiveContext)& ctx, const Handle(AIS_InteractiveObject)& obj, Standard_Boolean immediate)
{
    if (ctx.IsNull() || obj.IsNull()) return;
    ctx->SetDisplayMode(obj, AIS_Shaded, Standard_False); // 着色显示
    obj->SetMaterial(Graphic3d_NOM_PLASTIC);              // 设置材质
    Handle(Prs3d_Drawer) drw = obj->Attributes();
    if (!drw.IsNull()) {
        drw->SetFaceBoundaryDraw(Standard_False);         // 关闭面边界
    }
    ctx->Redisplay(obj, immediate);
}
