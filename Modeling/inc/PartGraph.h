#pragma once
#include <string>
#include <vector>
#include <map>
#include <gp_Ax1.hxx>
#include <json.hpp>
#include <AIS_InteractiveContext.hxx>

class AIS_ModelWithAxis;
class PartAssembler;  // 前向声明

using json = nlohmann::json;

// === 零件类型 ===
enum class PartType
{
    Rod,      // 棒
    Slider,   // 滑块
    Screw,    // 螺钉
    Bone      // 骨 (新增类型)
};

// === 孔类型 ===
enum class HoleType
{
    RodHole,      // 棒型孔
    ScrewHole     // 螺钉孔
};

// === 自由度信息 ===
struct DOFInfo
{
    bool translational[3] = { false, false, false };  // 在 X,Y,Z 平移
    bool rotational[3] = { false, false, false };  // 绕 X,Y,Z 旋转
};

// === 约束信息 ===
struct ConstraintInfo
{
    std::string targetPart;   // 被约束到的零件（名称）
    HoleType type;            // 约束类型
};

// === 装配约束：滑块→棒的轴向约束 ===
struct MateConstraint
{
    std::string sliderName;   // 滑块名称
    std::string rodName;      // 棒/螺钉名称
    gp_Ax1 rodAxis;          // 约束轴（滑块的孔轴，世界坐标系）
    int holeIndex = -1;      // 使用的孔索引
};

// === 零件信息 ===
struct PartInfo
{
    PartType type = PartType::Rod;                      // 类型
    Handle(AIS_ModelWithAxis) model;                    // 模型
    std::vector<std::pair<HoleType, gp_Ax1>> holes;     // 所有孔
    std::vector<double> holeRadii;                      // 每个孔的半径（与 holes 一一对应）
    double mainRadius = 0.0;                            // 棒或螺钉的半径

    DOFInfo dof;                                        // 自由度信息
    ConstraintInfo constraint;                          // 当前约束信息

    bool isLockedOnRod = false;                         //  是否是否固定
    std::vector<std::string> connectedParts;            //  和谁装配
};

// === 零件图：管理所有零件 ===
class PartGraph
{
public:
    void AddPart(const std::string& name, PartType type, const Handle(AIS_ModelWithAxis)& model, double mainRadius);
    //void AddHole(const std::string& partName, HoleType holeType, const gp_Ax1& axis, double radius);

    void AddConstraint(const std::string& partName, const std::string& targetName, HoleType type);

    // === 装配约束管理 ===
    void AddMate(const MateConstraint& mate);
    const std::vector<MateConstraint>& GetMates() const { return mates; }

    void PrintSummary() const;
    void SaveToJson(const std::string& file) const;
    
    // 只读访问函数
    const std::map<std::string, PartInfo>& GetParts() const { return parts; }

    // === 新增：查询"滑块绑定的棒轴 & 螺钉列表" ===
    // 获取该滑块对应的"棒"装配（若有），用于得到棒轴（世界）
    const MateConstraint* FindRodMateForSlider(const std::string& sliderName) const;
    // 获取与该滑块装配的"螺钉"名称列表（用作刚体联动）
    std::vector<std::string> GetScrewsForSlider(const std::string& sliderName) const;
    
    // === 复制滑块（保留位姿 + Rod 约束） ===
    std::string DuplicateSliderWithRodMates(const std::string& sliderName,
        const Handle(AIS_InteractiveContext)& context);
    
    // 允许 PartAssembler 访问私有成员
    friend class PartAssembler;
    
private:
    DOFInfo InferDOF(PartType type);
    std::map<std::string, PartInfo> parts;
    std::vector<MateConstraint> mates;  // 所有装配约束
};
