#include "PartGraph.h"
#include "AIS_ModelWithAxis.h"
#include <fstream>
#include <iostream>
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <QDebug>



// === 自动推断不同零件类型的自由度 ===
DOFInfo PartGraph::InferDOF(PartType type)
{
    DOFInfo info;
    switch (type)
    {
    case PartType::Rod:
        // 棒：可平移、旋转（主轴方向）
        info.translational[2] = true;
        info.rotational[2] = true;
        break;
    case PartType::Slider:
        // 滑块：沿棒方向平移
        info.translational[2] = true;
        break;
    case PartType::Screw:
        // 螺钉：可旋转（安装时一般不自由移动）
        info.rotational[2] = true;
        break;
    case PartType::Bone:
        // 骨：保持自由度为空（示例，可根据需求调整）
        break;
    }
    return info;
}
/*
// === 添加零件 ===
void PartGraph::AddPart(const std::string& name, PartType type, const Handle(AIS_ModelWithAxis)& model, double mainRadius)
{
    PartInfo info;
    info.type = type;
    info.model = model;
    info.dof = InferDOF(type);
    info.mainRadius = mainRadius;

    // 从模型中读取所有轴线信息
    const auto& axes = model->GetAllAxes();
    const auto& radii = model->GetAllRadii();
    const auto& types = model->GetAllHoleTypes(); // 从 _axes.json 读取的孔类型

    for (size_t i = 0; i < axes.size(); ++i)
    {
        HoleType holeType = (i < types.size()) ? types[i] : HoleType::RodHole;
        double radius = (i < radii.size()) ? radii[i] : 0.0;
        info.holes.emplace_back(holeType, axes[i]);
        info.holeRadii.push_back(radius);
    }

    parts[name] = info;

    qDebug() << "[PartGraph] 已添加零件:" << QString::fromStdString(name)
             << "类型=" << (type == PartType::Rod ? "Rod" :
            type == PartType::Slider ? "Slider" : "Screw")
             << "主半径=" << mainRadius
             << "孔数=" << info.holes.size();
}
*/
// === 添加孔 ===
void PartGraph::AddPart(const std::string& name, PartType type, const Handle(AIS_ModelWithAxis)& model, double mainRadius)
{
    if (model.IsNull()) {
        qWarning() << "[PartGraph::AddPart] 模型句柄为空，无法添加零件:" << QString::fromStdString(name);
        return;
    }

    PartInfo info;
    info.type = type;
    info.model = model;
    info.dof = InferDOF(type);
    info.mainRadius = mainRadius;

    // === 自动从模型中读取轴线、半径、孔类型 ===
    const auto& axes = model->GetAllAxes();
    const auto& radii = model->GetAllRadii();
    const auto& holeTypes = model->GetAllHoleTypes(); // 从 _axes.json 读取

    size_t holeCount = axes.size();
    for (size_t i = 0; i < holeCount; ++i)
    {
        HoleType holeType = HoleType::RodHole;
        if (i < holeTypes.size()) {
            holeType = holeTypes[i];
        }

        double radius = (i < radii.size()) ? radii[i] : 0.0;
        gp_Ax1 axis = axes[i];

        info.holes.emplace_back(holeType, axis);
        info.holeRadii.push_back(radius);

        QString typeStr = (holeType == HoleType::RodHole)
            ? QStringLiteral("RodHole")
            : QStringLiteral("ScrewHole");

        qDebug().noquote()
            << QString("[PartGraph::AddPart] 零件:%1 添加孔 #%2 类型:%3 半径:%4")
            .arg(QString::fromStdString(name))
            .arg(i)
            .arg(typeStr)
            .arg(radius, 0, 'f', 3);
    }

    // === 登记到 PartGraph ===
    parts[name] = info;

    qDebug().noquote()
        << QString("[PartGraph] ✅ 已添加零件:%1 | 类型:%2 | 孔数:%3 | 主半径:%4")
        .arg(QString::fromStdString(name))
        .arg((type == PartType::Rod) ? "Rod"
            : (type == PartType::Slider) ? "Slider"
            : (type == PartType::Screw) ? "Screw" : "Bone")
        .arg(info.holes.size())
        .arg(mainRadius, 0, 'f', 3);
}



