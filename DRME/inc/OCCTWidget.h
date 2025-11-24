/* ====================================================
#   Copyright (C)2024 Zhang Royi All rights reserved.
#
#   Author        : Zhang Royi
#   File Name     : cmakewindow.h
#   Last Modified : 202-03-08 22:00
#   Describe      : Main Window
#
# ====================================================*/

#ifndef OCCTWIDGET_H
#define OCCTWIDGET_H

#pragma once
#include <QtCore/QObject>
#include <QtCore/QDebug>

#include <QtWidgets/QWidget>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>

#include <QtWidgets/QApplication>

#include <AIS_InteractiveContext.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include "TopTools_IndexedMapOfShape.hxx"

#include <QtWidgets/QOpenGLWidget>

#include "PartGraph.h"
#include "AssemblyDialog.h"

#include <AIS_ColoredShape.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Manipulator.hxx>
#ifdef _WIN32
#include <WNT_Window.hxx>
#else
#undef None
#include <Xw_Window.hxx>
#endif
class Ui_MainWindow;
class AIS_ModelWithAxis;

class PartGraph;

//三维显示窗口
class OCCTWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit OCCTWidget(QWidget *parent = nullptr);
    void setMainWindow(Ui_MainWindow* main) { m_mainWindow = main; }
    Ui_MainWindow* mainWindow() const { return m_mainWindow; }

    //  获取三维环境交互对象
    Handle(AIS_InteractiveContext) getInteractiveContext(){return m_InteractiveContext;}

    //  获取三维显示界面
    Handle(V3d_View)  get3dView(){return m_3dView;}

	//  获取三维显示界面
	Handle(V3d_Viewer)  get3dViewer() { return m_3dViewer; }

    // ====== 模型与轴线控制接口 ======
    void DisplayAxes(const TopoDS_Shape& shape, const std::string& jsonFile);
    void ShowAxes(bool visible);
    void ClearAxes();
    void HighlightAxis(int index, bool highlight);

    // 根据 AIS_ModelWithAxis 创建可拾取的轴线
    void CreateAxisLines(const Handle(AIS_ModelWithAxis)& model);
	//
	AIS_ColoredShape* ais_shape = nullptr;

	//
	TopoDS_Shape aViewShape;

	Handle(AIS_Manipulator) aManipulator =  new AIS_Manipulator();

    // 在类成员区添加
    bool m_usingManipulator = false;
    
    // 所有加载的模型对象
    std::vector<Handle(AIS_ModelWithAxis)> m_models;  

    bool m_isAssemblyMode = false;
    void setAssemblyMode(bool enable) { m_isAssemblyMode = enable; }
    bool isAssemblyMode() const { return m_isAssemblyMode; }

    // === 获取 PartGraph (只读使用） ===
    void setPartGraph(PartGraph* graph) { 
        m_partGraph = graph;
        qDebug() << "[OCCTWidget] setPartGraph called, ptr =" << graph;
    }
    PartGraph* getPartGraph() const { return m_partGraph; }

    // === 约束相关方法 ===
    void ApplyDOFProjectionForActive();
    void RememberAttachedModel(const Handle(AIS_ModelWithAxis)& model);

private:

    // 初始化交互环境
    void initializeInteractiveContext();


    // 交互式上下文能够管理一个或多个查看器(viewer)中的图形行为和交互式对象的选择
    Handle(AIS_InteractiveContext) m_InteractiveContext;

    // 定义查看器(viewer)类型对象上的服务
    Handle(V3d_Viewer) m_3dViewer;

    // 创建一个视图
    Handle(V3d_View) m_3dView;

    // 创建3d接口定义图形驱动程序
    Handle(Graphic3d_GraphicDriver) m_graphicDriver;

    PartGraph* m_partGraph = nullptr; // 仅保存指针，不负责生命周期
    
    // === 操纵器约束相关成员 ===
    Handle(AIS_ModelWithAxis) m_attachedModel;  // 当前被操纵的模型
    gp_Trsf m_prevL;                            // 上一帧的变换矩阵

protected:

    // 覆写绘图事件
    void paintEvent(QPaintEvent *);

    // 覆写窗口尺寸变化事件
    void resizeEvent(QResizeEvent *);

    // 覆写鼠标按键按下事件
    void mousePressEvent(QMouseEvent *event);

    // 覆写鼠标按键释放事件
    void mouseReleaseEvent(QMouseEvent *event);

    // 覆写鼠标移动事件
    void mouseMoveEvent(QMouseEvent *event);

    // 覆写鼠标滚轮事件
    void wheelEvent(QWheelEvent* event) override;
    void zoomViewByWheel(QWheelEvent* event);

    //拖拽事件
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
protected:

    // 三维场景转换模式
    enum CurrentAction3d
    {
        CurAction3d_Nothing,
        CurAction3d_DynamicPanning, //平移
        CurAction3d_DynamicZooming, //缩放
        CurAction3d_DynamicRotation //旋转
    };

private:
    Ui_MainWindow* m_mainWindow = nullptr;   // 存储主窗口指针
    Standard_Integer m_xValue;    // 记录鼠标平移坐标X
    Standard_Integer m_yValue;    // 记录鼠标平移坐标Y
    CurrentAction3d m_currentMode; // 三维场景转换模式
    // === 轴线选择逻辑支持 ===
    std::vector<Handle(AIS_InteractiveObject)> m_axisShapes;  // 存放每条轴线对象
    std::vector<int> m_selectedAxisIndices;                   // 存放被选中的轴线索引
private:
    // 轴线数据
    std::vector<gp_Ax1> m_axes;                   // 每条轴线的几何信息
    std::string m_axisJsonFile;                   // 记录保存的 JSON 文件路径

};

#endif // OCCTWIDGET_H
