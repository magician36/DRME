#include "PartGraph.h"
#include "AIS_ModelWithAxis.h"
#include "IModelLoader.h"
#include <fstream>
#include <iostream>
#include <QDebug>
#include <cctype>
#include <algorithm>
#include <AIS_InteractiveContext.hxx>

namespace {
    static std::string stripTrailingDigits(std::string s) {
        while (!s.empty() && std::isdigit(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    }
    static std::string makeUniqueName(const std::map<std::string, PartInfo>& parts, const std::string& desired) {
        if (!parts.count(desired)) return desired;
        std::string base = stripTrailingDigits(desired);
        if (base.empty()) base = desired;
        int k = 1;
        std::string cand;
        do { cand = base + std::to_string(k++); } while (parts.count(cand));
        return cand;
    }
}

DOFInfo PartGraph::InferDOF(PartType type)
{
    DOFInfo info;
    switch (type) {
    case PartType::Rod:
        info.translational[2] = true;
        info.rotational[2] = true;
        break;
    case PartType::Slider:
        info.translational[2] = true;
        break;
    case PartType::Screw:
        info.rotational[2] = true;
        break;
    case PartType::Bone:
    default:
        break;
    }
    return info;
}

void PartGraph::AddPart(const std::string& name, PartType type, const Handle(AIS_ModelWithAxis)& model, double mainRadius, const std::string& sourcePath)
{
    if (model.IsNull()) {
        qWarning() << "[PartGraph::AddPart] model is null:" << QString::fromStdString(name);
        return;
    }

    PartInfo info;
    info.type = type;
    info.model = model;
    info.dof = InferDOF(type);
    info.mainRadius = mainRadius;
    info.sourcePath = sourcePath;

    const auto& axes = model->GetAllAxes();
    const auto& radii = model->GetAllRadii();
    const auto& holeTypes = model->GetAllHoleTypes();
    for (size_t i = 0; i < axes.size(); ++i) {
        HoleType ht = (i < holeTypes.size() ? holeTypes[i] : HoleType::RodHole);
        double r = (i < radii.size() ? radii[i] : 0.0);
        info.holes.emplace_back(ht, axes[i]);
        info.holeRadii.push_back(r);
        qDebug().noquote() << QString("[PartGraph::AddPart] %1 hole#%2 type:%3 r=%4")
            .arg(QString::fromStdString(name)).arg((int)i)
            .arg(ht == HoleType::RodHole ? "RodHole" : "ScrewHole")
            .arg(r, 0, 'f', 3);
    }

    std::string key = makeUniqueName(parts, name);
    if (key != name) qDebug().noquote() << QString("[PartGraph::AddPart] rename %1 -> %2").arg(QString::fromStdString(name)).arg(QString::fromStdString(key));
    parts[key] = std::move(info);
    qDebug().noquote() << QString("[PartGraph] Added: %1 type:%2 holes:%3 mainRadius:%4 src=%5")
        .arg(QString::fromStdString(key))
        .arg(type == PartType::Rod ? "Rod" : type == PartType::Slider ? "Slider" : type == PartType::Screw ? "Screw" : "Bone")
        .arg((int)parts[key].holes.size())
        .arg(mainRadius, 0, 'f', 3)
        .arg(QString::fromStdString(sourcePath));
}

void PartGraph::PrintSummary() const
{
    std::cout << "\n=== PartGraph Summary ===\n";
    for (const auto& kv : parts) {
        const PartInfo& info = kv.second;
        std::cout << "Part:" << kv.first << " type:" << (info.type == PartType::Rod ? "Rod" : info.type == PartType::Slider ? "Slider" : info.type == PartType::Screw ? "Screw" : "Bone")
            << " mainRadius:" << info.mainRadius << " holes:" << info.holes.size() << " locked:" << (info.isLockedOnRod ? "true" : "false") << " src:" << info.sourcePath << "\n";
        if (!info.connectedParts.empty()) {
            std::cout << "  Connected:";
            for (const auto& c : info.connectedParts) std::cout << c << " ";
            std::cout << "\n";
        }
    }
    std::cout << "==========================\n";
}

void PartGraph::AddConstraint(const std::string& partName, const std::string& targetName, HoleType type)
{
    auto it = parts.find(partName);
    if (it == parts.end()) return;
    it->second.constraint.targetPart = targetName;
    it->second.constraint.type = type;
    auto& v = it->second.connectedParts;
    if (std::find(v.begin(), v.end(), targetName) == v.end()) v.push_back(targetName);
    std::cout << "[Constraint] " << partName << " -> " << targetName << " (" << (type == HoleType::RodHole ? "RodHole" : "ScrewHole") << ")\n";
}

void PartGraph::AddMate(const MateConstraint& mate)
{
    mates.push_back(mate);
    qDebug().noquote() << QString("[PartGraph] Mate %1 -> %2 hole=%3").arg(QString::fromStdString(mate.sliderName)).arg(QString::fromStdString(mate.rodName)).arg(mate.holeIndex);
    auto it = parts.find(mate.rodName);
    if (it != parts.end()) it->second.isLockedOnRod = true;
}

void PartGraph::SaveToJson(const std::string& file) const
{
    json root;
    root["parts"] = json::array();

    for (const auto& kv : parts) {
        const std::string& name = kv.first;
        const PartInfo& info = kv.second;
        json entry;
        entry["name"] = name;
        json data;
        data["type"] = (info.type == PartType::Rod ? "Rod" : info.type == PartType::Slider ? "Slider" : info.type == PartType::Screw ? "Screw" : "Bone");
        json dof;
        dof["translational"] = json::array({ info.dof.translational[0], info.dof.translational[1], info.dof.translational[2] });
        dof["rotational"] = json::array({ info.dof.rotational[0], info.dof.rotational[1], info.dof.rotational[2] });
        data["dof"] = dof;
        data["mainRadius"] = info.mainRadius;
        data["sourcePath"] = info.sourcePath;

        if (!info.model.IsNull()) {
            gp_Trsf tr = info.model->LocalTransformation();
            json tf = json::array();
            for (int r = 1; r <= 3; ++r)
                for (int c = 1; c <= 4; ++c)
                    tf.push_back(tr.Value(r, c));
            data["transform"] = tf;
        }

        json holes = json::array();
        for (size_t i = 0; i < info.holes.size(); ++i) {
            HoleType ht = info.holes[i].first;
            const gp_Ax1& ax = info.holes[i].second;
            double rad = (i < info.holeRadii.size() ? info.holeRadii[i] : 0.0);
            gp_Pnt p = ax.Location();
            gp_Dir d = ax.Direction();
            json h;
            h["holeType"] = (ht == HoleType::RodHole ? "RodHole" : "ScrewHole");
            h["radius"] = rad;
            h["px"] = p.X(); h["py"] = p.Y(); h["pz"] = p.Z();
            h["dx"] = d.X(); h["dy"] = d.Y(); h["dz"] = d.Z();
            holes.push_back(h);
        }
        data["holes"] = holes;

        std::string ctype = "None";
        if (info.constraint.type == HoleType::RodHole) ctype = "RodHole";
        else if (info.constraint.type == HoleType::ScrewHole) ctype = "ScrewHole";
        json c;
        c["target"] = info.constraint.targetPart;
        c["type"] = ctype;
        data["constraint"] = c;

        data["isLockedOnRod"] = info.isLockedOnRod;
        data["connectedParts"] = info.connectedParts;
        entry["data"] = data;
        root["parts"].push_back(entry);
    }

    // mates
    root["mates"] = json::array();
    for (const auto& m : mates) {
        json jm;
        jm["sliderName"] = m.sliderName;
        jm["rodName"] = m.rodName;
        jm["holeIndex"] = m.holeIndex;
        gp_Pnt p = m.rodAxis.Location();
        gp_Dir d = m.rodAxis.Direction();
        jm["px"] = p.X(); jm["py"] = p.Y(); jm["pz"] = p.Z();
        jm["dx"] = d.X(); jm["dy"] = d.Y(); jm["dz"] = d.Z();
        root["mates"].push_back(jm);
    }

    // assemblies
    root["assemblies"] = json::array();
    for (const auto& kv : assemblies) {
        json ja;
        ja["name"] = kv.first;
        ja["members"] = kv.second.members;
        json tf = json::array();
        const gp_Trsf& tr = kv.second.transform;
        for (int r = 1; r <= 3; ++r)
            for (int c = 1; c <= 4; ++c)
                tf.push_back(tr.Value(r, c));
        ja["transform"] = tf;
        root["assemblies"].push_back(ja);
    }

    std::ofstream ofs(file);
    if (ofs) ofs << root.dump(4);
    else qWarning() << "[PartGraph::SaveToJson] open failed" << QString::fromStdString(file);
}

bool PartGraph::LoadFromJson(const std::string& file)
{
    return LoadFromJson(file, nullptr);
}

bool PartGraph::LoadFromJson(const std::string& file, IModelLoader* loader)
{
    std::map<std::string, Handle(AIS_ModelWithAxis)> old;
    for (auto& kv : parts) if (!kv.second.model.IsNull()) old[kv.first] = kv.second.model;

    std::ifstream ifs(file);
    if (!ifs.is_open()) {
        std::cerr << "[PartGraph] open failed " << file << std::endl;
        return false;
    }

    json root;
    try { ifs >> root; }
    catch (const std::exception& e) { std::cerr << "[PartGraph] parse failed " << e.what() << std::endl; return false; }

    parts.clear();
    mates.clear();
    assemblies.clear();

    if (!root.contains("parts") || !root["parts"].is_array()) { std::cerr << "[PartGraph] missing parts" << std::endl; return false; }

    for (auto& entry : root["parts"]) {
        if (!entry.contains("name") || !entry.contains("data")) continue;
        std::string name = entry["name"].get<std::string>();
        auto data = entry["data"];
        PartInfo info;
        std::string ts = data.value("type", "Rod");
        if (ts == "Rod") info.type = PartType::Rod;
        else if (ts == "Slider") info.type = PartType::Slider;
        else if (ts == "Screw") info.type = PartType::Screw;
        else if (ts == "Bone") info.type = PartType::Bone;
        info.sourcePath = data.value("sourcePath", std::string());
        if (data.contains("dof")) {
            try {
                auto td = data["dof"]["translational"];
                auto rd = data["dof"]["rotational"];
                for (int i = 0; i < 3; ++i) { info.dof.translational[i] = td[i].get<bool>(); info.dof.rotational[i] = rd[i].get<bool>(); }
            } catch (...) {}
        }
        info.mainRadius = data.value("mainRadius", 0.0);
        if (data.contains("holes") && data["holes"].is_array()) {
            for (auto& h : data["holes"]) {
                try {
                    std::string hs = h.value("holeType", "RodHole");
                    HoleType ht = (hs == "ScrewHole" ? HoleType::ScrewHole : HoleType::RodHole);
                    double r = h.value("radius", 0.0);
                    gp_Pnt p(h.value("px", 0.0), h.value("py", 0.0), h.value("pz", 0.0));
                    gp_Dir d(h.value("dx", 0.0), h.value("dy", 0.0), h.value("dz", 1.0));
                    info.holes.emplace_back(ht, gp_Ax1(p, d));
                    info.holeRadii.push_back(r);
                } catch (...) {}
            }
        }

        if (data.contains("constraint")) {
            auto jc = data["constraint"];
            info.constraint.targetPart = jc.value("target", "");
            std::string ct = jc.value("type", "RodHole");
            info.constraint.type = (ct == "ScrewHole" ? HoleType::ScrewHole : HoleType::RodHole);
        }

        info.isLockedOnRod = data.value("isLockedOnRod", false);
        if (data.contains("connectedParts") && data["connectedParts"].is_array()) {
            try { info.connectedParts = data["connectedParts"].get<std::vector<std::string>>(); } catch (...) {}
        }

        auto itOld = old.find(name);
        if (itOld != old.end()) info.model = itOld->second;
        if (info.model.IsNull() && loader && !info.sourcePath.empty()) {
            try { Handle(AIS_ModelWithAxis) h = loader->Create(name, info.type, info.mainRadius, info.sourcePath); info.model = h; } catch (...) {}
        }

        parts[name] = std::move(info);
    }

    // Second pass: restore transforms
    for (auto& entry : root["parts"]) {
        std::string name = entry["name"].get<std::string>();
        auto data = entry["data"];
        if (!data.contains("transform")) continue;
        auto it = parts.find(name);
        if (it == parts.end()) continue;
        if (it->second.model.IsNull()) continue;
        auto tf = data["transform"];
        if (tf.is_array() && tf.size() == 12) {
            gp_Trsf tr;
            tr.SetValues(tf[0].get<double>(), tf[1].get<double>(), tf[2].get<double>(), tf[3].get<double>(),
                         tf[4].get<double>(), tf[5].get<double>(), tf[6].get<double>(), tf[7].get<double>(),
                         tf[8].get<double>(), tf[9].get<double>(), tf[10].get<double>(), tf[11].get<double>());
            it->second.model->SetLocalTransformation(tr);
        }
    }

    // mates
    if (root.contains("mates") && root["mates"].is_array()) {
        for (auto& jm : root["mates"]) {
            try {
                MateConstraint m;
                m.sliderName = jm.value("sliderName", std::string());
                m.rodName = jm.value("rodName", std::string());
                m.holeIndex = jm.value("holeIndex", -1);
                gp_Pnt p(jm.value("px", 0.0), jm.value("py", 0.0), jm.value("pz", 0.0));
                gp_Dir d(jm.value("dx", 0.0), jm.value("dy", 0.0), jm.value("dz", 1.0));
                m.rodAxis = gp_Ax1(p, d);
                mates.push_back(m);
            } catch (...) {}
        }
    }

    // assemblies
    if (root.contains("assemblies") && root["assemblies"].is_array()) {
        for (auto& ja : root["assemblies"]) {
            try {
                std::string name = ja.value("name", std::string());
                AssemblyInfo ainfo;
                if (ja.contains("members") && ja["members"].is_array()) {
                    ainfo.members = ja["members"].get<std::vector<std::string>>();
                }
                if (ja.contains("transform") && ja["transform"].is_array() && ja["transform"].size() == 12) {
                    auto tf = ja["transform"];
                    gp_Trsf tr;
                    tr.SetValues(tf[0].get<double>(), tf[1].get<double>(), tf[2].get<double>(), tf[3].get<double>(),
                                 tf[4].get<double>(), tf[5].get<double>(), tf[6].get<double>(), tf[7].get<double>(),
                                 tf[8].get<double>(), tf[9].get<double>(), tf[10].get<double>(), tf[11].get<double>());
                    ainfo.transform = tr;
                }
                if (!name.empty()) assemblies[name] = std::move(ainfo);
            } catch (...) {}
        }
    }

    for (auto& kv : parts) kv.second.isLockedOnRod = false;
    for (const auto& m : mates) { auto it = parts.find(m.rodName); if (it != parts.end()) it->second.isLockedOnRod = true; }

    std::cout << "[PartGraph] JSON load complete parts=" << parts.size() << " mates=" << mates.size() << " assemblies=" << assemblies.size() << "\n";
    return true;
}

const MateConstraint* PartGraph::FindRodMateForSlider(const std::string& sliderName) const
{
    for (const auto& m : mates) if (m.sliderName == sliderName) {
        auto it = parts.find(m.rodName);
        if (it != parts.end() && it->second.type == PartType::Rod) return &m;
    }
    return nullptr;
}

std::vector<std::string> PartGraph::GetScrewsForSlider(const std::string& sliderName) const
{
    std::vector<std::string> out;
    for (const auto& m : mates) if (m.sliderName == sliderName) {
        auto it = parts.find(m.rodName);
        if (it != parts.end() && it->second.type == PartType::Screw) out.push_back(m.rodName);
    }
    return out;
}

std::string PartGraph::FindPartByModel(const Handle(AIS_ModelWithAxis)& model) const
{
    if (model.IsNull()) return std::string();
    for (const auto& kv : parts) if (!kv.second.model.IsNull() && kv.second.model == model) return kv.first;
    return std::string();
}

void PartGraph::UpdatePartTransform(const std::string& partName, const gp_Trsf& localTrsf)
{
    auto it = parts.find(partName);
    if (it == parts.end()) return;
    if (it->second.model.IsNull()) return;
    it->second.model->SetLocalTransformation(localTrsf);
}

bool PartGraph::CreateAssembly(const std::string& name, const std::vector<std::string>& members)
{
    if (name.empty()) return false;
    if (assemblies.find(name) != assemblies.end()) return false;
    AssemblyInfo a;
    a.members = members;
    a.transform = gp_Trsf();
    assemblies[name] = std::move(a);
    qDebug() << "[PartGraph] Created assembly:" << QString::fromStdString(name) << "members=" << (int)members.size();
    return true;
}

bool PartGraph::RemoveAssembly(const std::string& name)
{
    auto it = assemblies.find(name);
    if (it == assemblies.end()) return false;
    assemblies.erase(it);
    qDebug() << "[PartGraph] Removed assembly:" << QString::fromStdString(name);
    return true;
}

bool PartGraph::AddPartToAssembly(const std::string& assemblyName, const std::string& partName)
{
    auto it = assemblies.find(assemblyName);
    if (it == assemblies.end()) return false;
    if (parts.find(partName) == parts.end()) return false;
    auto& members = it->second.members;
    if (std::find(members.begin(), members.end(), partName) != members.end()) return false;
    members.push_back(partName);
    qDebug() << "[PartGraph] Added" << QString::fromStdString(partName) << "to assembly" << QString::fromStdString(assemblyName);
    return true;
}

std::vector<std::string> PartGraph::GetAssemblyMembers(const std::string& name) const
{
    auto it = assemblies.find(name);
    if (it == assemblies.end()) return {};
    return it->second.members;
}

bool PartGraph::MoveAssembly(const std::string& name, const gp_Trsf& delta, const Handle(AIS_InteractiveContext)& ctx)
{
    auto it = assemblies.find(name);
    if (it == assemblies.end()) return false;

    AssemblyInfo& ainfo = it->second;
    // accumulate assembly transform
    gp_Trsf newAssemblyTr = ainfo.transform;
    newAssemblyTr.Multiply(delta);
    ainfo.transform = newAssemblyTr;

    // apply delta to each member: Lnew = delta * Lold
    for (const auto& pname : ainfo.members) {
        auto pit = parts.find(pname);
        if (pit == parts.end()) continue;
        PartInfo& pinfo = pit->second;
        if (pinfo.model.IsNull()) continue;
        gp_Trsf Lold = pinfo.model->LocalTransformation();
        gp_Trsf Lnew = delta;
        Lnew.Multiply(Lold);
        pinfo.model->SetLocalTransformation(Lnew);
        UpdatePartTransform(pname, Lnew);
        if (!ctx.IsNull()) ctx->Redisplay(pinfo.model, Standard_False);
    }

    if (!ctx.IsNull()) ctx->UpdateCurrentViewer();

    qDebug() << "[PartGraph] Moved assembly:" << QString::fromStdString(name);
    return true;
}

// Find which assembly contains given part (returns empty string if none)
std::string PartGraph::FindAssemblyForPart(const std::string& partName) const
{
    for (const auto& kv : assemblies) {
        const AssemblyInfo& a = kv.second;
        if (std::find(a.members.begin(), a.members.end(), partName) != a.members.end())
            return kv.first;
    }
    return std::string();
}
