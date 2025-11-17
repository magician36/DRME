#pragma once
#include "MainWindow_OSG.h"
#include "OCCTWidget.h"
#include "OCCModeling.h"
#include <QWheelEvent>
#include "AIS_ViewCube.hxx"
#include <AIS_InteractiveContext.hxx>
#include "TopoDS_Shape.hxx"
#include <V3d_View.hxx>
#include <QtCore/QtDebug>
#include <QMenu>
#include <QColorDialog>
#include <AIS_Shape.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <GC_MakeSegment.hxx>
#include <Quantity_Color.hxx>
#include "AIS_ModelWithAxis.h"
#include "AssemblyDialog.h"
#include <QMessageBox>
#include <gp_Quaternion.hxx> // 新增: 用于增量旋转轴角提取
#include <QDialog>
#include <QMimeData>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include "OCCBrepDataProcess.h"

OCCTWidget::OCCTWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    //配置QWidget
    setBackgroundRole( QPalette::NoRole );  //无背景
    setMouseTracking( true );   //开启鼠标位置追踪
	setAcceptDrops(true);   // 启用拖拽接收
	if (m_InteractiveContext.IsNull())
	{
		initializeInteractiveContext();
	}

	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NoSystemBackground);
	setFocusPolicy(Qt::StrongFocus);

}

void OCCTWidget::initializeInteractiveContext()
{
    //若交互式上下文为空，则创建对象
    if (m_InteractiveContext.IsNull())
    {
        //此对象提供与X server的连接，在Windows和Mac OS中不起作用
        Handle(Aspect_DisplayConnection) m_display_donnection = new Aspect_DisplayConnection();
        //创建OpenGl图形驱动
        if (m_graphicDriver.IsNull())
        {
            m_graphicDriver = new OpenGl_GraphicDriver(m_display_donnection);
        }
        //获取QWidget的窗口系统标识符
        WId window_handle = (WId) winId();

#ifdef _WIN32
        // 创建Windows NT 窗口
        Handle(WNT_Window) wind = new WNT_Window((Aspect_Handle) window_handle);
#else
        // 创建XLib window 窗口
        Handle(Xw_Window) wind = new Xw_Window(m_display_donnection, (Window) window_handle);
#endif

        //创建3D查看器
        m_3dViewer = new V3d_Viewer(m_graphicDriver);

        //创建视图
        m_3dView = m_3dViewer->CreateView();
        m_3dView->SetWindow(wind);

        //打开窗口
        if (!wind->IsMapped())
        {
            wind->Map();
        }
        m_InteractiveContext = new AIS_InteractiveContext(m_3dViewer);  //创建交互式上下文

        //配置查看器的光照
        m_3dViewer->SetDefaultLights();
        m_3dViewer->SetLightOn();

        //设置视图的背景颜色为蓝色
       //m_3dView->SetBackgroundColor(Quantity_TOC_RGB, 0.333, 0.494, 0.865);
        m_3dView->MustBeResized();

		//显示直角坐标系，可以配置在窗口显示位置、文字颜色、大小、样式
		m_3dView->TriedronDisplay(Aspect_TOTP_RIGHT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_ZBUFFER);

		/*m_3dView->SetBgGradientColors(Quantity_Color(206/255., 215/255., 222/255.,Quantity_TOC_RGB),
			Quantity_Color(128 / 255., 128 / 255., 128 / 255., Quantity_TOC_RGB));*/

		/*m_3dView->SetBgGradientColors(Quantity_Color(255 / 255., 255 / 255., 255 / 255., Quantity_TOC_RGB),
				Quantity_Color(255 / 255., 255 / 255., 255 / 255., Quantity_TOC_RGB));*/
		m_3dView->SetBgGradientColors(Quantity_NOC_BLUE, Quantity_NOC_WHITE, Aspect_GFM_VER);

		m_3dView->MustBeResized();


        //设置显示模式
		
        m_InteractiveContext->SetDisplayMode(AIS_Shaded, Standard_True);
		m_InteractiveContext->DefaultDrawer()->SetFaceBoundaryDraw(true);
		
		m_InteractiveContext->Activate(TopAbs_EDGE, Standard_True);
		m_InteractiveContext->SetPixelTolerance(10); // 提高拾取容差

    }
}

