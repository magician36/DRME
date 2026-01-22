#define _USE_MATH_DEFINES
#include <cmath>
#include "PartAssembler.h"
#include "AIS_ModelWithAxis.h"
#include <iostream>
#include <algorithm>
#include <QDebug>
#include <Bnd_Box.hxx> // 新增: 计算包围盒以获中心
#include <BRepBndLib.hxx> // 新增: 生成包围盒
#include <Precision.hxx>
#if __has_include(<IntCurvesFace_ShapeIntersector.hxx>)
  #include <IntCurvesFace_ShapeIntersector.hxx>
  #define HAS_OCCT_SHAPEINTERSECTOR 1
#else
  #define HAS_OCCT_SHAPEINTERSECTOR 0
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// === 构造函数 ===
PartAssembler::PartAssembler(PartGraph* graph, const Handle(AIS_InteractiveContext)& context)
    : m_graph(graph), m_context(context)
{
}

// === 装配实现（支持重复重新对齐不同孔） ===
bool PartAssembler::AssembleParts(const std::string& partA, const std::string& partB, int holeIndexB)
{
    if (!m_graph) {
        std::cerr << "[PartAssembler] PartGraph 为空" << std::endl;
        return false;
    }
    qDebug() << "[AssembleParts] 输入参数:"\
        << "moving =" << QString::fromStdString(partA)\
        << "fixed =" << QString::fromStdString(partB)\
        << "hole=" << holeIndexB;

    // 通过 friend 访问获取零件信息（可修改）
    auto& parts = m_graph->parts;
    auto itA = parts.find(partA);
    auto itB = parts.find(partB);

    if (itA == parts.end() || itB == parts.end()) {
        std::cerr << "[PartAssembler] 未找到零件: " << partA << " 或 " << partB << std::endl;
        return false;
    }

    PartInfo& infoA = itA->second;
    PartInfo& infoB = itB->second;

    // 检查模型有效性
    if (infoA.model.IsNull() || infoB.model.IsNull()) {
        std::cerr << "[PartAssembler] 模型句柄为空" << std::endl;
        return false;
    }

    // 检查孔索引有效性
    if (holeIndexB < 0 || holeIndexB >= static_cast<int>(infoB.holes.size())) {
        std::cerr << "[PartAssembler] 孔索引超出范围: " << holeIndexB << std::endl;
        return false;
    }

    // === 获取目标孔轴（考虑零件B的当前变换） ===
    gp_Ax1 targetAxis = infoB.holes[holeIndexB].second;  // 原始存储轴
    if (!infoB.model.IsNull()) {
        gp_Trsf transformB = infoB.model->LocalTransformation();
        targetAxis.Transform(transformB);
    }
    HoleType targetHoleType = infoB.holes[holeIndexB].first;

    qDebug().noquote() << QString("[PartAssembler] 目标孔: 零件=%1, 索引=%2, 类型=%3")\
        .arg(QString::fromStdString(partB))\
        .arg(holeIndexB)\
        .arg(targetHoleType == HoleType::RodHole ? "RodHole" : "ScrewHole");

    // === 查找源零件上的匹配孔 ===
    gp_Ax1 sourceAxis;
    if (!FindSourceAxis(infoA, targetHoleType, sourceAxis)) {
        std::cerr << "[PartAssembler] 未找到匹配的源孔" << std::endl;
        return false;
    }

    // Transform source axis from model-local to world coordinates so comparisons are consistent
    if (!infoA.model.IsNull()) {
        gp_Trsf transformA = infoA.model->LocalTransformation();
        sourceAxis.Transform(transformA);
    }

    // === 方向校正：始终使源轴与目标孔轴同向（若点积<0则翻转） ===
    {
        gp_Dir dirS = sourceAxis.Direction();
        gp_Dir dirT = targetAxis.Direction();
        // 原逻辑含螺钉反向特殊处理，现统一：保持同向，避免用户反馈的“螺钉方向装反”
        if (dirS.Dot(dirT) < 0.0) {
            dirS.Reverse();
            sourceAxis = gp_Ax1(sourceAxis.Location(), dirS);
            qDebug() << "[PartAssembler] 源轴方向已翻转以与目标孔同向";
        }
    }

    // === 构造稳定参考坐标系 ===
    gp_Ax2 frameSource = BuildFrame(sourceAxis);
    gp_Ax2 frameTarget = BuildFrame(targetAxis);

    // === 利用 SetDisplacement 精确生成变换 ===
    gp_Trsf trsf;
    trsf.SetDisplacement(frameSource, frameTarget);

    // === 应用变换 ===
    if (!ApplyTransformation(infoA, trsf)) {
        std::cerr << "[PartAssembler] 变换失败" << std::endl;
        return false;
    }

    // === 螺钉装配到滑块：无论当前选的是不是螺孔，都自动插入最近的螺孔 ===
    if (infoA.type == PartType::Screw && infoB.type == PartType::Slider)
    {
        // ---- 0) 决定要用的孔轴：优先当前目标是螺孔；否则在滑块上找最近的螺孔 ----
        gp_Ax1 holeAxisW = targetAxis;                // 默认用当前目标轴
        gp_Pnt refPointW = targetAxis.Location();     // 参考点（用于“最近螺孔”）
        bool foundScrewHole = (targetHoleType == HoleType::ScrewHole);

        if (!foundScrewHole) {
            // 参考点改用“螺钉当前中心”，更稳
            Bnd_Box sb; sb.SetGap(0.0);
            BRepBndLib::Add(infoA.model->Shape(), sb);
            if (!sb.IsVoid()) {
                Standard_Real xmin,ymin,zmin,xmax,ymax,zmax; sb.Get(xmin,ymin,zmin,xmax,ymax,zmax);
                gp_Pnt c((xmin+xmax)*0.5,(ymin+ymax)*0.5,(zmin+zmax)*0.5);
                c.Transform(infoA.model->LocalTransformation());
                refPointW = c;
            }

            // 在滑块上找“类型=螺孔”的轴，取与 refPointW 最近的一个
            Standard_Real best2 = RealLast();
            for (size_t i=0; i<infoB.holes.size(); ++i) {
                if (infoB.holes[i].first != HoleType::ScrewHole) continue;
                gp_Ax1 ax = infoB.holes[i].second;
                gp_Trsf LB = infoB.model->LocalTransformation();
                ax.Transform(LB);
                Standard_Real d2 = refPointW.SquareDistance(ax.Location());
                if (d2 < best2) { best2 = d2; holeAxisW = ax; foundScrewHole = true; }
            }
        }

        // ---- 1) 把“螺钉整体中心”对到“孔中心”（纯平移）----
        Bnd_Box box; box.SetGap(0.0);
        BRepBndLib::Add(infoA.model->Shape(), box);
        if (box.IsVoid()) return true;

        Standard_Real xmin,ymin,zmin,xmax,ymax,zmax;
        box.Get(xmin,ymin,zmin,xmax,ymax,zmax);

        gp_Trsf Lcur = infoA.model->LocalTransformation();
        gp_Pnt screwCenterLocal((xmin+xmax)*0.5,(ymin+ymax)*0.5,(zmin+zmax)*0.5);
        gp_Pnt screwCenter = screwCenterLocal.Transformed(Lcur);
        gp_Pnt holeCenter  = holeAxisW.Location();
        gp_Vec shift(screwCenter, holeCenter);

        gp_Trsf L1 = Lcur; L1.SetTranslationPart(gp_Vec(Lcur.TranslationPart()) + shift);
        infoA.model->SetLocalTransformation(L1);

        // ---- 2) 计算推进方向、螺钉轴向长度、入口/出口位置 ----
        const gp_Dir axDir = holeAxisW.Direction();

        // 2.1 方向：沿与“中心->孔中心”同向的轴向前进
        const double sign = (gp_Vec(axDir.XYZ()).Dot(shift) >= 0.0) ? +1.0 : -1.0;

        // 2.2 螺钉轴向长度（用对齐后的位置）
        gp_Trsf Lc = infoA.model->LocalTransformation();
        auto proj = [&](const gp_Pnt& p)->double { return p.X()*axDir.X() + p.Y()*axDir.Y() + p.Z()*axDir.Z(); };

        double minProj =  1e100, maxProj = -1e100;
        for (int ix=0; ix<2; ++ix)
        for (int iy=0; iy<2; ++iy)
        for (int iz=0; iz<2; ++iz) {
            gp_Pnt p(ix?xmax:xmin, iy?ymax:ymin, iz?zmax:zmin);
            p.Transform(Lc);
            const double v = proj(p);
            if (v < minProj) minProj = v;
            if (v > maxProj) maxProj = v;
        }
        const double screwLenAxis = std::max(0.0, maxProj - minProj);
        const double sFront = (sign > 0.0) ? maxProj : minProj; // 沿推进方向的“前端”投影

        // 2.3 入口/出口（优先几何求交；失败用滑块包围盒兜底）
        double sEnter = proj(holeCenter), sExit = sEnter;
        bool haveThickness = false;

#if HAS_OCCT_SHAPEINTERSECTOR
        try {
            IntCurvesFace_ShapeIntersector isec;
            isec.Load(infoB.model->Shape(), Precision::Confusion());
            isec.Perform(gp_Lin(holeAxisW), -Precision::Infinite(), Precision::Infinite());
            if (isec.NbPnt() >= 2) {
                const double a = proj(isec.Pnt(1)), b = proj(isec.Pnt(2));
                sEnter = std::min(a,b); sExit = std::max(a,b);
                haveThickness = true;
            }
        } catch(...) { /* ignore */ }
#endif
        if (!haveThickness) {
            Bnd_Box b; b.SetGap(0.0);
            BRepBndLib::Add(infoB.model->Shape(), b);
            Standard_Real bxmin,bymin,bzmin,bxmax,bymax,bzmax;
            b.Get(bxmin,bymin,bzmin,bxmax,bymax,bzmax);
            gp_Trsf LB = infoB.model->LocalTransformation();
            double Bmin= 1e100, Bmax=-1e100;
            for (int ix=0; ix<2; ++ix)
            for (int iy=0; iy<2; ++iy)
            for (int iz=0; iz<2; ++iz) {
                gp_Pnt p(ix?bxmax:bxmin, iy?bymax:bymin, iz?bzmax:bzmin);
                p.Transform(LB);
                const double v = proj(p);
                if (v < Bmin) Bmin = v; if (v > Bmax) Bmax = v;
            }
            // 把孔中心当作中点附近
            const double mid = (Bmin + Bmax) * 0.5;
            const double half= (Bmax - Bmin) * 0.5;
            sEnter = mid - half; sExit = mid + half;
        }

        // ---- 3) 目标：让“螺钉前端”进入入口面内的咬合深度 ----
        // 插到一半：把前端推进到孔厚度的中面
        const double thickness = std::max(0.0, sExit - sEnter);
        const double sTarget   = 0.5 * (sEnter + sExit);  // 孔内“中面”
        const double delta     = sTarget - sFront;        // 推进到中面

        gp_Trsf L2 = infoA.model->LocalTransformation();
        gp_Vec  t2(L2.TranslationPart());
        gp_Vec  push = gp_Vec(axDir.XYZ()) * delta;
        L2.SetTranslationPart(t2 + push);
        infoA.model->SetLocalTransformation(L2);

        if (!m_context.IsNull()) m_context->Redisplay(infoA.model, Standard_False);
        qDebug().noquote() << QString("[PartAssembler] 自动插入到螺孔：δ=%1, 厚度=%2, len=%3%4")\
            .arg(delta,0,'f',3).arg(thickness,0,'f',3).arg(screwLenAxis,0,'f',3)\
            .arg(foundScrewHole?QString():QString(" (fallback)"));

        return true;
    }

    // === 更新约束关系 ===
    UpdateConstraints(infoA, partB, targetHoleType);

    // 如果固定方是滑块且目标孔是 RodHole，并且移动方是 Rod 或 Screw，
    // 则建立 MateConstraint（滑块 <- 棒/螺钉）并写入 PartGraph
    if (targetHoleType == HoleType::RodHole) {
        if (!m_graph) {
            // nothing
        } else {
            if (infoB.type == PartType::Slider && (infoA.type == PartType::Rod || infoA.type == PartType::Screw)) {
                MateConstraint mc;
                mc.sliderName = partB; // fixed slider
                mc.rodName = partA;    // moving rod/screw
                mc.rodAxis = targetAxis; // targetAxis is already transformed to world
                mc.holeIndex = holeIndexB;
                m_graph->AddMate(mc);
            }
        }
    }

    // === 输出装配信息 ===
    LogAssemblyInfo(partA, partB, holeIndexB, sourceAxis, targetAxis);

    return true;
}

