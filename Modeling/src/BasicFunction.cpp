#include "BasicFunction.h"
#include <StlAPI_Reader.hxx>
#include <Poly_Triangulation.hxx>
#include <AIS_Triangulation.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ModelWithAxis.h>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <QDebug>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <TopoDS_Shape.hxx>
#include <cmath>

// Global scene state
SceneState gSceneState;

// 计算一组对象的包围盒中心，会自动考虑 LocalTransformation
gp_Pnt ComputeGroupCenter(
    const Handle(AIS_InteractiveContext)& ctx,
    const std::vector<Handle(AIS_InteractiveObject)>& group)
{
    Bnd_Box totalBox; totalBox.SetGap(0.0);
    bool has = false;

    for (const auto& obj : group) {
        if (obj.IsNull()) continue;

        // 尝试获取底层 shape（如果是 AIS_Shape 或 AIS_ModelWithAxis）
        TopoDS_Shape shape;
        Handle(AIS_Shape) s = Handle(AIS_Shape)::DownCast(obj);
        if (!s.IsNull()) {
            shape = s->Shape();
        }
        else {
            Handle(AIS_ModelWithAxis) m = Handle(AIS_ModelWithAxis)::DownCast(obj);
            if (!m.IsNull()) {
                shape = m->Shape();
            }
        }

        if (shape.IsNull()) {
            // If no TopoDS shape, try to find precomputed center (e.g., AIS_Triangulation)
            Handle(AIS_Triangulation) tri = Handle(AIS_Triangulation)::DownCast(obj);
            if (!tri.IsNull()) {
                size_t key = reinterpret_cast<size_t>(tri.get());
                auto it = gSceneState.ObjectCenters.find(key);
                if (it != gSceneState.ObjectCenters.end()) {
                    gp_Pnt c = it->second;
                    // apply local transformation if any
                    gp_Trsf L = obj->LocalTransformation();
                    c.Transform(L);
                    Bnd_Box bb; bb.SetGap(0.0); bb.Add(c);
                    totalBox.Add(bb);
                    has = true;
                }
            }
            continue;
        }

        // 计算 shape 的包围盒
        Bnd_Box box; box.SetGap(0.0);
        BRepBndLib::Add(shape, box);
        if (box.IsVoid()) continue;

        // transform the 8 corners by LocalTransformation (works fine even if identity)
        gp_Trsf L = obj->LocalTransformation();
        Standard_Real xmin,ymin,zmin,xmax,ymax,zmax; box.Get(xmin,ymin,zmin,xmax,ymax,zmax);
        gp_Pnt corners[8] = {
            gp_Pnt(xmin,ymin,zmin), gp_Pnt(xmin,ymin,zmax), gp_Pnt(xmin,ymax,zmin), gp_Pnt(xmin,ymax,zmax),
            gp_Pnt(xmax,ymin,zmin), gp_Pnt(xmax,ymin,zmax), gp_Pnt(xmax,ymax,zmin), gp_Pnt(xmax,ymax,zmax)
        };
        Bnd_Box tb; tb.SetGap(0.0);
        for (int i=0;i<8;++i){ gp_Pnt p = corners[i]; p.Transform(L); tb.Add(p); }
        totalBox.Add(tb);
        has = true;
    }

    if (!has) return gp_Pnt(0,0,0);

    Standard_Real xmin,ymin,zmin,xmax,ymax,zmax; totalBox.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    gp_Pnt center((xmin+xmax)*0.5, (ymin+ymax)*0.5, (zmin+zmax)*0.5);
    return center;
}

