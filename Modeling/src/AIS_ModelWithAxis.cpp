#include "AIS_ModelWithAxis.h"
#include <AIS_InteractiveContext.hxx>  
#include <Graphic3d_ArrayOfPolylines.hxx>
#include <StdPrs_ShadedShape.hxx>
#include <StdPrs_WFShape.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <TopExp_Explorer.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Surface.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <fstream>
#include <iostream>
#include <Bnd_Box.hxx>

IMPLEMENT_STANDARD_RTTIEXT(AIS_ModelWithAxis, AIS_ColoredShape)

AIS_ModelWithAxis::AIS_ModelWithAxis(const TopoDS_Shape& shape, const std::string& axisFile)
    : AIS_ColoredShape(shape), m_axisFile(axisFile)
{
    // 先尝试从文件加载轴线；否则尝试从 shape 提取并保存
    std::ifstream ifs(axisFile);
    if (ifs.good()) {
        ifs.close();
        LoadAxesFromFile(axisFile);
    }
    else {
        ExtractCylinderAxesFromShape(shape);
        SaveAxesToFile();
    }
}

void AIS_ModelWithAxis::LoadAxesFromFile(const std::string& file)
{
    std::ifstream ifs(file);
    if (!ifs.is_open()) return;

    json j; ifs >> j; ifs.close();
    m_axes.clear();
    m_radii.clear();
    m_holeTypes.clear();  // ✅ 新增：清空类型

    if (j.contains("axes"))
    {
        for (auto& a : j["axes"])
        {
            gp_Pnt p(a["px"], a["py"], a["pz"]);
            gp_Dir d(a["dx"], a["dy"], a["dz"]);
            double radius = a.value("radius", 0.0);
            std::string typeStr = a.value("holeType", "RodHole"); // ✅ 新增字段
            HoleType hType = (typeStr == "ScrewHole") ? HoleType::ScrewHole : HoleType::RodHole;

            m_axes.emplace_back(p, d);
            m_radii.push_back(radius);
            m_holeTypes.push_back(hType); // ✅ 保存类型
        }
    }

    // 读完轴线与半径后，尝试从当前几何重建对应的圆柱面
    RebuildCylFacesFromShape(Shape());

    std::cout << "[AIS_ModelWithAxis] 从文件加载轴线数量: "
        << m_axes.size() << std::endl;
}

void AIS_ModelWithAxis::SaveAxesToFile() const
{
    if (m_axisFile.empty()) return;
    json j;
    j["axes"] = json::array();

    for (size_t i = 0; i < m_axes.size(); ++i)
    {
        gp_Pnt p = m_axes[i].Location();
        gp_Dir d = m_axes[i].Direction();
        double r = (i < m_radii.size()) ? m_radii[i] : 0.0;

        // 默认类型为 RodHole（如果还没初始化类型）
        std::string typeStr = "RodHole";
        if (i < m_holeTypes.size())
        {
            typeStr = (m_holeTypes[i] == HoleType::ScrewHole)
                ? "ScrewHole"
                : "RodHole";
        }

        j["axes"].push_back({
            {"px", p.X()}, {"py", p.Y()}, {"pz", p.Z()},
            {"dx", d.X()}, {"dy", d.Y()}, {"dz", d.Z()},
            {"radius", r},
            {"holeType", typeStr}
            });
    }

    std::ofstream ofs(m_axisFile);
    ofs << j.dump(4);
    ofs.close();

    std::cout << "[AIS_ModelWithAxis] 已保存 " << m_axes.size()
        << " 条轴线到文件: " << m_axisFile << std::endl;
}

// // 递归提取所有圆柱面：输出 轴线+半径+对应圆柱面
static void ExtractCylFacesRecursive(const TopoDS_Shape& s,std::vector<std::pair<gp_Ax1, double>>& outAxRad,
    std::vector<TopoDS_Face>& outFaces)
{
    if (s.IsNull()) return;

    if (s.ShapeType() == TopAbs_FACE)
    {
        const TopoDS_Face face = TopoDS::Face(s);
        TopLoc_Location loc;
        Handle(Geom_Surface) surf = BRep_Tool::Surface(face, loc);
        Handle(Geom_CylindricalSurface) cyl = Handle(Geom_CylindricalSurface)::DownCast(surf);

        if (!cyl.IsNull())
        {
            gp_Ax1 ax = cyl->Axis();
            ax.Transform(loc.Transformation());
            double r = cyl->Radius();

            outAxRad.emplace_back(ax, r);
            outFaces.push_back(face);

            std::cout << "[Axis] Cylinder detected: r=" << r
                << " dir=(" << ax.Direction().X() << "," << ax.Direction().Y() << "," << ax.Direction().Z() << ")"
                << std::endl;
        }
    }
    else
    {
        for (TopoDS_Iterator it(s); it.More(); it.Next())
            ExtractCylFacesRecursive(it.Value(), outAxRad, outFaces);
    }
}