// === 输出零件信息 ===
void PartGraph::PrintSummary() const
{
    std::cout << "\n=== PartGraph Summary ===" << std::endl;
    for (const auto& pair : parts)
    {
        const std::string& name = pair.first;
        const PartInfo& info = pair.second;

        std::cout << "零件: " << name << " | 类型: ";
        switch (info.type)
        {
        case PartType::Rod: std::cout << "Rod"; break;
        case PartType::Slider: std::cout << "Slider"; break;
        case PartType::Screw: std::cout << "Screw"; break;
        case PartType::Bone: std::cout << "Bone"; break;
        }
        std::cout 
            << " | 主半径: " << info.mainRadius
            << " | 孔数: " << info.holes.size()
            << " | isLockedOnRod: " << (info.isLockedOnRod ? "true" : "false")
            << std::endl;

        if (!info.connectedParts.empty()) {
            std::cout << "  Connected parts: ";
            for (const auto& cp : info.connectedParts)
                std::cout << cp << " ";
            std::cout << std::endl;
        }
    }
    std::cout << "==========================" << std::endl;
}
// === 添加约束关系 ===
void PartGraph::AddConstraint(const std::string& partName, const std::string& targetName, HoleType type)
{
    auto it = parts.find(partName);
    if (it == parts.end()) return;

    it->second.constraint.targetPart = targetName;
    it->second.constraint.type = type;
    //防止重复添加连接信息
    auto& conns = it->second.connectedParts;
    if (std::find(conns.begin(), conns.end(), targetName) == conns.end())
        conns.push_back(targetName);

    std::cout << "[Constraint] " << partName << " 装配到 " << targetName
        << " (" << (type == HoleType::RodHole ? "RodHole" : "ScrewHole") << ")\n";
}

// === 添加装配约束（滑块→棒） ===
void PartGraph::AddMate(const MateConstraint& mate)
{
    mates.push_back(mate);
    
    qDebug().noquote() << QString("[PartGraph] 添加装配约束: %1(滑块) → %2(棒) | 孔索引=%3")
        .arg(QString::fromStdString(mate.sliderName))
        .arg(QString::fromStdString(mate.rodName))
        .arg(mate.holeIndex);
    
    // 标记棒已锁定在滑块上
    auto it = parts.find(mate.rodName);
    if (it != parts.end()) {
        it->second.isLockedOnRod = true;
    }
}

// === 保存为 JSON 文件 ===
void PartGraph::SaveToJson(const std::string& file) const
{
    json root;
    root["parts"] = json::array();

    for (const auto& pair : parts)
    {
        const std::string& name = pair.first;
        const PartInfo& info = pair.second;

        json partJson;
        // 类型
        switch (info.type)
        {
        case PartType::Rod: partJson["type"] = "Rod"; break;
        case PartType::Slider: partJson["type"] = "Slider"; break;
        case PartType::Screw: partJson["type"] = "Screw"; break;
        case PartType::Bone: partJson["type"] = "Bone"; break;
        }

        // 自由度
        partJson["dof"] = {
            {"translational", {info.dof.translational[0], info.dof.translational[1], info.dof.translational[2]}},
            {"rotational", {info.dof.rotational[0], info.dof.rotational[1], info.dof.rotational[2]}}
        };

        // 主半径
        partJson["mainRadius"] = info.mainRadius;

        // 孔列表
        partJson["holes"] = json::array();
        for (size_t i = 0; i < info.holes.size(); ++i)
        {
            HoleType hType = info.holes[i].first;
            const gp_Ax1& ax = info.holes[i].second;
            double r = (i < info.holeRadii.size() ? info.holeRadii[i] : 0.0);

            gp_Pnt pnt = ax.Location();
            gp_Dir dir = ax.Direction();

            json holeJson;
            holeJson["holeType"] = (hType == HoleType::RodHole ? "RodHole" : "ScrewHole");
            holeJson["radius"] = r; // 保存半径
            holeJson["px"] = pnt.X();
            holeJson["py"] = pnt.Y();
            holeJson["pz"] = pnt.Z();
            holeJson["dx"] = dir.X();
            holeJson["dy"] = dir.Y();
            holeJson["dz"] = dir.Z();

            partJson["holes"].push_back(holeJson);
        }

        // 约束关系
        std::string constraintTypeStr = "None";
        if (info.constraint.type == HoleType::RodHole)
            constraintTypeStr = "RodHole";
        else if (info.constraint.type == HoleType::ScrewHole)
            constraintTypeStr = "ScrewHole";

        partJson["constraint"] = {
            {"target", info.constraint.targetPart},
            {"type", constraintTypeStr}
        };

        // 状态
        partJson["isLockedOnRod"] = info.isLockedOnRod;
        partJson["connectedParts"] = info.connectedParts;

        json entry;
        entry["name"] = name;
        entry["data"] = partJson;

        root["parts"].push_back(entry);
    }

    std::ofstream ofs(file);
    ofs << root.dump(4);
    ofs.close();

    std::cout << "[PartGraph] 已保存到文件: " << file << std::endl;
}

// === 新增实现：查询接口 ===
const MateConstraint* PartGraph::FindRodMateForSlider(const std::string& sliderName) const
{
    for (const auto& m : mates) {
        if (m.sliderName == sliderName) {
            auto it = parts.find(m.rodName);
            if (it != parts.end() && it->second.type == PartType::Rod) {
                return &m; // 返回“滑块—棒”的那条 Mate
            }
        }
    }
    return nullptr;
}

std::vector<std::string> PartGraph::GetScrewsForSlider(const std::string& sliderName) const
{
    std::vector<std::string> out;
    for (const auto& m : mates) {
        if (m.sliderName == sliderName) {
            auto it = parts.find(m.rodName);
            if (it != parts.end() && it->second.type == PartType::Screw) {
                out.push_back(m.rodName); // 这里 rodName 复用为“被约束件”（螺钉名）
            }
        }
    }
    return out;
}