void OCCTWidget::CreateAxisLines(const Handle(AIS_ModelWithAxis)& model)
{
	m_axisShapes.clear();
	m_selectedAxisIndices.clear();

	const auto& axes = model->GetAllAxes();
	for (const auto& ax : axes)
	{
		gp_Pnt p1 = ax.Location();
		gp_Pnt p2 = p1.Translated(ax.Direction().XYZ() * 50.0);

		TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(p1, p2);
		Handle(AIS_Shape) axisShape = new AIS_Shape(edge);
		axisShape->SetColor(Quantity_NOC_RED);
		axisShape->SetWidth(2.0);

		m_InteractiveContext->Display(axisShape, Standard_False);
		m_axisShapes.push_back(axisShape);
	}
	m_3dView->Redraw();
}

void OCCTWidget::paintEvent(QPaintEvent *)
{
    m_3dView->Redraw();
}

void OCCTWidget::resizeEvent(QResizeEvent *)
{
    if (!m_3dView.IsNull())
    {
        m_3dView->MustBeResized();
    }
}

void OCCTWidget::mousePressEvent(QMouseEvent *event)
{

    if((event->buttons()&Qt::MidButton))
    {
		// 鼠标中键按下 → 记录当前位置，准备用于平移操作
        m_xValue=event->x();
        m_yValue=event->y();
    }
	else if ((event->buttons()&Qt::RightButton))
	{
		// 鼠标右键按下
		m_xValue = event->x();
		m_yValue = event->y();

		// 检测是否选中对象
		AIS_StatusOfPick t_pick_status = m_InteractiveContext->SelectDetected();																												
		QMenu* menu = new QMenu();

		if (t_pick_status == AIS_SOP_OneSelected)
		{
			// 初始化选中对象
			m_InteractiveContext->InitSelected();
			if (m_InteractiveContext->MoreSelected())
			{
				Handle(AIS_InteractiveObject) selectedObj = m_InteractiveContext->SelectedInteractive();
				ais_shape = dynamic_cast<AIS_ColoredShape*>(selectedObj.get());
			}

			// === 菜单项：操纵 / 取消操纵 ===
			if (aManipulator->IsAttached())
			{
				QAction* actDetach = new QAction(QStringLiteral("取消操纵"), menu);

				menu->addAction(actDetach);

				connect(actDetach, &QAction::triggered, this, [=]()
					{
						aManipulator->DeactivateCurrentMode();
						aManipulator->Detach();
						RememberAttachedModel(Handle(AIS_ModelWithAxis)());  // 清空
						qDebug() << "[Manipulator] 已解除绑定";
					});
			}
			else
			{	
				
				QAction* actManip = new QAction(QStringLiteral("操纵"), menu);
				menu->addAction(actManip);

				connect(actManip, &QAction::triggered, this, [=]()
				{
					// 获取被选中的 AIS_ModelWithAxis
					Handle(AIS_ModelWithAxis) attachedModel;
					m_InteractiveContext->InitSelected();
					if (m_InteractiveContext->MoreSelected()) {
						Handle(AIS_InteractiveObject) obj = m_InteractiveContext->SelectedInteractive();
						attachedModel = Handle(AIS_ModelWithAxis)::DownCast(obj);
					}

					// 基础操纵器设置
					aManipulator->SetPart(0, AIS_ManipulatorMode::AIS_MM_Scaling, Standard_False);
					aManipulator->SetPart(1, AIS_ManipulatorMode::AIS_MM_Rotation, Standard_False);

					aManipulator->Attach(ais_shape);
					
					// 先只启用平移，后面根据约束再决定是否启用旋转
					aManipulator->EnableMode(AIS_ManipulatorMode::AIS_MM_Translation);
					aManipulator->SetModeActivationOnDetection(Standard_True);

					// === 记住被操纵的模型 ===
					if (!attachedModel.IsNull()) {
						RememberAttachedModel(attachedModel);

                        // === 新增：对螺钉进行操纵器裁剪（仅保留沿约束轴的一个平移手柄，禁用旋转） ===
                        std::string selfName; PartType pty = PartType::Rod;
                        const auto& parts = m_partGraph->GetParts();
                        for (const auto& kv : parts) {
                            if (!kv.second.model.IsNull() && kv.second.model == attachedModel) {
                                selfName = kv.first; pty = kv.second.type; break;
                            }
                        }
                        auto chooseAxisIndex = [&](const Handle(AIS_ModelWithAxis)& mdl, const gp_Dir& allowAxisW)->int {
                            gp_Trsf Linv = mdl->LocalTransformation(); Linv.Invert();
                            gp_Dir uL( gp_Dir(allowAxisW.Transformed(Linv)) );
                            double dx = fabs(uL.Dot(gp_Dir(1,0,0)));
                            double dy = fabs(uL.Dot(gp_Dir(0,1,0)));
                            double dz = fabs(uL.Dot(gp_Dir(0,0,1)));
                            if (dz >= dx && dz >= dy) return 2; // Z
                            if (dy >= dx && dy >= dz) return 1; // Y
                            return 0; // X
                        };
                        if (pty == PartType::Screw) {
                            gp_Dir allowU(0,0,1); bool ok=false;
                            for (const auto& m : m_partGraph->GetMates()) {
                                if (m.rodName == selfName) { allowU = m.rodAxis.Direction(); ok=true; break; }
                            }
                            if (ok) {
                                // Rotation mode is not enabled; just ensure only translation axis kept
                                aManipulator->EnableMode(AIS_MM_Translation);
                                for (int i=0;i<3;++i) aManipulator->SetPart(i, AIS_MM_Translation, Standard_False);
                                int keep = chooseAxisIndex(attachedModel, allowU);
                                aManipulator->SetPart(keep, AIS_MM_Translation, Standard_True);
                            }
                        }

						// === 根据 Mate 裁剪"棒"的操纵器 ===
						auto chooseAxisIndex2 = [&](const Handle(AIS_ModelWithAxis)& mdl, const gp_Dir& allowAxisW)->int {
							gp_Trsf Linv = mdl->LocalTransformation(); 
							Linv.Invert();
							gp_Dir uL(gp_Dir(allowAxisW.Transformed(Linv)));
							double dx = fabs(uL.Dot(gp_Dir(1,0,0)));
							double dy = fabs(uL.Dot(gp_Dir(0,1,0)));
							double dz = fabs(uL.Dot(gp_Dir(0,0,1)));
							if (dz >= dx && dz >= dy) return 2;
							if (dy >= dx && dy >= dz) return 1;
							return 0;
						};

						if (pty == PartType::Rod || pty == PartType::Screw) {
							// 读取 Mate 的"滑块孔轴(世界)"
							gp_Dir allowU(0,0,1); 
							bool ok=false;
							for (const auto& m : m_partGraph->GetMates()) {
								if (m.rodName == selfName) { 
									allowU = m.rodAxis.Direction(); 
									ok=true; 
									break;
								}
							}
							if (ok) {
								// 禁用旋转：不启用旋转模式即可
								aManipulator->EnableMode(AIS_MM_Translation);  // 只启用平移
								for (int i=0;i<3;++i) 
									aManipulator->SetPart(i, AIS_MM_Translation, Standard_False);
								int keep = chooseAxisIndex2(attachedModel, allowU);
								aManipulator->SetPart(keep, AIS_MM_Translation, Standard_True);
								
								qDebug() << "[操纵器] 已约束棒" << QString::fromStdString(selfName) 
									 << "只允许沿轴" << keep << "平移";
							}
						}
					}

					qDebug() << "[Manipulator] 已激活";
				});
			}
			// === 菜单项：装配 ===
			QAction* actAssemble = new QAction(QStringLiteral("装配…"), menu);
			menu->addAction(actAssemble);

			connect(actAssemble, &QAction::triggered, this, [=]()
			{
					qDebug() << "=== 打开装配对话框 ===";
					qDebug() << "m_partGraph pointer =" << m_partGraph;
					qDebug() << "m_InteractiveContext isNull =" << m_InteractiveContext.IsNull();
					qDebug() << "m_3dView isNull =" << m_3dView.IsNull();
					if (!m_3dView.IsNull())
						qDebug() << "m_3dView->Window().IsNull =" << m_3dView->Window().IsNull();

					if (!m_partGraph)
					{
						QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("PartGraph 未初始化"));
						return;
					}

					// 弹出装配对话框（非模态，可移动）
					AssemblyDialog* dlg = new AssemblyDialog(
						m_partGraph,
						m_InteractiveContext,
						m_3dView,
						this);
					dlg->show();
					qDebug() << "[右键] 打开装配对话框";
			});

            // ===== 新增：直接在 3D 窗口右键修改选中零件颜色与透明度（保留原树功能，不修改原代码） =====
            if (ais_shape) {
                QAction* actColor = new QAction(QStringLiteral("更改颜色"), menu);
                QAction* actAlpha = new QAction(QStringLiteral("更改透明度"), menu);
                menu->addAction(actColor);
                menu->addAction(actAlpha);
                connect(actColor, &QAction::triggered, this, [=]() {
                    QColorDialog dlg(this);
                    dlg.setOption(QColorDialog::ShowAlphaChannel, false);
                    if (dlg.exec() == QDialog::Accepted) {
                        QColor color = dlg.selectedColor();
                        if (!color.isValid()) return;
                        Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_RGB);
                        ais_shape->SetColor(occColor);
                        if (!m_InteractiveContext.IsNull()) {
                            m_InteractiveContext->Redisplay(Handle(AIS_ColoredShape)(ais_shape), Standard_True);
                            if (!m_3dView.IsNull()) m_3dView->MustBeResized();
                        }
                    }
                });
                connect(actAlpha, &QAction::triggered, this, [=]() {
                    QDialog dlg(this);
                    dlg.setWindowTitle(QStringLiteral("设置透明度"));
                    QVBoxLayout* lay = new QVBoxLayout(&dlg);
                    QDoubleSpinBox* spin = new QDoubleSpinBox(&dlg);
                    spin->setRange(0.0, 1.0);
                    spin->setDecimals(2);
                    spin->setValue(1.0 - ais_shape->Transparency()); // 当前透明度的反映射
                    lay->addWidget(spin);
                    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                    lay->addWidget(box);
                    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                    if (dlg.exec() == QDialog::Accepted) {
                        double alpha = spin->value();
                        ais_shape->SetTransparency(1.0 - alpha); // OCC 透明度是反的
                        if (!m_InteractiveContext.IsNull()) {
                            m_InteractiveContext->Redisplay(Handle(AIS_ColoredShape)(ais_shape), Standard_True);
                            if (!m_3dView.IsNull()) m_3dView->MustBeResized();
                        }
                    }
                });
            }
		}

		// 弹出菜单
		if (!menu->isEmpty())
			menu->exec(QCursor::pos());

		delete menu;
	}
	else if (event->buttons() & Qt::LeftButton)
	{
		// 记录位置
		m_xValue = event->x();
		m_yValue = event->y();

		// 把鼠标位置传给 interactive context，确保检测是基于当前坐标的
		m_InteractiveContext->MoveTo(event->pos().x(), event->pos().y(), m_3dView, Standard_True);

		// 检查是不是点到操纵器（DetectedInteractive() 返回被检测到的 AIS 对象）
		if (!aManipulator.IsNull()
			&& m_InteractiveContext->HasDetected()
			&& m_InteractiveContext->DetectedInteractive() == aManipulator)
		{
			// 点击到操纵器 → 开始变换（注意 StartTransform 的签名你项目中用的是 (x,y,view)）
			aManipulator->StartTransform(event->pos().x(), event->pos().y(), m_3dView);

			// 标记：本次拖拽会话是操纵器
			m_usingManipulator = true;
			return;
		}

		// 没点到操纵器 → 进入视角旋转
		m_3dView->StartRotation(event->x(), event->y());

		// 保持你的选择逻辑
		AIS_StatusOfPick t_pick_status = AIS_SOP_NothingSelected;
		if (qApp->keyboardModifiers() == Qt::ControlModifier)
			t_pick_status = m_InteractiveContext->SelectDetected(AIS_SelectionScheme_Add);
		else
			t_pick_status = m_InteractiveContext->SelectDetected();

		if (t_pick_status == AIS_SOP_OneSelected)
		{
			m_InteractiveContext->InitSelected();
			while (m_InteractiveContext->MoreSelected())
			{
				if (m_InteractiveContext->HasSelectedShape())
				{
					// 你需要的处理
				}
				m_InteractiveContext->NextSelected();
			}
		}
	}
}

void OCCTWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (m_usingManipulator && !aManipulator.IsNull())
		{
			// 停止操纵器会话
			aManipulator->StopTransform(Standard_True);
			qDebug() << "[StopTransform] HasActive=" << aManipulator->HasActiveMode();   // 👈 插在这里
			m_usingManipulator = false;
			m_3dView->Redraw();
		}
		else
		{
			// 不是操纵器会话 → 视角结束或重绘
			m_3dView->Redraw();
		}
	}

	QOpenGLWidget::mouseReleaseEvent(event);
}

void OCCTWidget::mouseMoveEvent(QMouseEvent *event)
{

    if ((event->buttons() & Qt::MidButton) || (event->buttons() & Qt::RightButton))
    {
        // 中键按下：执行平移
        m_3dView->Pan(event->pos().x()-m_xValue,m_yValue-event->pos().y());
        m_xValue=event->x();
        m_yValue=event->y();
    }

	else if (event->buttons() & Qt::LeftButton)
	{
		// 左键按下
		// 如果当前会话是操纵器（通过按下时决定），就走操纵器逻辑
		if (m_usingManipulator && !aManipulator.IsNull())
		{
			// 使用你当前 OCCT 版本的 Transform 签名
			aManipulator->Transform(event->pos().x(), event->pos().y(), m_3dView);
			
			// === 应用自由度约束投影 ===
			ApplyDOFProjectionForActive();
			
			m_3dView->Redraw();
			qDebug() << "[Transform] Manipulator active, moving...";
			return;
		}

		// 执行旋转（视角）
		m_3dView->Rotation(event->x(), event->y());
	}

    else
    {
		// 没有鼠标按键 → 普通 hover 检测
        m_InteractiveContext->MoveTo(event->pos().x(),event->pos().y(),m_3dView,Standard_True);
		bool hasDetected = m_InteractiveContext->HasDetected();
    }

	
}

