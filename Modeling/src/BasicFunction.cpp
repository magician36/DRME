#include "BasicFunction.h"
#include <StlAPI_Reader.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ModelWithAxis.h>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <QDebug>

// BasicFunction.cpp
TopoDS_Shape ImportStp(std::string sFileName)
{
    STEPCAFControl_Reader StepCAFReader;
    StepCAFReader.SetColorMode(true);
    StepCAFReader.SetNameMode(true);

    if (StepCAFReader.ReadFile(sFileName.c_str()) != IFSelect_RetDone)
        return TopoDS_Shape(); // 返回空形状

    Handle(XCAFApp_Application) anApp = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) aDoc;
    anApp->NewDocument("MDTV-XCAF", aDoc);
    StepCAFReader.Transfer(aDoc);

    // 提取 root shape
    Handle(XCAFDoc_ShapeTool) aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aDoc->Main());
    TDF_LabelSequence labels;
    aShapeTool->GetFreeShapes(labels);

    if (labels.Length() > 0)
        return aShapeTool->GetShape(labels.Value(1));
    else
        return TopoDS_Shape();
}

// 读取 STL 并直接在给定的 AIS_InteractiveContext 中显示（不加入 PartGraph 数据结构）。
// 返回创建的 AIS_Shape 句柄，调用者负责后续从 context 中移除或释放。
Handle(AIS_Shape) ImportStlToAIS(const std::string& sFileName, const Handle(AIS_InteractiveContext)& context)
{
    if (sFileName.empty() || context.IsNull()) {
        qWarning() << "ImportStlToAIS: empty filename or null context";
        return nullptr;
    }

    TopoDS_Shape shape;
    StlAPI_Reader reader;
    if (!reader.Read(shape, sFileName.c_str())) {
        qWarning() << "ImportStlToAIS: failed to read STL:" << QString::fromStdString(sFileName);
        return nullptr;
    }

    Handle(AIS_Shape) ais = new AIS_Shape(shape);
    context->Display(ais, Standard_False);
    context->Redisplay(ais, Standard_True);

    qDebug() << "ImportStlToAIS: displayed" << QString::fromStdString(sFileName);
    return ais;
}

// 读取 STL 并构造为 AIS_ModelWithAxis（包含操纵杆支持），返回 handle 并在 context 中显示。
Handle(AIS_ModelWithAxis) ImportStlToAISModel(const std::string& sFileName, const Handle(AIS_InteractiveContext)& context)
{
    if (sFileName.empty() || context.IsNull()) {
        qWarning() << "ImportStlToAISModel: empty filename or null context";
        return nullptr;
    }

    TopoDS_Shape shape;
    StlAPI_Reader reader;
    if (!reader.Read(shape, sFileName.c_str())) {
        qWarning() << "ImportStlToAISModel: failed to read STL:" << QString::fromStdString(sFileName);
        return nullptr;
    }

    // 创建 AIS_ModelWithAxis；不传 axisFile（空字符串）表示轴线将从几何中提取
    Handle(AIS_ModelWithAxis) model = new AIS_ModelWithAxis(shape, std::string());
    model->SetAxesVisible(true);

    context->Display(model, Standard_False);
    context->Redisplay(model, Standard_True);

    qDebug() << "ImportStlToAISModel: displayed model" << QString::fromStdString(sFileName);
    return model;
}
