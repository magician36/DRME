#include "BonePointTool.h"
//#include "OCCModeling.h"
#include "OCCTWidget.h" // Added for OCCTWidget access
#include "MainWindow_OSG.h" // Added for MainWindow access
#include "BasicFunction.h"      // ImportStp
#include "AIS_ModelWithAxis.h"  // 直接 new 模型
#include <QMessageBox>
#include <QCursor>
#include <AIS_Shape.hxx>
#include <TopoDS_Shape.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
//#include <BRepExtrema_DistShapeShape.hxx>
#include <gp_Lin.hxx>
#include <gp_Vec.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <QInputDialog>
#include <QFileInfo>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <AIS_Triangulation.hxx>
#include <Poly_Triangulation.hxx>
#include <Poly_Triangle.hxx>
#include <cmath>
#include <array>
#include <BRepAdaptor_Surface.hxx>
#include <Geom_Surface.hxx>
#include <Geom_Plane.hxx>

// 新增：Möller–Trumbore 射线-三角形相交
static bool RayIntersectTriangleMT(
    const gp_Pnt& O, const gp_Dir& D,
    const gp_Pnt& A, const gp_Pnt& B, const gp_Pnt& C,
    Standard_Real& outT)
{
    const gp_Vec e1(A, B);
    const gp_Vec e2(A, C);
    const gp_Vec pvec = gp_Vec(D) ^ e2;
    const Standard_Real det = e1.Dot(pvec);
    if (std::abs(det) < 1e-12) return false;

    const Standard_Real invDet = 1.0 / det;
    const gp_Vec tvec(O, A);
    const Standard_Real u = tvec.Dot(pvec) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    const gp_Vec qvec = tvec ^ e1;
    const Standard_Real v = gp_Vec(D).Dot(qvec) * invDet;
    if (v < 0.0 || (u + v) > 1.0) return false;

    const Standard_Real t = e2.Dot(qvec) * invDet;
    if (t < 0.0) return false;

    outT = t;
    return true;
}

BonePointTool::BonePointTool(const Handle(AIS_InteractiveContext)& context, const Handle(V3d_View)& view, QWidget* parentWidget)
    : m_context(context), m_view(view), m_parentWidget(parentWidget), m_enabled(false)
{
}

BonePointTool::~BonePointTool()
{
    // 清理逻辑（如果需要）
}

