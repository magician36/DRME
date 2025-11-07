
#include "BasicFunction.h"

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