// Helper: compute group's half-diagonal (approximate radius) by accumulating transformed bbox
static double ComputeGroupHalfDiagonal(const std::vector<Handle(AIS_InteractiveObject)>& group)
{
    Bnd_Box totalBox; totalBox.SetGap(0.0);
    bool has = false;

    for (const auto& obj : group) {
        if (obj.IsNull()) continue;

        TopoDS_Shape shape;
        Handle(AIS_Shape) s = Handle(AIS_Shape)::DownCast(obj);
        if (!s.IsNull()) shape = s->Shape();
        else {
            Handle(AIS_ModelWithAxis) m = Handle(AIS_ModelWithAxis)::DownCast(obj);
            if (!m.IsNull()) shape = m->Shape();
        }

        if (!shape.IsNull()) {
            Bnd_Box box; box.SetGap(0.0);
            BRepBndLib::Add(shape, box);
            if (box.IsVoid()) continue;
            gp_Trsf L = obj->LocalTransformation();
            Standard_Real xmin,ymin,zmin,xmax,ymax,zmax; box.Get(xmin,ymin,zmin,xmax,ymax,zmax);
            gp_Pnt corners[8] = {
                gp_Pnt(xmin,ymin,zmin), gp_Pnt(xmin,ymin,zmax), gp_Pnt(xmin,ymax,zmin), gp_Pnt(xmin,ymax,zmax),
                gp_Pnt(xmax,ymin,zmin), gp_Pnt(xmax,ymin,zmax), gp_Pnt(xmax,ymax,zmin), gp_Pnt(xmax,ymax,zmax)
            };
            Bnd_Box tb; tb.SetGap(0.0);
            for (int i=0;i<8;++i){ gp_Pnt p = corners[i]; p.Transform(L); tb.Add(p); }
            totalBox.Add(tb);
            has = true;
            continue;
        }

        Handle(AIS_Triangulation) tri = Handle(AIS_Triangulation)::DownCast(obj);
        if (!tri.IsNull()) {
            size_t key = reinterpret_cast<size_t>(tri.get());
            auto it = gSceneState.ObjectCenters.find(key);
            auto itR = gSceneState.ObjectHalfDiagonal.find(key);
            if (it != gSceneState.ObjectCenters.end() && itR != gSceneState.ObjectHalfDiagonal.end()) {
                gp_Pnt c = it->second;
                double r = itR->second;
                // use cube of half-length r around center
                gp_Pnt pmin(c.X()-r, c.Y()-r, c.Z()-r);
                gp_Pnt pmax(c.X()+r, c.Y()+r, c.Z()+r);
                Bnd_Box bb; bb.SetGap(0.0); bb.Update(pmin.X(), pmin.Y(), pmin.Z(), pmax.X(), pmax.Y(), pmax.Z());
                totalBox.Add(bb);
                has = true;
            }
            continue;
        }
    }

    if (!has) return 0.0;
    Standard_Real xmin,ymin,zmin,xmax,ymax,zmax; totalBox.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    double dx = double(xmax - xmin);
    double dy = double(ymax - ymin);
    double dz = double(zmax - zmin);
    double halfDiag = 0.5 * std::sqrt(dx*dx + dy*dy + dz*dz);
    return halfDiag;
}

// 计算并应用 “零件组整体平移到骨骼组”
void ComputeAndApplyTranslation(
    const Handle(AIS_InteractiveContext)& ctx,
    std::vector<Handle(AIS_InteractiveObject)>& parts,
    const std::vector<Handle(AIS_InteractiveObject)>& bones,
    gp_Trsf& outTrsf)
{
    if (parts.empty() || bones.empty()) return;

    gp_Pnt centerBones = ComputeGroupCenter(ctx, bones);
    gp_Pnt centerParts = ComputeGroupCenter(ctx, parts);

    // compute approximate group 'radius' for safe separation
    double halfDiagParts = ComputeGroupHalfDiagonal(parts);
    double halfDiagBones = ComputeGroupHalfDiagonal(bones);

    // margin to avoid parts touching bones (units same as model)
    const double margin = 10.0; // mm (tunable)
    double safeDistance = halfDiagParts + halfDiagBones + margin;

    gp_Vec v(centerParts, centerBones); // vector from parts center -> bones center
    double dist = v.Magnitude();

    gp_Vec shift;
    if (dist < 1e-6) {
        // centers coincide -> pick arbitrary direction to move parts to "safeDistance" from bones
        gp_Vec dir(1.0, 0.0, 0.0);
        gp_Pnt target = centerBones.Translated(-dir.Normalized() * safeDistance);
        shift = gp_Vec(centerParts, target);
    } else {
        gp_Vec dir = v;
        dir.Normalize();
        // target center for parts = bones center - dir * safeDistance
        gp_Pnt target = centerBones.Translated(-dir * safeDistance);
        shift = gp_Vec(centerParts, target);
    }

    gp_Trsf tr; tr.SetTranslation(shift);

    // 对 parts 的 LocalTransformation 右乘该平移（保持原相对关系）
    for (auto& p : parts) {
        if (p.IsNull()) continue;
        gp_Trsf cur = p->LocalTransformation();
        gp_Trsf newL = cur.Multiplied(tr);
        p->SetLocalTransformation(newL);
        if (!ctx.IsNull()) ctx->Redisplay(p, Standard_False);
    }

    outTrsf = tr; // 保存平移
}

// 导入 STL 的逻辑（骨骼永远是世界坐标）
void ImportSTL(SceneState& S, const Handle(AIS_InteractiveObject)& boneObj)
{
    if (boneObj.IsNull() ) return;

    // Ensure scene context
    if (S.Ctx.IsNull()) S.Ctx = gSceneState.Ctx;

    // STL 保持原始坐标（World） —— 不改变 LocalTransformation
    S.Bones.push_back(boneObj);

    if (!S.HasBoneRef) {
        S.HasBoneRef = true;

        // 如果之前已有 STEP，则立即对所有 STEP 做整体平移
        if (!S.Parts.empty() && !S.HasPartToBoneTrsf) {
            ComputeAndApplyTranslation(S.Ctx, S.Parts, S.Bones, S.PartToBoneTrsf);
            S.HasPartToBoneTrsf = true;
            qDebug() << "[SceneState] Parts aligned to Bones after first STL import.";
        }
    }

    // 显示并刷新
    if (!S.Ctx.IsNull()) {
        S.Ctx->Display(boneObj, Standard_True);
        S.Ctx->UpdateCurrentViewer();
    }
}

