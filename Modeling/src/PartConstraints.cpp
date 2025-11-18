#include "PartConstraints.h"
#include "PartAssembler.h"
#include "AIS_ModelWithAxis.h"
#include <AIS_InteractiveContext.hxx>
#include <QDebug>
#include <BRepBuilderAPI_Transform.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>

ConstraintManager::ConstraintManager(PartGraph* graph, const Handle(AIS_InteractiveContext)& ctx)
    : m_graph(graph), m_context(ctx) {}

CreateConstraintResult ConstraintManager::LinkParts(const std::string& part,
                                                    const std::string& target,
                                                    HoleType type)
{
    CreateConstraintResult r; if (!m_graph) { r.message = "PartGraph null"; return r; }
    const auto &parts = m_graph->GetParts(); // 仅访问公开接口
    if (parts.find(part) == parts.end() || parts.find(target) == parts.end()) {
        r.message = "part not found"; return r;
    }
    m_graph->AddConstraint(part, target, type);
    r.success = true; r.message = "constraint linked"; return r;
}

CreateConstraintResult ConstraintManager::AssembleSliderToRod(const std::string& sliderName,
                                                              const std::string& rodName,
                                                              int holeIndex)
{
    CreateConstraintResult r;
    if (!m_graph) { r.message = "PartGraph null"; return r; }

    const auto& parts = m_graph->GetParts();
    auto itSlider = parts.find(sliderName);
    auto itRod = parts.find(rodName);
    if (itSlider == parts.end() || itRod == parts.end()) { r.message = "part not found"; return r; }

    const PartInfo& slider = itSlider->second;
    const PartInfo& rod = itRod->second;

    if (slider.type != PartType::Slider) { r.message = "moving part is not slider"; return r; }
    if (rod.type != PartType::Rod) { r.message = "fixed part is not rod"; return r; }

    if (holeIndex < 0 || holeIndex >= (int)slider.holes.size()) { r.message = "hole index invalid"; return r; }
    if (slider.model.IsNull() || rod.model.IsNull()) { r.message = "model null"; return r; }

    // 取出滑块上的选中孔（局部坐标）
    gp_Ax1 sourceAxisLocal = slider.holes[holeIndex].second;

    // 将滑块孔轴转换到世界坐标（必须与棒的轴在同一坐标系比较）
    gp_Trsf Lslider = slider.model->LocalTransformation();
    gp_Ax1 sourceAxisWorld = sourceAxisLocal;
    sourceAxisWorld.Transform(Lslider);

    // 在棒上找最近的 RodHole 轴（以世界坐标比较）
    gp_Ax1 bestRodAxisWorld;
    double bestDist = 1e100;
    bool found = false;

    gp_Trsf Lrod = rod.model->LocalTransformation();
    for (size_t i = 0; i < rod.holes.size(); ++i) {
        if (rod.holes[i].first != HoleType::RodHole) continue;
        gp_Ax1 ax = rod.holes[i].second;
        ax.Transform(Lrod); // to world
        double d = sourceAxisWorld.Location().Distance(ax.Location());
        if (d < bestDist) { bestDist = d; bestRodAxisWorld = ax; found = true; }
    }

    if (!found) {
        // 使用棒的主轴作为兜底（world）
        gp_Ax1 mainAx = rod.model->MainAxis();
        mainAx.Transform(Lrod);
        bestRodAxisWorld = mainAx;
    }

    // 如果方向相反则翻转源轴，使得两轴方向一致，避免出现180度的多义旋转
    {
        gp_Dir dirS = sourceAxisWorld.Direction();
        gp_Dir dirT = bestRodAxisWorld.Direction();
        if (dirS.Dot(dirT) < 0.0) {
            sourceAxisWorld.Reverse();
        }
    }

    // Build frames in world coordinates so SetDisplacement produces a world->world transform
    auto BuildFrameWorld = [](const gp_Ax1& axis)->gp_Ax2 {
        gp_Dir zDir = axis.Direction();
        gp_Dir refDir = (std::abs(zDir.Z()) < 0.9) ? gp_Dir(0,0,1) : gp_Dir(1,0,0);
        gp_Vec vz(zDir); gp_Vec vref(refDir);
        gp_Vec vx = vz.Crossed(vref);
        if (vx.Magnitude() < 1e-6) { refDir = gp_Dir(0,1,0); vref = gp_Vec(refDir); vx = vz.Crossed(vref); }
        gp_Dir xDir(vx);
        return gp_Ax2(axis.Location(), zDir, xDir);
    };

    gp_Ax2 frameSource = BuildFrameWorld(sourceAxisWorld);
    gp_Ax2 frameTarget = BuildFrameWorld(bestRodAxisWorld);

    gp_Trsf trsf;
    trsf.SetDisplacement(frameSource, frameTarget);

    // 应用到滑块：覆盖本地变换为 trsf
    try {
        // 更新显示变换（trsf 是 world->world，设为 LocalTransformation 直接把 slider 放置到目标）
        slider.model->SetLocalTransformation(trsf);
        if (!m_context.IsNull()) m_context->Redisplay(slider.model, Standard_True);

        // 注意：不修改 PartGraph 中存储的 holes（它们应始终为局部坐标）。
        // 之前修改 holes 会导致语义混乱和重复变换问题。
    }
    catch (...) {
        r.message = "transform apply failed"; return r;
    }

    // 构造 MateConstraint：sliderName（滑块） -> rodName（棒）
    MateConstraint mc;
    mc.sliderName = sliderName;
    mc.rodName = rodName;
    mc.rodAxis = bestRodAxisWorld;
    mc.holeIndex = holeIndex;

    m_graph->AddMate(mc);

    // 记录约束（把滑块标记为约束到棒）
    m_graph->AddConstraint(sliderName, rodName, HoleType::RodHole);

    r.success = true; r.message = "assemble slider->rod success";
    return r;
}