// 滚轮事件：只做事件捕获与转发
void OCCTWidget::wheelEvent(QWheelEvent* event)
{
	if (m_3dView.IsNull())
		return;

	zoomViewByWheel(event);
	QOpenGLWidget::wheelEvent(event);  // 保留 Qt 默认处理
}

void OCCTWidget::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasText() || event->mimeData()->hasUrls())
	{
		event->acceptProposedAction();
		qDebug() << "[DragEnter] Accepted drag event";
	}
	else {
		qDebug() << "[DragEnter] Rejected drag event";
	}
}

void OCCTWidget::dropEvent(QDropEvent* event)
{
	QString filePath;

	// 支持两种来源（文件系统 or 零件库）
	if (event->mimeData()->hasUrls())
		filePath = event->mimeData()->urls().first().toLocalFile();
	else
		filePath = event->mimeData()->text();

	qDebug() << "[DropEvent] File path:" << filePath;

	if (filePath.isEmpty())
		return;

	// === 找到主窗口 ===
	if (!m_mainWindow) {
		qDebug() << "[DropEvent] Main window pointer not set!";
		return;
	}

	qDebug() << "[DropEvent] Found main window via member pointer:" << m_mainWindow;
	qDebug() << "[DropEvent] Calling LoadModelToWidget...";

	// === 调用统一的加载逻辑 ===
	OCCModeling::LoadModelToWidget(filePath, this, m_mainWindow->featureTreeWidget, *this->getPartGraph());

}
// 平滑缩放核心函数
void OCCTWidget::zoomViewByWheel(QWheelEvent* event)
{
	int delta = event->angleDelta().y();
	QPoint mousePos = event->pos();

	// 平滑缩放系数
	double zoomFactor = 1.0 + (delta / 1200.0);
	if (zoomFactor < 0.1) zoomFactor = 0.1;

	int xCenter = mousePos.x();
	int yCenter = mousePos.y();

	// 拖拽距离决定缩放速度
	int offset = static_cast<int>(80 * std::abs(zoomFactor - 1.0));

	int x1 = xCenter;
	int y1, x2 = xCenter, y2 = yCenter;

	if (delta > 0)
		y1 = y2 - offset; // 上滑放大
	else
		y1 = y2 + offset; // 下滑缩小

	m_3dView->StartZoomAtPoint(xCenter, yCenter);
	m_3dView->ZoomAtPoint(x1, y1, x2, y2);

	m_3dView->Redraw();
}

