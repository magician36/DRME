#define _USE_MATH_DEFINES
#include <cmath>
#include "PartAssembler.h"
#include "AIS_ModelWithAxis.h"
#include <iostream>
#include <algorithm>
#include <QDebug>
#include <Bnd_Box.hxx> // 新增: 计算包围盒以获中心
#include <BRepBndLib.hxx> // 新增: 生成包围盒

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

    qDebug().noquote() << QString("[PartAssembler] 目标孔: 零件=%1, 索引=%2, 类型=%3")
        .arg(QString::fromStdString(partB))
        .arg(holeIndexB)
        .arg(targetHoleType == HoleType::RodHole ? "RodHole" : "ScrewHole");

    // === 查找源零件上的匹配孔 ===
    gp_Ax1 sourceAxis;
    if (!FindSourceAxis(infoA, targetHoleType, sourceAxis)) {
        std::cerr << "[PartAssembler] 未找到匹配的源孔" << std::endl;
        return false;
    }

    // === 如果方向反向，翻转源轴 ===
    if (sourceAxis.Direction().Dot(targetAxis.Direction()) < 0) {
        gp_Dir flippedDir(-sourceAxis.Direction().X(), 
                         -sourceAxis.Direction().Y(), 
                         -sourceAxis.Direction().Z());
        sourceAxis = gp_Ax1(sourceAxis.Location(), flippedDir);
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

    // === 新增：螺钉装配到滑块螺钉孔时，对齐“螺钉整体中心”到“孔中心”且保持同轴 ===
    if (infoA.type == PartType::Screw && infoB.type == PartType::Slider && targetHoleType == HoleType::ScrewHole) {
        // 计算螺钉当前局部包围盒中心（已变换后）
        Bnd_Box box; box.SetGap(0.0);
        BRepBndLib::Add(infoA.model->Shape(), box);
        if (!box.IsVoid()) {
            Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
            box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
            gp_Pnt screwCenter((xmin + xmax) * 0.5, (ymin + ymax) * 0.5, (zmin + zmax) * 0.5);
            gp_Trsf curL = infoA.model->LocalTransformation();
            screwCenter.Transform(curL);
            gp_Pnt holeCenter = targetAxis.Location();
            gp_Vec shift(screwCenter, holeCenter); // 需要补偿的平移
            gp_Vec curT(curL.TranslationPart());
            gp_Trsf newL = curL; newL.SetTranslationPart(curT + shift);
            infoA.model->SetLocalTransformation(newL);
            if (!m_context.IsNull()) m_context->Redisplay(infoA.model, Standard_False);
            qDebug().noquote() << QString("[PartAssembler] 螺钉中心与孔中心已对齐，Δ=(%1,%2,%3)")
                .arg(shift.X(),0,'f',3).arg(shift.Y(),0,'f',3).arg(shift.Z(),0,'f',3);
            // ==== 追加：沿孔轴方向打入一定距离（保持不修改原有代码与注释） ====
            // 依据螺钉包围盒在轴方向的长度，取 30% 作为打入距离
            gp_Dir axisDir = targetAxis.Direction();
            double minProj = 1e100, maxProj = -1e100;
            for (int ix=0; ix<2; ++ix)
                for (int iy=0; iy<2; ++iy)
                    for (int iz=0; iz<2; ++iz) {
                        gp_Pnt p(ix?xmax:xmin, iy?ymax:ymin, iz?zmax:zmin);
                        p.Transform(curL); // 变换到世界再投影
                        double proj = p.X()*axisDir.X() + p.Y()*axisDir.Y() + p.Z()*axisDir.Z();
                        if (proj < minProj) minProj = proj;
                        if (proj > maxProj) maxProj = proj;
                    }
            double screwLenAxis = std::max(0.0, maxProj - minProj);
            double depthRatio = 0.30; // 打入比例，可调
            double depth = screwLenAxis * depthRatio;
            // 负方向作为“向里”
            gp_Vec push = gp_Vec(axisDir.XYZ()) * (-depth);
            gp_Trsf afterPush = infoA.model->LocalTransformation();
            gp_Vec curT2(afterPush.TranslationPart());
            afterPush.SetTranslationPart(curT2 + push);
            infoA.model->SetLocalTransformation(afterPush);
            if (!m_context.IsNull()) m_context->Redisplay(infoA.model, Standard_False);
            qDebug().noquote() << QString("[PartAssembler] 螺钉沿孔轴打入距离=%1 (占轴向长度=%2%)")
                .arg(depth,0,'f',3).arg(depthRatio*100.0,0,'f',1);
        }
    }

    // === 更新约束关系 ===
    UpdateConstraints(infoA, partB, targetHoleType);

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
        // 应用变换到模型
        partInfo.model->SetLocalTransformation(trsf);
        
        // 刷新显示
        if (!m_context.IsNull()) {
            m_context->Redisplay(partInfo.model, Standard_True);
        }
        
        // 更新所有孔轴到新位置（保持后续再次选择正确）
        for (auto& hole : partInfo.holes) {
            gp_Ax1 axis = hole.second;
            axis.Transform(trsf);
            hole.second = axis;
        }
        
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
