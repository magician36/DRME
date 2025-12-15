#pragma once

#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include <AIS_Point.hxx>
#include <Geom_CartesianPoint.hxx>
#include <AIS_ColoredShape.hxx>
#include <vector>
#include <QWidget>

class AIS_ModelWithAxis; // Forward declaration

class BonePointTool
{
public:
    BonePointTool(const Handle(AIS_InteractiveContext)& context, const Handle(V3d_View)& view, QWidget* parentWidget);
    ~BonePointTool();

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // 处理鼠标点击事件，返回是否处理了该事件（即是否生成了骨点）
    bool HandleMousePress(int x, int y, AIS_ColoredShape* fallbackShape);
    
    // 更新所有骨点的位置（当骨头移动时调用）
    void UpdateMarkers();

private:
    struct BoneMarker
    {
        Handle(AIS_InteractiveObject) owner;   // 此红点附着的骨头（AIS_ColoredShape / AIS_Shape）
        Handle(Geom_CartesianPoint)   geom;    // 红点几何（坐标存这里）
        Handle(AIS_Point)             aisPoint;// 用来显示的小红点
        gp_Pnt                        localPnt;// 在 owner 局部坐标系中的点
        Handle(AIS_ModelWithAxis)     attachedModel; // 关联的模型（如滑块）
    };

    Handle(AIS_InteractiveContext) m_context;
    Handle(V3d_View) m_view;
    QWidget* m_parentWidget;
    bool m_enabled = false;
    std::vector<BoneMarker> m_markers;
};