// === 构造稳定的参考坐标系 ===
gp_Ax2 PartAssembler::BuildFrame(const gp_Ax1& axis) const
{
    gp_Dir zDir = axis.Direction();

    // 选择参考方向（避免与 Z 轴平行）
    gp_Dir refDir = (std::abs(zDir.Z()) < 0.9) ? gp_Dir(0, 0, 1) : gp_Dir(1, 0, 0);

    gp_Vec vz(zDir);
    gp_Vec vref(refDir);
    gp_Vec vx = vz.Crossed(vref);

    // 如果叉积太小，换一个参考方向
    if (vx.Magnitude() < 1e-6) {
        refDir = gp_Dir(0, 1, 0);
        vref = gp_Vec(refDir);
        vx = vz.Crossed(vref);
    }

    gp_Dir xDir(vx);  // 自动归一化
    return gp_Ax2(axis.Location(), zDir, xDir);
}

// === 查找源零件上与目标孔类型匹配的孔轴 ===
bool PartAssembler::FindSourceAxis(const PartInfo& partInfo, HoleType targetHoleType, gp_Ax1& outAxis) const
{
    // 优先匹配孔类型
    for (const auto& hole : partInfo.holes) {
        if (hole.first == targetHoleType) {
            outAxis = hole.second;
            return true;
        }
    }

    // 如果没有匹配的孔，使用主轴
    if (!partInfo.model.IsNull()) {
        outAxis = partInfo.model->MainAxis();
        qDebug() << "[PartAssembler] 未找到匹配孔类型，使用主轴";
        return true;
    }

    return false;
}

