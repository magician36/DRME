#pragma once
// 集成所有与“棒 / 螺钉 / 滑块”约束相关的接口，
// 不修改原有代码，仅新增本文件供统一调用或后续扩展。
// 说明：现有约束逻辑分散在 PartGraph / AssemblyDialog / PartAssembler 中。
// 本文件做轻量式封装/聚合，保持原逻辑不变，只提供更清晰的调用入口。

#include <string>
#include <vector>
#include <gp_Ax1.hxx>
#include "PartGraph.h"
class AIS_InteractiveContext; // 前向声明，避免引入多余依赖
class PartAssembler;          // 前向声明

// =============== 原子约束类型封装（语义标签） ===============
struct RodConstraintTag { };      // 棒相关约束标签
struct ScrewConstraintTag { };    // 螺钉相关约束标签
struct SliderConstraintTag { };   // 滑块相关约束标签

// =============== 约束创建结果 ===============
struct CreateConstraintResult
{
    bool success = false;      // 是否成功
    std::string message;       // 说明信息
};

// =============== 聚合管理器 ===============
class ConstraintManager
{
public:
    ConstraintManager(PartGraph* graph, const Handle(AIS_InteractiveContext)& ctx);

    // 普通零件之间的简单连接（不做几何对齐，只记录）
    CreateConstraintResult LinkParts(const std::string& part, const std::string& target, HoleType type);

    // 滑块 + 棒：执行几何装配并写入轴向 Mate（沿滑块棒孔轴平移）
    CreateConstraintResult AssembleSliderToRod(const std::string& sliderName,
                                              const std::string& rodName,
                                              int holeIndex);

    // 滑块 + 螺钉：螺钉仅沿滑块“螺钉孔”轴向平移
    CreateConstraintResult AssembleScrewToSlider(const std::string& screwName,
                                                 const std::string& sliderName,
                                                 int holeIndex);

    // 查询：返回滑块对应的“棒”Mate（若存在）
    const MateConstraint* FindRodMate(const std::string& slider) const;

    // 查询：返回与滑块装配的所有螺钉名称
    std::vector<std::string> GetScrews(const std::string& slider) const;

private:
    // 内部通用装配（rodOrScrew -> slider）
    CreateConstraintResult assembleToSlider(const std::string& movingPart,
                                            const std::string& sliderName,
                                            int holeIndex,
                                            HoleType holeType);

    PartGraph* m_graph = nullptr;
    Handle(AIS_InteractiveContext) m_context; // 保留，用于后续可能的高亮/交互扩展
};

