#include "BasicFunction.h"
#include <StlAPI_Reader.hxx>
#include <Poly_Triangulation.hxx>
#include <AIS_Triangulation.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ModelWithAxis.h>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <QDebug>
#include <Bnd_Box.hxx>
// BasicFunction.cpp
/*
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
*/
// 读取 STL 并直接在给定的 AIS_InteractiveContext 中显示（不加入 PartGraph 数据结构）。
Handle(AIS_Shape) ImportStlToAIS(const std::string& sFileName, 
    const Handle(AIS_InteractiveContext)& context)
{
    //“空壳 + 警告”
    Q_UNUSED(sFileName);
    Q_UNUSED(context);
    qWarning() << "ImportStlToAIS is deprecated. Use LoadStlLightweight instead.";
    return nullptr;
}

// 读取 STL 并构造为 AIS_ModelWithAxis（包含操纵杆支持），返回 handle 并在 context 中显示。
Handle(AIS_ModelWithAxis) ImportStlToAISModel(const std::string& sFileName, 
    const Handle(AIS_InteractiveContext)& context)
{
    //“空壳 + 警告”
    Q_UNUSED(sFileName);
    Q_UNUSED(context);
    qWarning() << "ImportStlToAISModel is deprecated. STL should not be loaded as AIS_ModelWithAxis.";
    return nullptr;
}

// 轻量级 STL 加载：直接解析二进制 STL，构造 Poly_Triangulation → AIS_Triangulation
Handle(AIS_InteractiveObject)
LoadStlLightweight(const std::string& file,
    const Handle(AIS_InteractiveContext)& ctx)
{
    if (file.empty() || ctx.IsNull()) {
        qWarning() << "[LoadStlLightweight] 文件名为空或上下文为空";
        return nullptr;
    }

    // 1) 以二进制方式打开 STL 文件
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        qWarning() << "[LoadStlLightweight] 无法打开 STL 文件:"
            << QString::fromStdString(file);
        return nullptr;
    }

    // 获取文件大小
    in.seekg(0, std::ios::end);
    std::streamoff fileSize = in.tellg();
    in.seekg(0, std::ios::beg);

    if (fileSize < 84) {
        qWarning() << "[LoadStlLightweight] STL 文件太小，可能损坏:"
            << QString::fromStdString(file);
        return nullptr;
    }

    // 2) 读取 80 字节头部 + 4 字节三角形数量
    char header[80];
    in.read(header, 80);

    uint32_t triCount = 0;
    in.read(reinterpret_cast<char*>(&triCount), 4);

    // 检查文件大小是否符合“二进制 STL 格式：84 + 50 * N”
    const std::streamoff expectedSize = 84 + static_cast<std::streamoff>(50) * triCount;
    if (expectedSize != fileSize) {
        qWarning() << "[LoadStlLightweight] 该 STL 可能不是标准二进制格式（或已损坏），"
            "当前实现只支持二进制 STL。";
        return nullptr;
    }

    if (triCount == 0) {
        qWarning() << "[LoadStlLightweight] STL 三角形数量为 0:";
        return nullptr;
    }

    // 3) 创建 Poly_Triangulation
    //    简化起见：每个三角形使用 3 个独立节点（不做顶点合并）
    const Standard_Integer nbNodes = static_cast<Standard_Integer>(triCount) * 3;
    const Standard_Integer nbTriangles = static_cast<Standard_Integer>(triCount);

    Handle(Poly_Triangulation) tri = new Poly_Triangulation(nbNodes, nbTriangles, Standard_False);

    // 4) 逐个三角形读取数据
    for (uint32_t i = 0; i < triCount; ++i)
    {
        float normal[3];
        float verts[9];
        uint16_t attrByteCount = 0;

        // 法线（可以忽略）
        in.read(reinterpret_cast<char*>(normal), sizeof(normal));
        // 顶点
        in.read(reinterpret_cast<char*>(verts), sizeof(verts));
        // 属性字节数
        in.read(reinterpret_cast<char*>(&attrByteCount), sizeof(attrByteCount));

        if (!in) {
            qWarning() << "[LoadStlLightweight] 读取 STL 三角形数据失败，i =" << i;
            return nullptr;
        }

        // 顶点索引（Poly_Triangulation 的节点索引从 1 开始）
        Standard_Integer base = static_cast<Standard_Integer>(i) * 3;

        tri->SetNode(base + 1, gp_Pnt(verts[0], verts[1], verts[2]));
        tri->SetNode(base + 2, gp_Pnt(verts[3], verts[4], verts[5]));
        tri->SetNode(base + 3, gp_Pnt(verts[6], verts[7], verts[8]));

        tri->SetTriangle(static_cast<Standard_Integer>(i) + 1,
            Poly_Triangle(base + 1, base + 2, base + 3));
    }

    qDebug().noquote() << QStringLiteral("[LoadStlLightweight]已从二进制 STL 读取三角形数量")
        << "triCount =" << triCount
        << QStringLiteral(", 节点数 =") << nbNodes;

    // 5) 构造 AIS_Triangulation（真正轻量级显示）
    Handle(AIS_Triangulation) aisTri = new AIS_Triangulation(tri);
    aisTri->SetDisplayMode(AIS_WireFrame);
    aisTri->SetColor(Quantity_NOC_RED);


    // 计算一下三角网的包围盒
    Bnd_Box box;
    for (Standard_Integer i = 1; i <= nbNodes; ++i) {
        box.Add(tri->Node(i));
    }
    Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
    box.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    qDebug().noquote()
        << QStringLiteral("[LoadStlLightweight] BBox:")
        << "X:[" << xMin << "," << xMax << "]"
        << "Y:[" << yMin << "," << yMax << "]"
        << "Z:[" << zMin << "," << zMax << "]";


    ctx->Display(aisTri, Standard_True);

    qDebug().noquote() 
        << QStringLiteral("[LoadStlLightweight] 使用 AIS_Triangulation 轻量显示 STL")
        << QString::fromStdString(file);

    return aisTri;
}