// === 应用变换并更新孔轴位置 ===
bool PartAssembler::ApplyTransformation(PartInfo& partInfo, const gp_Trsf& trsf)
{
    try {
        // Compose the incoming world->world transform with the current local transform
        // New local transform Lnew should satisfy: world_new = trsf * world_old = trsf * Lold
        gp_Trsf Lold = partInfo.model->LocalTransformation();
        gp_Trsf Lnew = trsf; Lnew.Multiply(Lold); // Lnew = trsf * Lold

        // Apply new local transform
        partInfo.model->SetLocalTransformation(Lnew);

        // Refresh display
        if (!m_context.IsNull()) {
            m_context->Redisplay(partInfo.model, Standard_True);
        }

        // IMPORTANT: Do not modify partInfo.holes here. holes are stored in model-local coordinates.
        // Transforming them would convert them out of the "local" semantic and cause double transforms later.

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[PartAssembler] 变换异常: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "[PartAssembler] 变换未知异常" << std::endl;
        return false;
    }
}

// === 更新装配约束关系 ===
void PartAssembler::UpdateConstraints(PartInfo& partA, const std::string& partBName, HoleType holeType)
{
    // 记录约束（允许重复覆盖）
    partA.constraint.targetPart = partBName;
    partA.constraint.type = holeType;

    // 添加连接关系（防止重复）
    auto& connections = partA.connectedParts;
    if (std::find(connections.begin(), connections.end(), partBName) == connections.end()) {
        connections.push_back(partBName);
    }
}

