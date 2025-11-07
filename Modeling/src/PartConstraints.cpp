#include "PartConstraints.h"
#include "PartAssembler.h"
#include "AIS_ModelWithAxis.h"
#include <AIS_InteractiveContext.hxx>
#include <QDebug>

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
    return assembleToSlider(rodName, sliderName, holeIndex, HoleType::RodHole);
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

CreateConstraintResult ConstraintManager::assembleToSlider(const std::string& movingPart,
                                                            const std::string& sliderName,
                                                            int holeIndex,
                                                            HoleType holeType)
{
    CreateConstraintResult r; if (!m_graph) { r.message = "PartGraph null"; return r; }
    const auto &parts = m_graph->GetParts(); // 仅访问公开接口
    auto itSlider = parts.find(sliderName); auto itMov = parts.find(movingPart);
    if (itSlider == parts.end() || itMov == parts.end()) { r.message = "slider or moving part not found"; return r; }
    const PartInfo &sliderInfo = itSlider->second; const PartInfo &movInfo = itMov->second;
    if (holeIndex < 0 || holeIndex >= (int)sliderInfo.holes.size()) { r.message = "hole index invalid"; return r; }
    if (sliderInfo.model.IsNull() || movInfo.model.IsNull()) { r.message = "model null"; return r; }

    PartAssembler assembler(m_graph, m_context);
    bool ok = assembler.AssembleParts(movingPart, sliderName, holeIndex);
    if (!ok) { r.message = "assemble failure"; return r; }

    // 世界坐标轴
    gp_Ax1 holeAxLocal = sliderInfo.holes[holeIndex].second;
    gp_Trsf Ls = sliderInfo.model->LocalTransformation();
    gp_Ax1 holeAxWorld( holeAxLocal.Location().Transformed(Ls), gp_Dir(holeAxLocal.Direction().Transformed(Ls)) );

    MateConstraint mc; mc.sliderName = sliderName; mc.rodName = movingPart; mc.rodAxis = holeAxWorld; mc.holeIndex = holeIndex; m_graph->AddMate(mc);
    m_graph->AddConstraint(sliderName, movingPart, holeType);

    r.success = true; r.message = "assemble success"; return r;
}