void BonePointTool::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    if (m_parentWidget) {
        m_parentWidget->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

bool BonePointTool::IsEnabled() const
{
    return m_enabled;
}

bool BonePointTool::HandleMousePress(int x, int y, AIS_ColoredShape* fallbackShape)
{
    if (!m_enabled) return false;

    // 弹出确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        m_parentWidget,
        QStringLiteral("标记确认"),
        QStringLiteral("是否要在此处标记一个红点？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) {
        // 用户取消标记
        return true; // 事件已处理，但未生成点
    }

    // 再 MoveTo 一次，确保当前位置的检测信息是最新的
    m_context->MoveTo(x, y, m_view, Standard_False);

    gp_Pnt hitP;
    Handle(AIS_InteractiveObject) ownerObj;
    bool hasHit = false;

    // 对齐用的三角网格法线（若命中三角）
    gp_Dir triHitNormal(0.0, 0.0, 1.0);
    bool  hasTriNormal = false;

    // 保存平面对象和射线方向
    Handle(Geom_Plane) hitPlane;
    gp_Dir rayDirD(0,0,1);
    bool hasRayDir = false;

    // 使用 ConvertWithProj 生成真正射线
    Standard_Real rx, ry, rz, rdx, rdy, rdz;
    m_view->ConvertWithProj(x, y, rx, ry, rz, rdx, rdy, rdz);
    gp_Pnt rayO(rx, ry, rz);
    gp_Dir rayD(rdx, rdy, rdz);
    gp_Lin ray(rayO, rayD);
    gp_Pnt viewP(rx, ry, rz); // 兜底点

    // 方向向量（相机->场景）
    gp_Vec rayDir(rayO, viewP);
    if (rayDir.Magnitude() > 1e-9) {
        rayDirD = gp_Dir(rayDir);
        hasRayDir = true;
    }

    // 1) 优先使用 OCCT 的精确射线检测 (Ray Casting)
    if (m_context->HasDetected())
    {
        ownerObj = m_context->DetectedInteractive();
        if (!ownerObj.IsNull())
        {
            // 先处理 AIS_Triangulation
            Handle(AIS_Triangulation) aisTri = Handle(AIS_Triangulation)::DownCast(ownerObj);
            if (!aisTri.IsNull())
            {
                Handle(Poly_Triangulation) tri = aisTri->GetTriangulation();
                if (!tri.IsNull() && tri->NbTriangles() > 0)
                {
                    gp_Trsf L = ownerObj->LocalTransformation();
                    Standard_Real bestT = RealLast();
                    gp_Pnt bestP;
                    gp_Dir bestN(0, 0, 1);
                    bool found = false;

                    for (Standard_Integer i = 1; i <= tri->NbTriangles(); ++i)
                    {
                        Poly_Triangle T = tri->Triangle(i);
                        Standard_Integer n1, n2, n3;
                        T.Get(n1, n2, n3);

                        gp_Pnt A = tri->Node(n1); A.Transform(L);
                        gp_Pnt B = tri->Node(n2); B.Transform(L);
                        gp_Pnt C = tri->Node(n3); C.Transform(L);

                        Standard_Real t;
                        if (RayIntersectTriangleMT(rayO, rayD, A, B, C, t))
                        {
                            if (t < bestT)
                            {
                                bestT = t;
                                bestP = rayO.Translated(gp_Vec(rayD) * t);
                                gp_Vec n = gp_Vec(A, B) ^ gp_Vec(A, C);
                                if (n.SquareMagnitude() > 1e-18) {
                                    n.Normalize();
                                    bestN = gp_Dir(n);
                                }
                                found = true;
                            }
                        }
                    }

                    if (found)
                    {
                        hitP = bestP;
                        triHitNormal = bestN;
                        hasTriNormal = true;
                        hasHit = true;
                    }
                    else
                    {
                        hitP = viewP;
                        hasHit = false;
                    }
                }
                else
                {
                    hitP = viewP;
                    hasHit = false;
                }
            }
            else
            {
                // 然后处理 AIS_Shape（原逻辑）
                Handle(AIS_Shape) aisSh = Handle(AIS_Shape)::DownCast(ownerObj);
                if (!aisSh.IsNull())
                {
                    TopoDS_Shape shape = aisSh->Shape();

                    IntCurvesFace_ShapeIntersector intersector;
                    intersector.Load(shape, 1e-3);
                    intersector.Perform(ray, 0.0, 1e6);

                    if (intersector.IsDone() && intersector.NbPnt() > 0)
                    {
                        Standard_Real minParam = 1e9;
                        gp_Pnt bestP;
                        bool found = false;

                        for (int i = 1; i <= intersector.NbPnt(); ++i)
                        {
                            Standard_Real param = intersector.WParameter(i);
                            if (param < minParam)
                            {
                                minParam = param;
                                bestP = intersector.Pnt(i);
                                found = true;
                            }
                        }

                        if (found) {
                            hitP = bestP;
                            hasHit = true;
                        } else {
                            hitP = viewP;
                            hasHit = false;
                        }
                    }
                    else
                    {
                        hitP = viewP;
                        hasHit = false;
                    }
                }
                else
                {
                    hitP = viewP;
                    hasHit = false;
                }
            }
        }
        else
        {
            hitP = viewP;
            hasHit = false;
        }
    }
    else
    {
        // 2) 未直接点中模型：尝试吸附到当前选中的 ais_shape (回退逻辑)
        gp_Pnt viewP2 = viewP;

        if (fallbackShape != nullptr)
        {
            ownerObj = Handle(AIS_ColoredShape)(fallbackShape);
            const TopoDS_Shape& boneShape = fallbackShape->Shape();
            // 简化回退：没有使用 BRepExtrema_DistShapeShape，直接使用视点
            hitP = viewP2;
            hasHit = true;
        }
        else
        {
            hitP = viewP2;
            ownerObj.Nullify();
        }
    }

    // 若未命中，直接返回
    if (!hasHit) return true;

    // ========== 仅允许在平面上放置（切平面逻辑） ==========
    // 要求 ownerObj 为 AIS_Shape，且命中面为平面
    Handle(AIS_Shape) ownerShape = Handle(AIS_Shape)::DownCast(ownerObj);
    if (ownerShape.IsNull()) return true;

    // 通过 ray 与 shape 获取命中点的面，并判断是否为平面
    IntCurvesFace_ShapeIntersector intersector;
    intersector.Load(ownerShape->Shape(), 1e-3);
    intersector.Perform(ray, 0.0, 1e6);
    if (!intersector.IsDone() || intersector.NbPnt() <= 0) return true;

    // 取最近命中的面
    Standard_Integer bestIdx = 1;
    Standard_Real bestParam = intersector.WParameter(1);
    for (Standard_Integer i=2; i<=intersector.NbPnt(); ++i) {
        if (intersector.WParameter(i) < bestParam) { bestParam = intersector.WParameter(i); bestIdx = i; }
    }
    TopoDS_Face hitFace = TopoDS::Face(intersector.Face(bestIdx));
    BRepAdaptor_Surface surf(hitFace);
    bool faceIsPlanar = (surf.GetType() == GeomAbs_Plane);
    if (!faceIsPlanar) {
        // 非平面，拒绝放置
        QMessageBox::information(m_parentWidget, QStringLiteral("提示"), QStringLiteral("只能在平面上放置滑块。"));
        return true;
    }

    Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surf.Surface().Surface());
    if (!plane.IsNull()) hitPlane = plane;

    gp_Pln pln = plane->Pln();
    gp_Dir normal = pln.Axis().Direction();

    // 保存切平面法线和外侧法线
    gp_Dir cutN = normal;   // 切平面法线（世界）
    gp_Dir outN = cutN;     // 滑块露出的方向（平面外侧）

    // rayDir 指向点击射线来的方向（朝相机）
    gp_Vec rayDir2(rayO, hitP);
    if (outN.Dot(gp_Dir(rayDir2)) > 0.0) {
        outN.Reverse();
    }

    // 3) 选择滑块文件
    QString baseDir = QStringLiteral("C:/Users/Administrator/Desktop/demo/DRME/stp/parts/Slider");
    QDir dir(baseDir);
    QStringList filters;
    filters << QStringLiteral("*.stp") << QStringLiteral("*.step");
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    
    QStringList items;
    for (const QFileInfo& f : files) {
        items << f.fileName();
    }

    if (items.isEmpty()) {
        QMessageBox::warning(m_parentWidget, QStringLiteral("错误"), QStringLiteral("未找到滑块模型文件！"));
        return true;
    }

    bool ok;
    QString item = QInputDialog::getItem(m_parentWidget, QStringLiteral("选择滑块"), 
                                         QStringLiteral("请选择要放置的滑块模型:"), items, 0, false, &ok);
    if (!ok || item.isEmpty()) return true;

    QString filePath = dir.absoluteFilePath(item);

    // ====== 参数：留缝 + 夹角阈值 ======
    const double kGap = 0.5;           // 留缝(单位=模型单位；若是mm就=0.5mm)
    const double kMaxAngleDeg = 8.0;   // 与切平面法向夹角阈值（越小越严格）
    const double kCosTh = std::cos(kMaxAngleDeg * M_PI / 180.0);

    // 必须命中平面（你前面已经拦过一次，这里再保险）
    if (hitPlane.IsNull()) {
        QMessageBox::information(m_parentWidget,
            QStringLiteral("提示"),
            QStringLiteral("未检测到切平面，无法放置滑块。请在骨头切平面上点击。"));
        return true;
    }

    // ========== 1) 缓存导入滑块 STEP（只导入一次）==========
    static QString s_cachedSliderPath;
    static TopoDS_Shape s_cachedSliderShape;
    static std::string s_cachedAxisFile;

    if (s_cachedSliderPath != filePath || s_cachedSliderShape.IsNull()) {
        s_cachedSliderPath = filePath;
        s_cachedSliderShape = ImportStp(filePath.toUtf8().constData());
        s_cachedAxisFile = std::string(filePath.toUtf8().constData()) + "_axes.json";

        if (s_cachedSliderShape.IsNull()) {
            QMessageBox::warning(m_parentWidget,
                QStringLiteral("错误"),
                QStringLiteral("滑块STEP导入失败！"));
            return true;
        }
    }

    // ========== 2) 创建一个新滑块 AIS（不走 LoadModelToWidget，不进树/PartGraph）==========
    Handle(AIS_ModelWithAxis) modelWithAxis = new AIS_ModelWithAxis(s_cachedSliderShape, s_cachedAxisFile);

    // ========== 3) 计算滑块几何中心（用于绕中心旋转 + 平移到 hitP）==========
    Bnd_Box box0;
    BRepBndLib::Add(s_cachedSliderShape, box0);
    Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
    box0.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    gp_Pnt center0((xmin + xmax)*0.5, (ymin + ymax)*0.5, (zmin + zmax)*0.5);

    // ========== 4) 切平面法向（并决定“外侧”方向：朝向相机一侧）==========
    gp_Dir planeN = hitPlane->Position().Axis().Direction();
    gp_Dir outN2 = planeN;

    // 让 outN 指向“相机所在那一侧”，保证滑块露在外侧
    if (hasRayDir) {
        // rayDirD 是从相机指向场景的方向
        // 外侧希望更靠近相机，所以 outN 与 rayDirD 同向时反转
        if (outN2.Dot(rayDirD) > 0.0) outN2.Reverse();
    }

    // ========== 5) 旋转：让滑块局部 +Z 与骨头平行（在切平面内），绕 center0 ==========
    // 计算在平面内的目标方向：将相机射线方向投影到平面内，作为与骨头平行的参考
    gp_Dir targetDir(0,0,1);
    bool hasTargetDir = false;
    if (hasRayDir) {
        gp_Vec d(rayDirD);
        // 去除法向分量，得到平面内分量
        gp_Vec inPlane = d - gp_Vec(planeN) * d.Dot(gp_Vec(planeN));
        if (inPlane.SquareMagnitude() > 1e-12) {
            inPlane.Normalize();
            targetDir = gp_Dir(inPlane);
            hasTargetDir = true;
        }
    }
    // 若无法从射线得到稳定的平面内方向，则使用平面坐标系的X方向作为兜底
    if (!hasTargetDir) {
        gp_Ax3 ax(hitPlane->Position());
        targetDir = ax.XDirection();
        hasTargetDir = true;
    }

    // 现在让局部 +Z 旋转到 targetDir（与骨头平行），而非法向
    gp_Dir zdir(0,0,1);
    gp_Trsf rotTrsf;
    if (zdir.IsParallel(targetDir, 1e-6)) {
        // 平行/反平行
        if (zdir.Dot(targetDir) < 0.0) {
            rotTrsf.SetRotation(gp_Ax1(center0, gp_Dir(1,0,0)), M_PI);
        } else {
            rotTrsf.SetForm(gp_Identity);
        }
    } else {
        gp_Dir axisDir = zdir ^ targetDir;
        Standard_Real ang = zdir.Angle(targetDir);
        rotTrsf.SetRotation(gp_Ax1(center0, axisDir), ang);
    }

    // 额外自转90度：围绕已对齐的局部+Z（即 targetDir）在中心处旋转
    {
        gp_Trsf spin90;
        // 反向自转90度
        spin90.SetRotation(gp_Ax1(center0, targetDir), -M_PI * 0.5);
        rotTrsf = spin90 * rotTrsf;
    }

    // ========== 6) 平移：把 center0 移到 hitP ==========
    gp_Trsf transTrsf;
    transTrsf.SetTranslation(gp_Vec(center0, hitP));

    // 先旋转再平移（rot 已绕 center0，不会跑中心）
    gp_Trsf baseTrsf = transTrsf * rotTrsf;

    // ========== 7) 夹角判定（与切平面法向接近 90°）==========
    {
        gp_Vec zW(0,0,1);
        zW.Transform(baseTrsf); // 仅旋转影响向量
        gp_Dir zWdir(zW);
        double angDeg = zWdir.Angle(planeN) * 180.0 / M_PI;
        if (std::abs(angDeg - 90.0) > kMaxAngleDeg) {
            QMessageBox::information(m_parentWidget,
                QStringLiteral("提示"),
                QStringLiteral("滑块与骨头方向不平行（与切平面法线不近似90°），放置失败。"));
            return true;
        }
    }

    // ========== 8) “露出平面外侧 + 留缝”校正：让滑块包围盒整体在平面外侧，并留 kGap ==========
    gp_Trsf finalTrsf = baseTrsf;

    auto getCorners = [](Standard_Real xmin, Standard_Real ymin, Standard_Real zmin,
                         Standard_Real xmax, Standard_Real ymax, Standard_Real zmax) {
        std::array<gp_Pnt,8> c;
        c[0] = gp_Pnt(xmin,ymin,zmin);
        c[1] = gp_Pnt(xmax,ymin,zmin);
        c[2] = gp_Pnt(xmin,ymax,zmin);
        c[3] = gp_Pnt(xmax,ymax,zmin);
        c[4] = gp_Pnt(xmin,ymin,zmax);
        c[5] = gp_Pnt(xmax,ymin,zmax);
        c[6] = gp_Pnt(xmin,ymax,zmax);
        c[7] = gp_Pnt(xmax,ymax,zmax);
        return c;
    };

    // 计算变换后的滑块 bbox（滑块很小，这一步很快）
    {
        BRepBuilderAPI_Transform tb(s_cachedSliderShape, finalTrsf, true);
        TopoDS_Shape placed = tb.Shape();

        Bnd_Box pb;
        BRepBndLib::Add(placed, pb);
        Standard_Real axmin, aymin, azmin, axmax, aymax, azmax;
        pb.Get(axmin, aymin, azmin, axmax, aymax, azmax);

        auto corners = getCorners(axmin, aymin, azmin, axmax, aymax, azmax);

        gp_Pnt plO = hitPlane->Position().Location();
        double minS = 1e100;
        for (const auto& p : corners) {
            double s = gp_Vec(plO, p).Dot(gp_Vec(outN2)); // 平面外侧为正
            if (s < minS) minS = s;
        }

        // 让“最靠近平面”的点也 >= kGap
        double shift = kGap - minS;
        if (shift > 0.0) {
            gp_Trsf shiftTrsf;
            shiftTrsf.SetTranslation(gp_Vec(outN2) * shift);
            finalTrsf = shiftTrsf * finalTrsf;
        }
    }

    // ========== 9) 显示 + 记录 marker（用于跟随骨头移动）==========
    modelWithAxis->SetLocalTransformation(finalTrsf);
    m_context->Display(modelWithAxis, Standard_False);
    m_context->Redisplay(modelWithAxis, Standard_False);

    // 记录“滑块原点”最终世界坐标 -> 转 owner 局部
    gp_Pnt finalOriginWorld(0,0,0);
    finalOriginWorld.Transform(finalTrsf);

    gp_Pnt localP = finalOriginWorld;
    if (!ownerObj.IsNull()) {
        gp_Trsf ownerL = ownerObj->LocalTransformation();
        gp_Trsf Linv = ownerL; Linv.Invert();
        localP = finalOriginWorld.Transformed(Linv);
    }

    BoneMarker mk;
    mk.owner = ownerObj;
    mk.geom = new Geom_CartesianPoint(hitP); // 点击点参考
    mk.aisPoint = nullptr;
    mk.localPnt = localP;
    mk.attachedModel = modelWithAxis;
    m_markers.push_back(mk);

    m_view->Redraw();
    return true;
}

void BonePointTool::UpdateMarkers()
{
    if (m_context.IsNull()) return;
    
    for (auto& marker : m_markers)
    {
        if (marker.owner.IsNull())
            continue;
            
        // 获取 owner 当前的变换矩阵
        gp_Trsf currentTrsf = marker.owner->LocalTransformation();
        
        // 将局部坐标转换为当前世界坐标
        gp_Pnt worldPnt = marker.localPnt.Transformed(currentTrsf);
        
        // 更新几何点的坐标
        if (!marker.geom.IsNull())
            marker.geom->SetPnt(worldPnt);
        
        // 更新红点位置（如果有）
        if (!marker.aisPoint.IsNull())
            m_context->Redisplay(marker.aisPoint, Standard_False);

        // 更新关联模型的位置
        if (!marker.attachedModel.IsNull()) {
            gp_Trsf trsf = marker.attachedModel->LocalTransformation();
            trsf.SetTranslationPart(worldPnt.XYZ());
            marker.attachedModel->SetLocalTransformation(trsf);
            m_context->Redisplay(marker.attachedModel, Standard_False);
        }
    }
    
    // 刷新视图
    if (!m_view.IsNull())
        m_view->Redraw();
}