//从形状中提取轴线（含半径）
void AIS_ModelWithAxis::ExtractCylinderAxesFromShape(const TopoDS_Shape& shape)
{
    m_axes.clear();
    m_radii.clear();
    m_cylFaces.clear();

    if (shape.IsNull())
        return;

    // --- 1️⃣ 提取所有圆柱面 ---
    std::vector<std::pair<gp_Ax1, double>> candidates;
    std::vector<TopoDS_Face> faces;  //  同步收集 face
    ExtractCylFacesRecursive(shape, candidates, faces);

    // --- 2️⃣ 去除重复轴线 ---
    const double dirTol = 1e-2;
    const double posTol = 1e-1;
    std::vector<bool> used(candidates.size(), false);

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        if (used[i]) continue;

        const gp_Ax1& ax_i = candidates[i].first;
        double r_i = candidates[i].second;
        const TopoDS_Face& f_i = faces[i];

        gp_Dir dir_i = ax_i.Direction();
        gp_Pnt ori_i = ax_i.Location();

        for (size_t j = i + 1; j < candidates.size(); ++j)
        {
            if (used[j]) continue;
            const gp_Ax1& ax_j = candidates[j].first;
            gp_Dir dir_j = ax_j.Direction();
            gp_Pnt ori_j = ax_j.Location();

            if (std::abs(dir_i.Dot(dir_j)) > (1.0 - dirTol) &&
                ori_i.Distance(ori_j) < posTol)
            {
                used[j] = true; // 视作重复
            }
        }

        m_axes.push_back(ax_i);
        m_radii.push_back(r_i);
        m_cylFaces.push_back(f_i);


        used[i] = true;
    }

    // --- 3️⃣ 无圆柱面时回退 ---
    if (m_axes.empty())
    {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        if (!box.IsVoid())
        {
            gp_Pnt pMin = box.CornerMin();
            gp_Pnt pMax = box.CornerMax();

            gp_Pnt center(
                (pMin.X() + pMax.X()) * 0.5,
                (pMin.Y() + pMax.Y()) * 0.5,
                (pMin.Z() + pMax.Z()) * 0.5
            );

            gp_Vec vx(gp_Pnt(pMin.X(), center.Y(), center.Z()), gp_Pnt(pMax.X(), center.Y(), center.Z()));
            gp_Vec vy(gp_Pnt(center.X(), pMin.Y(), center.Z()), gp_Pnt(center.X(), pMax.Y(), center.Z()));
            gp_Vec vz(gp_Pnt(center.X(), center.Y(), pMin.Z()), gp_Pnt(center.X(), center.Y(), pMax.Z()));

            gp_Vec best = vx;
            if (vy.SquareMagnitude() > best.SquareMagnitude()) best = vy;
            if (vz.SquareMagnitude() > best.SquareMagnitude()) best = vz;

            if (best.Magnitude() > 1e-6)
            {
                best.Normalize();
                gp_Ax1 ax(center, gp_Dir(best));
                m_axes.push_back(ax);
                m_radii.push_back(0.0);
                m_cylFaces.push_back(TopoDS_Face()); // 没有真实圆柱面，放空占位
                std::cout << "[Axis] No cylinder found, fallback to bbox axis.\n";
            }
        }
    }

    // --- 4️⃣ 输出调试信息 ---
    std::cout << "[AIS_ModelWithAxis] Found " << m_axes.size()
        << " axes from shape.\n";

    // --- 5️⃣ 保存到 JSON ---
    json root;
    root["axes"] = json::array();
    for (size_t i = 0; i < m_axes.size(); ++i)
    {
        const gp_Ax1& ax = m_axes[i];
        const gp_Pnt& p = ax.Location();
        const gp_Dir& d = ax.Direction();
        double r = (i < m_radii.size()) ? m_radii[i] : 0.0;

        json a;
        a["px"] = p.X();
        a["py"] = p.Y();
        a["pz"] = p.Z();
        a["dx"] = d.X();
        a["dy"] = d.Y();
        a["dz"] = d.Z();
        a["radius"] = r;

        root["axes"].push_back(a);
    }

    std::string savePath = m_axisFile;
    if (!savePath.empty())
    {
        std::ofstream ofs(savePath);
        ofs << root.dump(4);
        ofs.close();
        std::cout << "[AIS_ModelWithAxis] Axes saved to " << savePath << "\n";
    }
}