// 显示模型与轴线（自动加载或提取）
void OCCTWidget::DisplayAxes(const TopoDS_Shape& shape, const std::string& jsonFile)
{
	if (shape.IsNull() || m_InteractiveContext.IsNull()) return;

	Handle(AIS_ModelWithAxis) model = new AIS_ModelWithAxis(shape, jsonFile);
	m_InteractiveContext->Display(model, Standard_True);
	m_models.push_back(model);

	qDebug() << "[DisplayAxes] 显示模型及其轴线，共" << model->NbAxes() << "条。";
}

// 控制所有模型的轴线显示或隐藏
void OCCTWidget::ShowAxes(bool visible)
{
	if (m_InteractiveContext.IsNull()) return;

	for (auto& model : m_models)
	{
		model->SetAxesVisible(visible);
		m_InteractiveContext->Redisplay(model, Standard_False);
	}

	m_3dView->Redraw();
	qDebug() << "[ShowAxes] 所有轴线" << (visible ? "已显示" : "已隐藏");
}

// 清除所有模型
void OCCTWidget::ClearAxes()
{
	if (m_InteractiveContext.IsNull()) return;

	for (auto& model : m_models)
	{
		m_InteractiveContext->Remove(model, Standard_False);
	}
	m_models.clear();

	m_3dView->Redraw();
	qDebug() << "[ClearAxes] 已清除所有模型与轴线。";
}