CreateConstraintResult ConstraintManager::AssembleScrewToSlider(const std::string& screwName,
                                                                const std::string& sliderName,
                                                                int holeIndex)
{
    return assembleToSlider(screwName, sliderName, holeIndex, HoleType::ScrewHole);
}

const MateConstraint* ConstraintManager::FindRodMate(const std::string& slider) const
{
    if (!m_graph) return nullptr; return m_graph->FindRodMateForSlider(slider);
}

std::vector<std::string> ConstraintManager::GetScrews(const std::string& slider) const
{
    if (!m_graph) return {}; return m_graph->GetScrewsForSlider(slider);
}

CreateConstraintResult ConstraintManager::assembleToSlider(
    const std::string& movingPart,     // B 表：移动的零件
    const std::string& fixedPart,      // A 表：固定的零件
    int holeIndex,
    HoleType holeType)
{
    CreateConstraintResult r;
    if (!m_graph) { r.message = "PartGraph null"; return r; }

    const auto& parts = m_graph->GetParts();

    auto itFixed = parts.find(fixedPart);
    auto itMov = parts.find(movingPart);

    if (itFixed == parts.end() || itMov == parts.end()) {
        r.message = "part not found";
        return r;
    }

    const PartInfo& fixInfo = itFixed->second;
    const PartInfo& movInfo = itMov->second;

    // C 表一定展示滑块的孔，所以 fixedPart 必须是滑块
    if (fixInfo.type != PartType::Slider) {
        r.message = "fixedPart is not slider";
        return r;
    }

    // 检查孔索引
    if (holeIndex < 0 || holeIndex >= (int)fixInfo.holes.size()) {
        r.message = "hole index invalid";
        return r;
    }

    if (fixInfo.model.IsNull() || movInfo.model.IsNull()) {
        r.message = "model null";
        return r;
    }

    // ---- 关键：严格 A 固定、B 移动 ----
    PartAssembler assembler(m_graph, m_context);
    bool ok = assembler.AssembleParts(
        movingPart,   // B 移动
        fixedPart,    // A 固定
        holeIndex
    );

    if (!ok) {
        r.message = "assemble failure";
        return r;
    }

    // === 建立 mate 数据并存入 PartGraph ===
    // 把滑块孔的轴转到世界坐标
    gp_Ax1 holeAxLocal = fixInfo.holes[holeIndex].second;
    gp_Trsf Ls = fixInfo.model->LocalTransformation();
    gp_Ax1 holeAxWorld(
        holeAxLocal.Location().Transformed(Ls),
        gp_Dir(holeAxLocal.Direction().Transformed(Ls))
    );

    MateConstraint mc;
    mc.sliderName = fixedPart;       // 滑块永远是固定方
    mc.rodName = movingPart;      // 移动方可能是棒或螺钉
    mc.rodAxis = holeAxWorld;
    mc.holeIndex = holeIndex;

    m_graph->AddMate(mc);

    // 同时记录装配约束
    m_graph->AddConstraint(fixedPart, movingPart, holeType);

    r.success = true;
    r.message = "assemble success";
    return r;
}