// === 计算并输出装配信息 ===
void PartAssembler::LogAssemblyInfo(const std::string& partA, const std::string& partB,
    int holeIndex, const gp_Ax1& sourceAxis,
    const gp_Ax1& targetAxis) const
{
    // 计算位移长度
    double moveLen = sourceAxis.Location().Distance(targetAxis.Location());

    // 计算角度差
    double angle = sourceAxis.Direction().Angle(targetAxis.Direction());
    double angleDeg = angle * 180.0 / M_PI;

    qDebug().noquote() << QString("[PartAssembler] ✅ 装配完成:");
    qDebug().noquote() << QString("  源零件: %1").arg(QString::fromStdString(partA));
    qDebug().noquote() << QString("  目标零件: %1").arg(QString::fromStdString(partB));
    qDebug().noquote() << QString("  目标孔索引: %1").arg(holeIndex);
    qDebug().noquote() << QString("  位移距离: %1 mm").arg(moveLen, 0, 'f', 3);
    qDebug().noquote() << QString("  角度调整: %1°").arg(angleDeg, 0, 'f', 2);

    std::cout << "[PartAssembler] " << partA << " -> " << partB
        << " | 孔=" << holeIndex
        << " | 位移=" << moveLen
        << " | 角度=" << angle << std::endl;
}