// 高亮或恢复轴线颜色
void OCCTWidget::HighlightAxis(int index, bool highlight)
{
	if (index < 0 || index >= (int)m_axisShapes.size())
		return;

	Handle(AIS_Shape) shape = Handle(AIS_Shape)::DownCast(m_axisShapes[index]);
	if (shape.IsNull())
		return;

	shape->SetColor(highlight ? Quantity_NOC_YELLOW : Quantity_NOC_RED);
	m_InteractiveContext->Redisplay(shape, Standard_False);
}

// === 应用自由度投影：限制棒/螺钉轴向；滑块可沿并绕棒孔轴（整体+螺钉） ===
void OCCTWidget::ApplyDOFProjectionForActive()
{
    if (m_attachedModel.IsNull() || !m_partGraph) return;

    // 反查当前选中对象名称与类型
    std::string selfName; PartType selfType = PartType::Rod; const PartInfo* pSelf = nullptr;
    const auto& parts = m_partGraph->GetParts();
    for (const auto& kv : parts) {
        if (!kv.second.model.IsNull() && kv.second.model == m_attachedModel) {
            selfName = kv.first; pSelf = &kv.second; selfType = kv.second.type; break;
        }
    }
    if (!pSelf) return;

    // 增量变换 dL = prev^-1 * now
    gp_Trsf Lnow = m_attachedModel->LocalTransformation();
    gp_Trsf dL = m_prevL; dL.Invert(); dL.Multiply(Lnow);

	// 新增：当与该滑块关联的棒 Mate 数 >=2 时，仅允许轴向平移（禁旋）
	if (selfType == PartType::Slider) {
		int rodMateCount = 0; gp_Dir u(0, 0, 1); gp_Pnt axisOrigin;
		for (const auto& m : m_partGraph->GetMates()) {
			auto itRod = parts.find(m.rodName);
			if (m.sliderName == selfName && itRod != parts.end() && itRod->second.type == PartType::Rod) {
				if (rodMateCount == 0) { u = m.rodAxis.Direction(); axisOrigin = m.rodAxis.Location(); }
				++rodMateCount;
			}
		}
		if (rodMateCount == 0) { m_prevL = Lnow; return; }

		// 投影平移到轴向
		gp_Vec t = dL.TranslationPart();
		Standard_Real s = t.Dot(u.XYZ());
		gp_Vec tProj = gp_Vec(u.XYZ()) * s;

		gp_Trsf dAllowed; dAllowed.SetTranslationPart(tProj);

		if (rodMateCount < 2) {
			// 单棒：保留原绕轴旋转约束（投影旋转到允许轴）
			gp_Mat R = dL.VectorialPart();
			gp_Quaternion qR; qR.SetMatrix(R);
			Standard_Real vx = qR.X(), vy = qR.Y(), vz = qR.Z(), w = qR.W();
			Standard_Real normv = sqrt(vx * vx + vy * vy + vz * vz);
			Standard_Real angAllowed = 0.0;
			if (normv > 1e-12) {
				gp_Dir axisQ(gp_Vec(vx, vy, vz));
				Standard_Real ang = 2.0 * atan2(normv, w);
				Standard_Real k = axisQ.Dot(u); // 同向正，反向负
				angAllowed = ang * k;
			}
			if (Abs(angAllowed) > 1e-12) {
				gp_Trsf dRot; dRot.SetRotation(gp_Ax1(axisOrigin, u), angAllowed);
				dAllowed.Multiply(dRot);
			}
		} // rodMateCount >=2 时不合成旋转

        // 应用到滑块
        gp_Trsf Lfix = m_prevL; Lfix.Multiply(dAllowed);
        m_attachedModel->SetLocalTransformation(Lfix);
        if (!m_InteractiveContext.IsNull())
            m_InteractiveContext->Redisplay(m_attachedModel, Standard_False);

        // 同步所有与该滑块装配的螺钉（同刚体增量）
        const auto screwNames = m_partGraph->GetScrewsForSlider(selfName);
        for (const auto& scName : screwNames) {
            auto it = parts.find(scName);
            if (it == parts.end() || it->second.model.IsNull()) continue;
            gp_Trsf Ls = it->second.model->LocalTransformation();
            Ls.Multiply(dAllowed);
            it->second.model->SetLocalTransformation(Ls);
            if (!m_InteractiveContext.IsNull())
                m_InteractiveContext->Redisplay(it->second.model, Standard_False);
        }

        m_prevL = Lfix;
        return;
    }

    // ===== 棒 / 螺钉：仅轴向平移（不旋转） =====
    if (selfType == PartType::Rod || selfType == PartType::Screw) {
        gp_Dir u(0,0,1); bool found=false;
        for (const auto& m : m_partGraph->GetMates()) {
            if (m.rodName == selfName) { u = m.rodAxis.Direction(); found = true; break; }
        }
        if (!found) { m_prevL = Lnow; return; }
        gp_Vec t = dL.TranslationPart();
        Standard_Real s = t.Dot(u.XYZ());
        gp_Vec tProj = gp_Vec(u.XYZ()) * s;
        gp_Trsf dAllowed; dAllowed.SetTranslationPart(tProj);
        gp_Trsf Lfix = m_prevL; Lfix.Multiply(dAllowed);
        m_attachedModel->SetLocalTransformation(Lfix);
        if (!m_InteractiveContext.IsNull())
            m_InteractiveContext->Redisplay(m_attachedModel, Standard_False);
        m_prevL = Lfix;
        return;
    }

    // 其他类型：暂不处理
    m_prevL = Lnow;
}

// === 记住被操纵的模型（用于约束计算） ===
void OCCTWidget::RememberAttachedModel(const Handle(AIS_ModelWithAxis)& model)
{
    m_attachedModel = model;
    if (!model.IsNull()) {
        m_prevL = model->LocalTransformation();
    } else {
        m_prevL = gp_Trsf();  // 默认构造为单位矩阵
    }
}