// 导入 STEP 的逻辑
void ImportSTEP(SceneState& S, const Handle(AIS_InteractiveObject)& partObj)
{
    if (partObj.IsNull()) return;

    // Ensure scene context
    if (S.Ctx.IsNull()) S.Ctx = gSceneState.Ctx;

    // 记录到 Parts 列表
    S.Parts.push_back(partObj);

    // 如果还没有骨骼（HasBoneRef = false）：STEP 暂时不变换，只显示
    if (!S.HasBoneRef) {
        if (!S.Ctx.IsNull()) S.Ctx->Display(partObj, Standard_True);
        qDebug() << "[SceneState] STEP imported before Bones; displayed without alignment.";
        return;
    }

    // 如果骨骼已出现且还没对齐过：调用 ComputeAndApplyTranslation 并保存
    if (S.HasBoneRef && !S.HasPartToBoneTrsf) {
        ComputeAndApplyTranslation(S.Ctx, S.Parts, S.Bones, S.PartToBoneTrsf);
        S.HasPartToBoneTrsf = true;
        if (!S.Ctx.IsNull()) S.Ctx->Display(partObj, Standard_True);
        qDebug() << "[SceneState] First alignment of Parts to Bones performed.";
        return;
    }

    // 如果骨骼已出现且已对齐过：对新 STEP 右乘 PartToBoneTrsf
    if (S.HasBoneRef && S.HasPartToBoneTrsf) {
        gp_Trsf cur = partObj->LocalTransformation();
        gp_Trsf newL = cur.Multiplied(S.PartToBoneTrsf);
        partObj->SetLocalTransformation(newL);
        if (!S.Ctx.IsNull()) S.Ctx->Display(partObj, Standard_True);
        if (!S.Ctx.IsNull()) S.Ctx->Redisplay(partObj, Standard_False);
        qDebug() << "[SceneState] New STEP aligned using saved PartToBoneTrsf.";
        return;
    }
}

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
    //    每个三角形使用 3 个独立节点（不做顶点合并）
    const Standard_Integer nbNodes = static_cast<Standard_Integer>(triCount) * 3;
    const Standard_Integer nbTriangles = static_cast<Standard_Integer>(triCount);

    Handle(Poly_Triangulation) tri =
        new Poly_Triangulation(nbNodes, nbTriangles, Standard_False);

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

    qDebug().noquote()
        << QStringLiteral("[LoadStlLightweight]已从二进制 STL 读取三角形数量")
        << "triCount =" << triCount
        << QStringLiteral(", 节点数 =") << nbNodes;

    // 5) 构造 AIS_Triangulation（真正轻量级显示，不改坐标）
    Handle(AIS_Triangulation) aisTri = new AIS_Triangulation(tri);
    aisTri->SetDisplayMode(AIS_WireFrame);        // 或 AIS_Shaded
    aisTri->SetColor(Quantity_NOC_RED);           // 随便选个颜色

    // compute triangulation center and cache it
    {
        Standard_Real xmin=RealLast(), ymin=RealLast(), zmin=RealLast();
        Standard_Real xmax=-RealLast(), ymax=-RealLast(), zmax=-RealLast();
        for (Standard_Integer i=1;i<=tri->NbNodes();++i){ gp_Pnt pt = tri->Node(i); if (pt.X()<xmin) xmin=pt.X(); if (pt.Y()<ymin) ymin=pt.Y(); if (pt.Z()<zmin) zmin=pt.Z(); if (pt.X()>xmax) xmax=pt.X(); if (pt.Y()>ymax) ymax=pt.Y(); if (pt.Z()>zmax) zmax=pt.Z(); }
        if (xmax>=xmin) {
            gp_Pnt center((xmin+xmax)*0.5, (ymin+ymax)*0.5, (zmin+zmax)*0.5);
            double dx = double(xmax - xmin);
            double dy = double(ymax - ymin);
            double dz = double(zmax - zmin);
            double halfDiag = 0.5 * std::sqrt(dx*dx + dy*dy + dz*dz);
            size_t key = reinterpret_cast<size_t>(aisTri.get());
            gSceneState.ObjectCenters[key] = center;
            gSceneState.ObjectHalfDiagonal[key] = halfDiag;
        }
    }

    ctx->Display(aisTri, Standard_True);

    qDebug().noquote()
        << QStringLiteral("[LoadStlLightweight] 使用 AIS_Triangulation 轻量显示 STL")
        << QString::fromStdString(file);

    return aisTri;
}













