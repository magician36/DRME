#pragma once

#include <AIS_ColoredShape.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx> 
#include <vector>
#include <string>
#include <gp_Ax1.hxx>
#include <json.hpp>
#include "PartGraph.h"

enum class HoleType;

using json = nlohmann::json;

class AIS_ModelWithAxis : public AIS_ColoredShape
{
    DEFINE_STANDARD_RTTIEXT(AIS_ModelWithAxis, AIS_ColoredShape)

public:
    AIS_ModelWithAxis(const TopoDS_Shape& shape, const std::string& axisFile);


    // 切换轴线显示
    void SetAxesVisible(bool visible);
    bool IsAxesVisible() const { return m_axesVisible; }

    // 返回轴线数量
    int NbAxes() const { return static_cast<int>(m_axes.size()); }
    const std::vector<gp_Ax1>& GetAllAxes() const { return m_axes; }
    const std::vector<double>& GetAllRadii() const { return m_radii; }

    //轴线类型访问函数
    const std::vector<HoleType>& GetAllHoleTypes() const { return m_holeTypes; }
    void SetAllHoleTypes(const std::vector<HoleType>& types) { m_holeTypes = types; }

    // 获取对应圆柱面的引用（只读）
    const TopoDS_Face& GetCylFace(int i) const { return m_cylFaces.at(i); }
    const std::vector<TopoDS_Face>& GetAllCylFaces() const { return m_cylFaces; }

    //主轴选择
    gp_Ax1 MainAxis() const
    {
        if (!m_axes.empty())
            return m_axes.front();
        return gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }

protected:
    // 覆写绘制：先调用父类绘制模型，再叠加轴线
    void Compute(
        const Handle(PrsMgr_PresentationManager)& thePrsMgr,
        const Handle(Prs3d_Presentation)& thePresentation,
        const Standard_Integer theMode
    ) override;

private:
    void LoadAxesFromFile(const std::string& file);
    void ExtractCylinderAxesFromShape(const TopoDS_Shape& s);
    void SaveAxesToFile() const;

    // 仅重建圆柱面（当从 json 加载轴线时，faces 需要从几何再匹配一次）
    void RebuildCylFacesFromShape(const TopoDS_Shape& s);

private:
	bool m_axesVisible = false;         //轴线显示开关
    std::vector<gp_Ax1> m_axes;         //每条轴线
    std::vector<double> m_radii;        //轴线半径
    std::vector<HoleType> m_holeTypes; // 每条轴线的类型

    // 保存圆柱面（与轴线和半径一一对应）
    std::vector<TopoDS_Face> m_cylFaces;

    std::string m_axisFile;

};