void AIS_ModelWithAxis::Compute(
    const Handle(PrsMgr_PresentationManager)& thePrsMgr,
    const Handle(Prs3d_Presentation)& thePresentation,
    const Standard_Integer theMode)
{
    //  先让父类绘制模型（只绘制一次） 
    AIS_ColoredShape::Compute(thePrsMgr, thePresentation, theMode);

    //  叠加轴线（如果打开）
    if (!m_axesVisible || m_axes.empty()) return;

    //为轴线创建绘制组
    Handle(Graphic3d_Group) aGroup = thePresentation->NewGroup();

	//定义线条样式
    Handle(Prs3d_LineAspect) aLineAspect =
        new Prs3d_LineAspect(Quantity_NOC_RED, Aspect_TOL_SOLID, 2.0);
    Handle(Prs3d_Drawer) aDrawer = new Prs3d_Drawer();
    aDrawer->SetLineAspect(aLineAspect);
    aGroup->SetGroupPrimitivesAspect(aLineAspect->Aspect());
    
    //手动绘制每条轴线
    for (auto& ax : m_axes)
    {
        gp_Pnt p1 = ax.Location();
        gp_Pnt p2 = p1.Translated(ax.Direction().XYZ() * 100.0);
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(p1, p2);

        //  改用直接绘制几何方式
        TColgp_Array1OfPnt points(1, 2);
        points.SetValue(1, p1);
        points.SetValue(2, p2);

        Handle(Graphic3d_ArrayOfPolylines) aLine = new Graphic3d_ArrayOfPolylines(2);
        aLine->AddVertex(p1);
        aLine->AddVertex(p2);
        aGroup->AddPrimitiveArray(aLine);
    }
}

void AIS_ModelWithAxis::SetAxesVisible(bool visible)
{
    if (m_axesVisible == visible) return;
    m_axesVisible = visible;
    Redisplay(Standard_True);
}

//重建对应的圆柱面
void AIS_ModelWithAxis::RebuildCylFacesFromShape(const TopoDS_Shape& s)
{
    m_cylFaces.clear();
    if (s.IsNull() || m_axes.empty()) return;

    // 先把场景里所有 cylinder face 抽出来
    std::vector<std::pair<gp_Ax1, double>> candAxRad;
    std::vector<TopoDS_Face> candFaces;
    ExtractCylFacesRecursive(s, candAxRad, candFaces);

    if (candAxRad.empty())
    {
        // 没有圆柱面，faces 只能占位
        m_cylFaces.resize(m_axes.size(), TopoDS_Face());
        return;
    }

    // 为每条 m_axes 找最相近的候选 face（简单贪心匹配）
    const double angTol = 1.0 - 1e-2; // 方向余弦接近 1
    const double radTol = 1e-3;       // 半径小差
    m_cylFaces.resize(m_axes.size(), TopoDS_Face());

    std::vector<bool> used(candAxRad.size(), false);

    for (size_t i = 0; i < m_axes.size(); ++i)
    {
        const gp_Ax1& wantAx = m_axes[i];
        const double wantR = (i < m_radii.size() ? m_radii[i] : 0.0);

        int bestIdx = -1;
        double bestScore = 1e100;

        for (size_t j = 0; j < candAxRad.size(); ++j)
        {
            if (used[j]) continue;

            const gp_Ax1& ax = candAxRad[j].first;
            const double  r = candAxRad[j].second;

            // 方向接近（|dot| -> 1）
            double cosang = std::abs(wantAx.Direction().Dot(ax.Direction()));
            if (cosang < angTol) continue;

            // 半径接近（优先）
            double dr = std::abs(wantR - r);

            // 轴线上某点距离差（用原点点距近似）
            double dloc = wantAx.Location().Distance(ax.Location());

            // 简单线性评分（也可以自己调权重）
            double score = dr * 10.0 + dloc;

            if (score < bestScore)
            {
                bestScore = score;
                bestIdx = (int)j;
            }
        }

        if (bestIdx >= 0)
        {
            m_cylFaces[i] = candFaces[bestIdx];
            used[bestIdx] = true;
        }
        else
        {
            // 未匹配上，给空占位，后面高亮时记得判空
            m_cylFaces[i] = TopoDS_Face();
        }
    }
}

// ⭐ 新增：从另一个模型复制轴线、半径和孔类型
void AIS_ModelWithAxis::CloneAxisDataFrom(const Handle(AIS_ModelWithAxis)& other)
{
    if (other.IsNull())
        return;

    // 直接拷贝对方的轴线、半径、孔类型
    m_axes = other->GetAllAxes();
    m_radii = other->GetAllRadii();
    m_holeTypes = other->GetAllHoleTypes();

    // 根据当前 shape 重新匹配圆柱面，保证拾取正确
    RebuildCylFacesFromShape(Shape());
}
