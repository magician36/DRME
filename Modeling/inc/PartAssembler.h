#pragma once
#include "PartGraph.h"
#include <AIS_InteractiveContext.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax2.hxx>
#include <string>

// === 装配器类：负责零件装配逻辑 ===
class PartAssembler
{
public:
    // 构造函数
    PartAssembler(PartGraph* graph, const Handle(AIS_InteractiveContext)& context);

    // === 装配实现（支持重复重新对齐不同孔） ===
    // partA: 要移动的零件名称
    // partB: 目标零件名称（保持不动）
    // holeIndexB: 在零件B上选择的孔索引
    // 返回: 装配是否成功
    bool AssembleParts(const std::string& partA, const std::string& partB, int holeIndexB);

    // 公共接口：将计算得到的 world->world 变换合成为模型的本地变换并应用
    bool ApplyTransformation(PartInfo& partInfo, const gp_Trsf& trsf);

private:
    // 构造稳定的参考坐标系（用于精确变换）
    gp_Ax2 BuildFrame(const gp_Ax1& axis) const;

    // 查找源零件上与目标孔类型匹配的孔轴
    bool FindSourceAxis(const PartInfo& partInfo, HoleType targetHoleType, gp_Ax1& outAxis) const;

    // 更新装配约束关系
    void UpdateConstraints(PartInfo& partA, const std::string& partBName, HoleType holeType);

    // 计算并输出装配信息（用于调试）
    void LogAssemblyInfo(const std::string& partA, const std::string& partB, 
                        int holeIndex, const gp_Ax1& sourceAxis, 
                        const gp_Ax1& targetAxis) const;

private:
    PartGraph* m_graph;                                  // 零件图引用
    Handle(AIS_InteractiveContext) m_context;            // 显示上下文
};
