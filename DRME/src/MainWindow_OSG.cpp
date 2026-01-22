#include "MainWindow_OSG.h"
#include "OCCTWidget.h"
#include "OCCBrepDataProcess.h"
#include <QMenu>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStringLiteral>
#include <AIS_ColoredShape.hxx>
#include <Standard_Type.hxx>
#include <QVariant>
#include "OCCModeling.h"
#include <AIS_Line.hxx>
#include <GC_MakeSegment.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include "AIS_ModelWithAxis.h"
#include <algorithm>
#include <Prs3d_Drawer.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include "PartLibraryDock.h"
#include <BRepBuilderAPI_Transform.hxx>
#include <fstream>
#include "IModelLoader.h"
#include <TopAbs_ShapeEnum.hxx>

// ========== 匿名命名空间：JsonModelLoader 实现 ==========
namespace {
class JsonModelLoader : public IModelLoader {
public:
    JsonModelLoader(OCCTWidget* w, QTreeWidget* tree) : m_widget(w), m_tree(tree) {}
    Handle(AIS_ModelWithAxis) Create(const std::string& name, PartType type, double mainRadius, const std::string& sourcePath) override {
        if (!m_widget || sourcePath.empty()) return Handle(AIS_ModelWithAxis)();
        TopoDS_Shape shape = ImportShape(sourcePath);
        if (shape.IsNull()) {
            qWarning() << "[JsonModelLoader] ImportShape 失败:" << QString::fromStdString(sourcePath);
            return Handle(AIS_ModelWithAxis)();
        }
        std::string axisFile = sourcePath + "_axes.json";
        Handle(AIS_ModelWithAxis) model = new AIS_ModelWithAxis(shape, axisFile);
        m_widget->m_models.push_back(model);
        if (m_widget->aManipulator.IsNull()) m_widget->aManipulator = new AIS_Manipulator();
        m_widget->aManipulator->Attach(model);
        auto ctx = m_widget->getInteractiveContext();
        if (!ctx.IsNull()) {
            ctx->Display(model, Standard_False);
            ctx->Activate(AIS_Shape::SelectionMode(TopAbs_SOLID), Standard_True);
        }
        m_widget->ais_shape = model.get();
        m_widget->aViewShape = shape;
        if (m_tree) {
            QTreeWidgetItem* root = m_tree->topLevelItem(0);
            if (!root) {
                root = new QTreeWidgetItem(m_tree);
                root->setText(0, QStringLiteral("模型"));
            }
            // 为空名称时回退为文件基名
            QString qName = QString::fromStdString(name);
            if (qName.trimmed().isEmpty()) {
                QFileInfo fi(QString::fromStdString(sourcePath));
                qName = fi.baseName();
            }
            QTreeWidgetItem* item = new QTreeWidgetItem(root);
            item->setText(0, qName);          // 第一列显示零件名称
            item->setText(1, QStringLiteral("已加载")); // 第二列显示状态
            item->setData(0, Qt::UserRole, QVariant::fromValue((void*)model.get()));
            // 额外：存储类型字符串以便需要时检索
            QString typeStr = (type == PartType::Rod) ? QStringLiteral("Rod") : (type == PartType::Slider) ? QStringLiteral("Slider") : (type == PartType::Screw) ? QStringLiteral("Screw") : QStringLiteral("Bone");
            item->setData(0, Qt::UserRole + 1, typeStr);
            root->addChild(item);
        }
        return model;
    }
private:
    OCCTWidget* m_widget = nullptr;
    QTreeWidget* m_tree = nullptr;
};

// ========== 辅助函数：零件类型转字符串 ==========
static QString PartTypeToString(PartType t)
{
    switch (t) {
    case PartType::Rod:    return QStringLiteral("Rod");
    case PartType::Slider: return QStringLiteral("Slider");
    case PartType::Screw:  return QStringLiteral("Screw");
    case PartType::Bone:   return QStringLiteral("Bone");
    default:               return QStringLiteral("Unknown");
    }
}

// ========== 辅助函数：从 PartGraph 重建建模树 ==========
static void RebuildFeatureTreeFromPartGraph(QTreeWidget* featureTree, const PartGraph& graph)
{
    if (!featureTree) return;

    featureTree->clear();

    // 顶层节点：装配体
    QTreeWidgetItem* root = new QTreeWidgetItem(featureTree);
    root->setText(0, QStringLiteral("装配体"));
    root->setText(1, QStringLiteral("Assembly"));
    featureTree->addTopLevelItem(root);

    // 遍历 PartGraph 所有零件
    const auto& parts = graph.GetParts();
    for (const auto& kv : parts) {
        const std::string& name = kv.first;
        const PartInfo& info = kv.second;

        QTreeWidgetItem* item = new QTreeWidgetItem(root);
        // 使用 JSON 里的 name 作为建模树显示名
        item->setText(0, QString::fromStdString(name)); 
        item->setText(1, PartTypeToString(info.type));

        // 把 AIS_ModelWithAxis* 挂到 item 上，后面选中树节点时可以反查模型
        if (!info.model.IsNull()) {
            item->setData(0, Qt::UserRole, QVariant::fromValue((void*)info.model.get()));
            item->setData(0, Qt::UserRole + 1, PartTypeToString(info.type));
        }
    }

    root->setExpanded(true);
}

} // namespace

Ui_MainWindow::Ui_MainWindow() { }

// 自定义模型加载器(FileModelLoader) 已移除，使用 JsonModelLoader

void Ui_MainWindow::show(QMainWindow* Form) { Form->show(); }

void Ui_MainWindow::ViewCascade()
{
 	 OCCSubWindow* pOCCWindow = (OCCSubWindow*)mdiArea->currentSubWindow();

 	 if (pOCCWindow != nullptr)
 	 {
 		 pOCCWindow->showMaximized();

 		 OCCTWidget* pOCCWidget = (OCCTWidget*)pOCCWindow->widget();

 		 if (pOCCWindow->bInitialize)
 		 {
 			 //pOCCWidget->get3dView()->FitAll();
 			 pOCCWidget->get3dView()->MustBeResized();
 		 }
 	 }
 }

 void Ui_MainWindow::ViewTiled()
 {
 	 mdiArea->tileSubWindows();

 	 QList<QMdiSubWindow *> pSubWindowList = mdiArea->subWindowList();

 	 for (QList<QMdiSubWindow*>::Iterator it = pSubWindowList.begin();it!=pSubWindowList.end();it++)
 	 {
 		 QMdiSubWindow* pMdiSubWindow = *it;

 		 if (pMdiSubWindow != nullptr)
 		 {
 			 OCCSubWindow* pOCCWindow = (OCCSubWindow*)pMdiSubWindow;

 			 OCCTWidget* pOCCWidget = (OCCTWidget*)pOCCWindow->widget();

 			 if (pOCCWindow->bInitialize)
 			 {
 				 //pOCCWidget->get3dView()->FitAll();
 				 pOCCWidget->get3dView()->MustBeResized();
 			 }
 		 }
 	 }
 }

 OCCSubWindow* Ui_MainWindow::AddSubWindow()
 {
 	 OCCSubWindow* subWindow = new OCCSubWindow();

 	 subWindow->setWidget(new QWidget());
 	 mdiArea->addSubWindow(subWindow);
 	 subWindow->setWindowTitle(QStringLiteral("窗口1"));
	
 	 return subWindow;
 }

 void Ui_MainWindow::SubWindowOperation()
 {
 	 OCCSubWindow* pOCCWindow = (OCCSubWindow*)mdiArea->currentSubWindow();

 	 if (pOCCWindow!=nullptr)
 	 {
 		 OCCTWidget* pOCCWidget = (OCCTWidget*)pOCCWindow->widget();

 		 if (pOCCWindow->bInitialize)
 		 {
 			 //pOCCWidget->get3dView()->FitAll();
 			 pOCCWidget->get3dView()->MustBeResized();
 		 }
 	 }
	
 	 printf("SubWindowOperation\n");
 }

 void Ui_MainWindow::NewDoc()
 {
 	 OCCSubWindow* subWindow = AddSubWindow();
 	 OCCTWidget* pOCCWidget = new OCCTWidget(subWindow);
 	 subWindow->setWidget(pOCCWidget);

 	 subWindow->bInitialize = true;
	 // 初始化灯光与全局显示（移除旧 getViewer 调用，使用 get3dViewer）
	 SetupViewerDisplay(pOCCWidget->get3dViewer(), pOCCWidget->getInteractiveContext());
 	 pOCCWidget->get3dView()->FitAll();
 	 pOCCWidget->get3dView()->MustBeResized();

 	 subWindow->show();
 }


 void Ui_MainWindow::FuelRoadDesign()
 {
 	 //构建圆柱体
 	 gp_Ax2 ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)); // 圆柱体的轴心和方向
 	 double radius = 1; // 半径
 	 double height = 20.0; // 高度
 	 BRepPrimAPI_MakeCylinder cylinder(ax2, radius, height);
 	 TopoDS_Shape cylinderShape = cylinder.Shape();

 	 // 创建一个空的复合体，用于存放阵列后的圆柱体
 	 TopoDS_Compound compoundMaker;
 	 BRep_Builder pBrepBuilder;
 	 pBrepBuilder.MakeCompound(compoundMaker);

 	 // 添加原始圆柱体到复合体中

 	 // 定义阵列参数
 	 int nx = 100; // X方向上的数量
 	 int ny = 100; // Y方向上的数量
 	 double dx = 3.0; // X方向上的间距
 	 double dy = 3.0; // Y方向上的间距

 	/* QWidget * pCurrentWidget = pQTabWidget->currentWidget();
	 
 	 if (pCurrentWidget == nullptr)
 	 {
 		 return;
 	 }*/

 	 OCCSubWindow* pOCCWindow = (OCCSubWindow*)mdiArea->currentSubWindow();

 	 if (pOCCWindow==nullptr)
 	 {
 		 printf("请先创建一个文档\n");
 		 return;
 	 }

 	 OCCTWidget* pOCCWidget = (OCCTWidget*)pOCCWindow->widget();

	// 创建阵列并添加到复合体中
 	 for (int i = 0; i < nx; ++i) {
 		 for (int j = 0; j < ny; ++j) {
 			 gp_Trsf trsf; // 创建变换对象
 			 trsf.SetTranslation(gp_Vec(i * dx, j * dy, 0)); // 设置变换矩阵为平移矩阵
 			 
 			 BRepBuilderAPI_Transform transformation(cylinder, trsf, false);

 			 pBrepBuilder.Add(compoundMaker, transformation.Shape());

 			 printf("*********************************%d\n",i*j);
 			 /*osg::Node* pPartNode = BuildShapeMesh(transformation.Shape());

 			 pOSGWidget->getRoot()->addChild(pPartNode);*/
 		 }
 	 }

 	 if (compoundMaker.IsNull())
 	 {
 		 return;
 	 }

 	 if (pOCCWidget->ais_shape != nullptr) {
 		 pOCCWidget->getInteractiveContext()->Erase(pOCCWidget->ais_shape, true);
 	 }

 	 pOCCWidget->ais_shape = new AIS_ColoredShape(compoundMaker);

 	 pOCCWidget->aViewShape = compoundMaker;
	 //

 	 pOCCWidget->getInteractiveContext()->Display(pOCCWidget->ais_shape, true);
	 // 统一应用显示属性
	 ApplyDisplayAttributes(pOCCWidget->getInteractiveContext(), Handle(AIS_InteractiveObject)(pOCCWidget->ais_shape), Standard_False);
	 pOCCWidget->getInteractiveContext()->Activate(AIS_Shape::SelectionMode(TopAbs_SOLID), true);

 	 pOCCWidget->get3dView()->FitAll();
 	 pOCCWidget->get3dView()->MustBeResized();

 	 OutputColorShape("E:\\OCC.stp", compoundMaker);


 }


 void Ui_MainWindow::openShape()
 {
 	 // 原过滤器：QString filename = QFileDialog::getOpenFileName(this, tr("Open Step File"), pPreFilePath, QStringLiteral("(*.stp)"));
 	 QString filename = QFileDialog::getOpenFileName(this, tr("Open Model"), pPreFilePath, QStringLiteral("STEP/STL (*.stp *.step *.stl)"));

 	 if (filename.isEmpty())  // 若文件名为空，则不执行操作
 	 {
 		 return;
 	 }
 	 // 新增：弹窗询问新建窗口还是添加到已有窗口
 	 QMessageBox::StandardButton btn = QMessageBox::question(this, QStringLiteral("导入模型"), QStringLiteral("请选择导入方式：\n是新建窗口还是添加到已有窗口？"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
 	 bool useNewWindow = (btn == QMessageBox::Yes);
 	 OCCTWidget* pOCCWidget = nullptr;
 	 OCCSubWindow* subWindow = nullptr;
 	 if (useNewWindow) {
 		 // 新建窗口
 		 subWindow = AddSubWindow();
 		 pOCCWidget = new OCCTWidget(subWindow);
         pOCCWidget->setMainWindow(this);
 		 pOCCWidget->setPartGraph(&partGraph);
 		 subWindow->setWidget(pOCCWidget);
 		 subWindow->bInitialize = true;
 		 subWindow->show();
 		 // 初始化全局显示（新窗口）
 		 SetupViewerDisplay(pOCCWidget->get3dViewer(), pOCCWidget->getInteractiveContext());
 	 } else {
 		 // 选择已有窗口
 		 QList<QMdiSubWindow*> subWindows = mdiArea->subWindowList();
 		 if (subWindows.isEmpty()) {
 			 QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先创建一个窗口！"));
 			 return;
 		 }
 		 QStringList windowNames;
 		 for (QMdiSubWindow* win : subWindows) {
 			 windowNames << win->windowTitle();
 		 }
 		 bool ok = false;
 		 QString selectedWin = QInputDialog::getItem(this, QStringLiteral("选择窗口"), QStringLiteral("请选择要添加模型的窗口："), windowNames, 0, false, &ok);
 		 if (!ok || selectedWin.isEmpty()) return;
 		 QMdiSubWindow* targetWin = nullptr;
 		 for (QMdiSubWindow* win : subWindows) {
 			 if (win->windowTitle() == selectedWin) {
 				 targetWin = win;
 				 
 			 }
 		 }
 		 if (!targetWin) return;
 		 pOCCWidget = (OCCTWidget*)targetWin->widget();
 	 }

 	 // ===  PartGraph 绑定 ===
 	 pOCCWidget->setPartGraph(&partGraph);

     OCCModeling::LoadModelToWidget(filename, pOCCWidget, featureTreeWidget, partGraph);
 	 
 }

 void Ui_MainWindow::mesh3d()
 {
 	 //查看那个窗口是活动的

 	 OCCSubWindow* pSubWindow = (OCCSubWindow*)mdiArea->currentSubWindow();

 	 //QWidget * pCurrentWidget = pQTabWidget->currentWidget();

 	 if (pSubWindow !=nullptr)
 	 {
 		 OCCTWidget* pPreOCCWidget = (OCCTWidget*)pSubWindow->widget();

 		 if (pPreOCCWidget->aViewShape.IsNull())
 		 {
 			 return;
 		 }

 		 Shape2Mesh3d(pPreOCCWidget->aViewShape);

 		 //创建新的TabWidget
 		 OCCSubWindow* subWindow = AddSubWindow();

 		 //设定文件名称
 		 OCCTWidget* pOCCWidget = new OCCTWidget(subWindow);

 		 subWindow->setWidget(pOCCWidget);

 		 subWindow->bInitialize = true;
		

 		 TopoDS_Shape aMeshShape = ImportStl("E:\\Gmsh.stl");

 		 pOCCWidget->ais_shape = new AIS_ColoredShape(aMeshShape);
 		 pOCCWidget->aViewShape = aMeshShape;
	 

 		 pOCCWidget->getInteractiveContext()->Display(pOCCWidget->ais_shape, true);
 		 ApplyDisplayAttributes(pOCCWidget->getInteractiveContext(), Handle(AIS_InteractiveObject)(pOCCWidget->ais_shape), Standard_False);
	 // === 新增：覆盖为着色并设置材质（保留原代码不删） ===
	 {
		 auto ctx = pOCCWidget->getInteractiveContext();
		 Handle(AIS_InteractiveObject) obj = Handle(AIS_InteractiveObject)(pOCCWidget->ais_shape);
		 if (!ctx.IsNull() && !obj.IsNull()) {
			 ctx->SetDisplayMode(obj, AIS_Shaded, Standard_False);
			 Handle(Prs3d_Drawer) drw = obj->Attributes();
			 if (!drw.IsNull()) {
				 drw->SetFaceBoundaryDraw(Standard_False);
				 if (!drw->ShadingAspect().IsNull()) drw->ShadingAspect()->SetMaterial(Graphic3d_NOM_PLASTIC);
			 }
			 ctx->Redisplay(obj, Standard_False);
		 }
	 }

 		 //
 		 subWindow->show();
 	 }

 }

 void Ui_MainWindow::Init(QMainWindow* MainWindow)
 {
 	 MainWindow->setCentralWidget(mdiArea);
 	 mdiArea->setViewMode(QMdiArea::TabbedView);
 	 mdiArea->setTabsMovable(true);
 	 mdiArea->setTabsClosable(true);

 	 MainWindow->resize(1200, 800);

 	 //特征控件
 	 FeatureItems = new QDockWidget(QStringLiteral("建模树"), MainWindow);
 	 QWidget* FeatureFunctionPanel = new QWidget();
 	 FeatureFunctionPanel->setFixedWidth(240);

 	 QVBoxLayout* FeatureWidgetLayout = new QVBoxLayout();
 	 //特征树控件
 	 featureTreeWidget = new QTreeWidget();
 	 featureTreeWidget->setColumnCount(2);
 	 QStringList Labels = { QStringLiteral("类型"),QStringLiteral("状态") };
 	 featureTreeWidget->setHeaderLabels(Labels);
 	 featureTreeWidget->setColumnWidth(0, 140);
 	 featureTreeWidget->setColumnWidth(1, 80);
 	 featureTreeWidget->setSelectionMode(QTreeWidget::ExtendedSelection);

 	 // 右键菜单支持
 	 featureTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
 	 QObject::connect(featureTreeWidget, &QTreeWidget::customContextMenuRequested, [this](const QPoint& pos) {
 		 QTreeWidgetItem* item = featureTreeWidget->itemAt(pos);
 		 if (!item) return;
 		 QTreeWidgetItem* root = featureTreeWidget->topLevelItem(0);

 		 QVariant ptrVar = item->data(0, Qt::UserRole);
 		 AIS_ColoredShape* aisShape = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
 		 if (!aisShape) return;

 		 QMenu menu;
 		 QAction* colorAct = menu.addAction(QStringLiteral("更改颜色"));
 		 QAction* alphaAct = menu.addAction(QStringLiteral("更改透明度"));


 		 QAction* replaceAct = nullptr;
 		 QAction* deleteAct = nullptr;
 		 if (item != root) {
 			 replaceAct = menu.addAction(QStringLiteral("更换零件"));
 			 deleteAct = menu.addAction(QStringLiteral("删除零件"));
 		 }


 		 // ===================== 新增：根据当前状态动态生成显示/隐藏轴线按钮 =====================
 		 Handle(AIS_ModelWithAxis) modelWithAxis = Handle(AIS_ModelWithAxis)::DownCast(Handle(AIS_InteractiveObject)(aisShape));

 		 QAction* toggleAxisAct = nullptr;
 		 if (!modelWithAxis.IsNull()) {
 			 if (modelWithAxis->IsAxesVisible()) {
 				 toggleAxisAct = menu.addAction(QStringLiteral("隐藏轴线"));
 			 }
 			 else {
 				 toggleAxisAct = menu.addAction(QStringLiteral("显示轴线"));
 			 }
 		 }

 		 QAction* selectedAct = menu.exec(featureTreeWidget->viewport()->mapToGlobal(pos));

 		 if (!selectedAct) return;

 		 if (selectedAct == colorAct && aisShape) {
 			 QColor color = QColorDialog::getColor(Qt::white, featureTreeWidget, QStringLiteral("选择颜色"));
 			 if (color.isValid()) {
 				 Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_RGB);
 				 aisShape->SetColor(occColor);
 				 if (mdiArea->currentSubWindow()) {
 					 OCCTWidget* pOCCWidget = (OCCTWidget*)mdiArea->currentSubWindow()->widget();
 					 pOCCWidget->getInteractiveContext()->Redisplay(Handle(AIS_ColoredShape)(aisShape), true);
 					 pOCCWidget->get3dView()->MustBeResized();
 				 }
 			 }
 		 }
 		 else if (selectedAct == alphaAct && aisShape) {
 			 bool ok = false;
 			 double alpha = QInputDialog::getDouble(featureTreeWidget, QStringLiteral("设置透明度"), QStringLiteral("透明度(0~1):"), 1.0, 0.0, 1.0, 2, &ok);
 			 if (ok) {
 				 aisShape->SetTransparency(1.0 - alpha);
 				 if (mdiArea->currentSubWindow()) {
 					 OCCTWidget* pOCCWidget = (OCCTWidget*)mdiArea->currentSubWindow()->widget();
 					 pOCCWidget->getInteractiveContext()->Redisplay(Handle(AIS_ColoredShape)(aisShape), true);
 					 pOCCWidget->get3dView()->MustBeResized();
 				 }
 			 }
 		 }

 		 else if (selectedAct == replaceAct) {
 			 // 原过滤器：QString stepFile = QFileDialog::getOpenFileName(this, QStringLiteral("选择新的STEP文件"), pPreFilePath, QStringLiteral("(*.stp *.step)"));
 			 QString stepFile = QFileDialog::getOpenFileName(this, QStringLiteral("选择新模型"), pPreFilePath, QStringLiteral("STEP/STL (*.stp *.step *.stl)"));
 			 if (!stepFile.isEmpty()) {
 				 replacePart(item, stepFile);
 			 }
 		 }
 		 else if (selectedAct == deleteAct) {
 			 QList<QTreeWidgetItem*> selectedItems = featureTreeWidget->selectedItems();
 			 QList<QTreeWidgetItem*> toDelete;
 			 for (QTreeWidgetItem* sel : selectedItems) {
 				 if (sel && sel != root) toDelete << sel;
 			 }
 			 if (toDelete.isEmpty() && item != root) {
 				 toDelete << item; // 仅右键对象
 			 }
 			 if (!toDelete.isEmpty()) {
 				 deleteParts(toDelete);
 			 }
 		 }
 		 else if (selectedAct == toggleAxisAct)
 		 {
 			 if (!modelWithAxis.IsNull())
 			 {
 				 bool isVisible = modelWithAxis->IsAxesVisible();
 				 modelWithAxis->SetAxesVisible(!isVisible);

 				 // 更新显示
 				 if (mdiArea->currentSubWindow()) {
 					 OCCTWidget* pOCCWidget = (OCCTWidget*)mdiArea->currentSubWindow()->widget();
 					 auto ctx = pOCCWidget->getInteractiveContext();
 					 ctx->Redisplay(modelWithAxis, true);
 					 pOCCWidget->get3dView()->MustBeResized();
 				 }
 			 }
 		 }

 	 });

 	 // 组装树与3D对象的对应：点击树节点高亮3D对象
 	 QObject::connect(featureTreeWidget, &QTreeWidget::itemClicked, [this](QTreeWidgetItem* item, int column) {
 		 QVariant ptrVar = item->data(0, Qt::UserRole);
 		 AIS_ColoredShape* aisShape = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
 		 if (!aisShape) return;
 		 if (mdiArea->currentSubWindow()) {
 			 OCCTWidget* pOCCWidget = (OCCTWidget*)mdiArea->currentSubWindow()->widget();
 			 pOCCWidget->getInteractiveContext()->SetSelected(Handle(AIS_ColoredShape)(aisShape), true);
 			 pOCCWidget->get3dView()->MustBeResized();
 		 }
 		 });

 	 QTreeWidgetItem* Root = new QTreeWidgetItem(featureTreeWidget);
 	 Root->setText(0, QStringLiteral("模型"));




 	 //
 	 FeatureWidgetLayout->addWidget(featureTreeWidget);
 	 FeatureFunctionPanel->setLayout(FeatureWidgetLayout);

 	 FeatureItems->setWidget(FeatureFunctionPanel);
 	 FeatureItems->setFloating(false);

 	 MainWindow->addDockWidget(Qt::LeftDockWidgetArea, FeatureItems);

// === 右侧零件库工具栏 ===
     QToolBar* partToolBar = addToolBar(tr("零件库"));
     partToolBar->setObjectName(QStringLiteral("PartLibraryToolBar"));
     partToolBar->setOrientation(Qt::Vertical);
     partToolBar->setIconSize(QSize(48, 48));
     partToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
     partToolBar->setMinimumWidth(90);
     partToolBar->setVisible(true);
     MainWindow->addToolBar(Qt::RightToolBarArea, partToolBar);

     // === 路径设置 ===
     QString baseDir = QStringLiteral("C:/Users/Administrator/Desktop/demo/DRME/stp/parts/");
     QString iconBasePath = QStringLiteral("C:/Users/Administrator/Desktop/demo/icons/");

     // 工具栏按钮列表
     struct PartButtonInfo {
         QString name;
         QString folder;
         QString icon;
     };
     QList<PartButtonInfo> parts = {
         {QStringLiteral("棒 (Rod)"), QStringLiteral("Rod"), QStringLiteral("rod.png")},
         {QStringLiteral("滑块 (Slider)"), QStringLiteral("Slider"), QStringLiteral("slider.png")},
         {QStringLiteral("螺钉 (Screw)"), QStringLiteral("Screw"), QStringLiteral("screw.png")}
     };

     for (const PartButtonInfo& info : parts)
     {
         // 创建主按钮
         QAction* partAction = new QAction(QIcon(iconBasePath + info.icon), info.name, this);
         partToolBar->addAction(partAction);

         // 为该按钮创建弹出菜单
         QMenu* menu = new QMenu(partToolBar);
         QString folderPath = baseDir + info.folder;

         QDir dir(folderPath);
         if (dir.exists()) {
             QStringList filters;
             filters << QStringLiteral("*.stp") << QStringLiteral("*.step") << QStringLiteral("*.stl");
             QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

             for (const QFileInfo& f : files) {
                 QAction* fileAct = new QAction(f.fileName(), menu);
                 QObject::connect(fileAct, &QAction::triggered, [this, f]() {
                     OCCSubWindow* subWin = (OCCSubWindow*)mdiArea->currentSubWindow();
                     if (!subWin) {
                         QMessageBox::warning(nullptr, QStringLiteral("提示"), QStringLiteral("请先创建一个窗口！"));
                         return;
                     }
                     OCCTWidget* pOCCWidget = (OCCTWidget*)subWin->widget();
                     if (!pOCCWidget) return;

                     OCCModeling::LoadModelToWidget(f.absoluteFilePath(), pOCCWidget, featureTreeWidget, partGraph);
                     qDebug().noquote() << QStringLiteral("[PartLibrary] ✅ 加载零件: %1").arg(f.absoluteFilePath());
                     });
                 menu->addAction(fileAct);
             }
         }
         else {
             QAction* emptyAct = new QAction(QStringLiteral("(目录不存在)"), menu);
             emptyAct->setEnabled(false);
             menu->addAction(emptyAct);
         }

         // 绑定点击事件 -> 弹出菜单
         QObject::connect(partAction, &QAction::triggered, [partToolBar, menu, partAction]() {
             QWidget* w = partToolBar->widgetForAction(partAction);
             if (!w) return;
             QPoint pos = w->mapToGlobal(QPoint(-menu->sizeHint().width(), w->height() / 2));  // 按钮左侧弹出
             menu->exec(pos);
             });
     }

 }

 // ================= Undo / Redo core =================
// 将一个操作压入撤销栈，同时清空重做栈；用于形成新的撤销断点
 void Ui_MainWindow::pushUndo(const PartAction& a)
 {
 	 undoStack.push_back(a);
 	 redoStack.clear();
 	 if (actionUndo) actionUndo->setEnabled(true);      // 新增记录 -> 可以撤销
 	 if (actionRedo) actionRedo->setEnabled(false);     // 由于 redo 栈被清空 -> 不能重做
 }

 // 执行撤销：
 // 1. 从 undo 栈弹出操作
 // 2. 根据操作类型恢复 UI 树节点与 AIS 形状显示
 // 3. 将该操作压入 redo 栈
 void Ui_MainWindow::doUndo()
 {
 	 if (undoStack.empty()) return; // 没有可撤销

 	 PartAction a = undoStack.back();
 	 undoStack.pop_back();

 	 QMdiSubWindow* subWin = mdiArea->currentSubWindow();
 	 if (!subWin) return; // 没有活动窗口

 	 OCCTWidget* occW = (OCCTWidget*)subWin->widget();
 	 if (!occW) return;   // 无效 widget

 	 auto ctx = occW->getInteractiveContext();
 	 auto view = occW->get3dView();

 	 if (a.type == PartAction::DeleteParts)
 	 {
 		 // 撤销删除：重新创建节点，恢复显示
 		 for (auto& info : a.deletedParts)
 		 {
 			 QTreeWidgetItem* item = new QTreeWidgetItem();
 			 item->setText(0, info.text0);              // 原名称
 			 item->setText(1, info.text1);              // 原状态
 			 item->setData(0, Qt::UserRole, QVariant::fromValue<void*>((void*)info.shape));

 			 if (info.parent && info.index >= 0)
 				 info.parent->insertChild(info.index, item); // 按原索引插回
 			 else if (featureTreeWidget->topLevelItemCount() > 0)
 				 featureTreeWidget->topLevelItem(0)->addChild(item); // 兜底：加入根下

 			 if (info.shape)
 				 ctx->Display(Handle(AIS_ColoredShape)(info.shape), Standard_False); // 恢复显示

 			 info.item = item; // 存储新节点指针以便 redo 时删除
 		 }
 		 ctx->UpdateCurrentViewer();
 		 view->FitAll();
 		 view->MustBeResized();
 	 }
 	 else if (a.type == PartAction::ReplacePart)
 	 {
 		 // 撤销替换：移除新形状，恢复旧形状与树节点文本
 		 if (a.replace.item)
 		 {
 			 if (a.replace.newShape)
 				 ctx->Erase(Handle(AIS_ColoredShape)(a.replace.newShape), Standard_False);
 			 if (a.replace.oldShape)
 				 ctx->Display(Handle(AIS_ColoredShape)(a.replace.oldShape), Standard_False);

 			 a.replace.item->setData(0, Qt::UserRole, QVariant::fromValue<void*>((void*)a.replace.oldShape));
 			 a.replace.item->setText(0, a.replace.oldName);
 			 a.replace.item->setText(1, a.replace.oldStatus);

 			 // 若当前选择的 shape 正是已替换的新 shape，需要回指旧 shape
 			 if (occW->ais_shape == a.replace.newShape)
 			 {
 				 occW->ais_shape = a.replace.oldShape;
 				 if (a.replace.oldShape)
 					 occW->aViewShape = a.replace.oldShape->Shape();
 			 }
 			 ctx->UpdateCurrentViewer();
 		    view->FitAll();
 		    view->MustBeResized();
 		 }
 	 }

 	 // 进入 redo 栈
 	 redoStack.push_back(a);
 	 if (actionRedo) actionRedo->setEnabled(true);
 	 if (actionUndo) actionUndo->setEnabled(!undoStack.empty());
 }

 // 执行重做：与撤销相反
 // 1. 从 redo 栈弹出
 // 2. 恢复撤销前的修改（再次删除或再次替换）
 // 3. 将操作压回 undo 栈
 void Ui_MainWindow::doRedo()
 {
     if (redoStack.empty()) return; // 无可重做

     PartAction a = redoStack.back();
     redoStack.pop_back();
     auto& areplace = a.replace; // 新增: 兼容旧代码中残留的 areplace 引用

     QMdiSubWindow* subWin = mdiArea->currentSubWindow();
     if (!subWin) return;

     OCCTWidget* occW = (OCCTWidget*)subWin->widget();
     if (!occW) return;

     auto ctx = occW->getInteractiveContext();
     auto view = occW->get3dView();

     if (a.type == PartAction::DeleteParts)
     {
         for (auto& info : a.deletedParts)
         {
             if (info.item)
             {
                 QTreeWidgetItem* parent = info.item->parent();
                 if (parent)
                     parent->removeChild(info.item);
                 if (info.shape)
                     ctx->Erase(Handle(AIS_ColoredShape)(info.shape), Standard_False);
                 delete info.item;
                 info.item = nullptr;
             }
         }
         ctx->UpdateCurrentViewer();
         view->FitAll();
         view->MustBeResized();
     }
     else if (a.type == PartAction::ReplacePart)
     {
         if (a.replace.item)
         {
             if (a.replace.oldShape)
                 ctx->Erase(Handle(AIS_ColoredShape)(a.replace.oldShape), Standard_False);
             if (a.replace.newShape)
             {
                 ctx->Display(Handle(AIS_ColoredShape)(a.replace.newShape), Standard_False);
                 ApplyDisplayAttributes(ctx, Handle(AIS_InteractiveObject)(a.replace.newShape), Standard_False);
             }
             a.replace.item->setData(0, Qt::UserRole, QVariant::fromValue<void*>((void*)a.replace.newShape));
             a.replace.item->setText(0, a.replace.newName);
             a.replace.item->setText(1, a.replace.newStatus);
             if (occW->ais_shape == a.replace.oldShape)
             {
                 occW->ais_shape = a.replace.newShape;
                 if (a.replace.newShape)
                     occW->aViewShape = a.replace.newShape->Shape();
             }
             ctx->UpdateCurrentViewer();
             view->FitAll();
             view->MustBeResized();
         }
     }

     undoStack.push_back(a);
     if (actionUndo) actionUndo->setEnabled(true);
     if (actionRedo) actionRedo->setEnabled(!redoStack.empty());
 }

 // ============== Part operations (replace & delete) ==============
 // 替换树节点对应的 STEP 模型：
 // 1. 读取新 STEP -> TopoDS_Shape
 // 2. 生成新的 AIS_ColoredShape 并替换显示
 // 3. 更新树节点文本与数据指针
 // 4. 记录 PartAction 以支持撤销 / 重做
 void Ui_MainWindow::replacePart(QTreeWidgetItem* item, const QString& stepFile)
 {
 	 if (!item) return;

 	 QVariant ptrVar = item->data(0, Qt::UserRole);
 	 AIS_ColoredShape* oldAis = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
 	 if (stepFile.isEmpty()) return; // 未选择文件

 	 // 原：TopoDS_Shape newShape = ImportStp(std::string(stepFile.toLocal8Bit().constData()));
 	 // 改为使用 std::string 扩展名解析
    std::string stepStd(stepFile.toLocal8Bit().constData());
    std::string ext2;
    size_t dotPos2 = stepStd.find_last_of('.');
    if (dotPos2 != std::string::npos) {
        ext2 = stepStd.substr(dotPos2 + 1);
        std::transform(ext2.begin(), ext2.end(), ext2.begin(), [](unsigned char c){ return std::tolower(c); });
    }
    TopoDS_Shape newShape;
    if (ext2 == "stl") {
        newShape = ImportStl(stepStd.c_str());
    } else {
        newShape = ImportStp(stepStd);
    }
 	 if (newShape.IsNull()) return;  // 读取失败

 	 QMdiSubWindow* subWin = mdiArea->currentSubWindow();
 	 if (!subWin) return;

 	 OCCTWidget* occW = (OCCTWidget*)subWin->widget();
 	 if (!occW) return;

 	 auto ctx = occW->getInteractiveContext();
 	 auto view = occW->get3dView();

 	 PartAction act; act.type = PartAction::ReplacePart;
 	 act.replace.item = item;
 	 act.replace.oldShape = oldAis;
 	 act.replace.oldName = item->text(0);
 	 act.replace.oldStatus = item->text(1);
 	 act.replace.newName = QFileInfo(stepFile).fileName();
 	 act.replace.newStatus = QStringLiteral("已更换");

 	 AIS_ColoredShape* newAis = new AIS_ColoredShape(newShape);
 	 act.replace.newShape = newAis;

 	 if (oldAis)
 		 ctx->Erase(Handle(AIS_ColoredShape)(oldAis), false);
 	 ctx->Display(Handle(AIS_ColoredShape)(newAis), true);
	 // 统一应用显示属性
	 ApplyDisplayAttributes(ctx, Handle(AIS_InteractiveObject)(newAis), Standard_False);

 	 item->setText(0, act.replace.newName);
 	 item->setText(1, act.replace.newStatus);
 	 item->setData(0, Qt::UserRole, QVariant::fromValue<void*>((void*)newAis));

 	 // 若当前预览 shape 正是旧的 shape，保持视图引用更新
 	 if (occW->ais_shape == oldAis)
 	 {
 		 occW->ais_shape = newAis;
 		 occW->aViewShape = newShape;
 	 }

 	 view->FitAll();
 	 view->MustBeResized();
 	 pushUndo(act); // 记录操作
 }

 // 批量删除节点：
 // 1. 收集选中项信息（父节点、原索引、文本、形状指针）
 // 2. 先在 AIS 场景中移除对应 shape
 // 3. 再从树结构移除节点
 // 4. 压入 undo 以便恢复
 void Ui_MainWindow::deleteParts(const QList<QTreeWidgetItem*>& items)
 {
 	 if (items.isEmpty()) return;

 	 QMdiSubWindow* subWin = mdiArea->currentSubWindow();
 	 if (!subWin) return;

 	 OCCTWidget* occW = (OCCTWidget*)subWin->widget();
 	 if (!occW) return;

 	 auto ctx = occW->getInteractiveContext();
 	 auto view = occW->get3dView();

 	 PartAction act; act.type = PartAction::DeleteParts;
 	 act.deletedParts.reserve(items.size());

 	 for (QTreeWidgetItem* item : items)
 	 {
 		 PartAction::DeletedPartInfo info;
 		 info.parent = item->parent();
 		 info.index = info.parent ? info.parent->indexOfChild(item) : -1; // 记录原位置
 		 info.text0 = item->text(0);
 		 info.text1 = item->text(1);

 		 QVariant ptrVar = item->data(0, Qt::UserRole);
 		 info.shape = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
 		 info.item = item;

 		 if (info.shape)
 		 {
 			 ctx->Erase(Handle(AIS_ColoredShape)(info.shape), false); // 隐藏/移除显示
 			 if (occW->ais_shape == info.shape)
 				 occW->ais_shape = nullptr; // 清空当前选择
 		 }
 		 act.deletedParts.push_back(info);
 	 }

 	 // 从树结构移除（第二步执行，避免迭代时结构破坏）
 	 for (auto& info : act.deletedParts)
 	 {
 		 if (info.item)
 		 {
 			 QTreeWidgetItem* parent = info.item->parent();
 			 if (parent)
 				 parent->removeChild(info.item);
 		 }
 	 }

 	 view->FitAll();
 	 view->MustBeResized();
 	 pushUndo(act); // 允许撤销删除
 }

 // ================= Save model (selected or all) =================
 // 保存模型：
 // 1. 若选中项存在且其父为根，保存选中项；否则保存根下所有一级子节点
 // 2. 导出前对每个 TopoDS_Shape 应用其 AIS 对象当前的 LocalTransformation
 // 3. 将多个 shape 合并为一个 Compound（如果多于 1 个）
 // 4. 调用 OutputColorShape 写入 STEP 文件
 void Ui_MainWindow::saveModel()
 {
 	 QMdiSubWindow* subWin = mdiArea->currentSubWindow();
 	 if (!subWin)
 	 {
 		 QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先打开或创建一个窗口"));
 		 return;
 	 }

 	 OCCTWidget* occW = (OCCTWidget*)subWin->widget();
 	 if (!occW) return;

 	 std::vector<TopoDS_Shape> shapes;

 	 if (featureTreeWidget)
 	 {
 		 QTreeWidgetItem* root = featureTreeWidget->topLevelItem(0);
 		 if (root)
 		 {
 			 QList<QTreeWidgetItem*> selected = featureTreeWidget->selectedItems();
 			 QList<QTreeWidgetItem*> src;

 			 // 收集：仅将父节点为 root 的选中项视为有效导出对象
 			 if (!selected.isEmpty())
 				 for (auto* it : selected)
 					 if (it->parent() == root) src << it;

 			 // 没有选中项 -> 导出全部一级子节点
 			 if (src.isEmpty())
 				 for (int i = 0; i < root->childCount(); ++i)
 					 src << root->child(i);

 			 // 提取 AIS_ColoredShape 指针中的 TopoDS_Shape, 并应用当前 LocalTransformation
 			 for (auto* it : src)
 			 {
 				 QVariant ptrVar = it->data(0, Qt::UserRole);
 				 AIS_ColoredShape* ais = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
 				 if (!ais) continue;

 					 TopoDS_Shape baseShape = ais->Shape();
 					 if (baseShape.IsNull()) continue;

 					 gp_Trsf loc = ais->LocalTransformation();
 					 // 判断是否为单位变换
 					 if (loc.Form() != gp_Identity)
 					 {
 						 BRepBuilderAPI_Transform tr(baseShape, loc, true); // 复制并应用变换
 						 shapes.push_back(tr.Shape()); // 已带装配姿态
 					 }
 					 else
 					 {
 						 shapes.push_back(baseShape); // 原始姿态
 					 }
 			 }
 		 }
 	 }

 	 if (shapes.empty())
 	 {
 		 QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("没有可保存的模型"));
 		 return;
 	 }

 	 QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("保存STEP"), pPreFilePath, QStringLiteral("(*.stp *.step)"));
 	 if (fileName.isEmpty()) return; // 取消

 	 pPreFilePath = QFileInfo(fileName).absolutePath();

 	 TopoDS_Shape outShape;
 	 if (shapes.size() == 1)
 		 outShape = shapes.front();
 	 else
 	 {
 		 // 多个 shape -> Compound 合并
 		 BRep_Builder builder;
 		 TopoDS_Compound comp;
 		 builder.MakeCompound(comp);
 		 for (auto& s : shapes) builder.Add(comp, s);
 		 outShape = comp;
 	 }

 	 if (outShape.IsNull())
 	 {
 		 QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("导出形状为空"));
 		 return;
 	 }

 	 OutputColorShape(fileName.toLocal8Bit().constData(), outShape);
 	 QMessageBox::information(this, QStringLiteral("完成"), QStringLiteral("保存成功"));
 }

 // 新增: 从 JSON 加载装配信息
 void Ui_MainWindow::loadAssemblyJson()
 {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开工程 / 装配 JSON"),
        pPreFilePath,
        QStringLiteral("JSON (*.json)")
    );
    if (fileName.isEmpty()) return;

    pPreFilePath = QFileInfo(fileName).absolutePath();

    // 1) 获取或创建窗口与 OCCTWidget
    OCCTWidget* pOCCWidget = nullptr; OCCSubWindow* subWindow = nullptr;
    if (!mdiArea->subWindowList().isEmpty()) {
        subWindow = (OCCSubWindow*)mdiArea->currentSubWindow();
        if (subWindow) pOCCWidget = qobject_cast<OCCTWidget*>(subWindow->widget());
    }
    if (!pOCCWidget) {
        subWindow = AddSubWindow();
        pOCCWidget = new OCCTWidget(subWindow);
        pOCCWidget->setMainWindow(this);
        pOCCWidget->setPartGraph(&partGraph);
        subWindow->setWidget(pOCCWidget);
        subWindow->bInitialize = true;
        subWindow->show();
        SetupViewerDisplay(pOCCWidget->get3dViewer(), pOCCWidget->getInteractiveContext());
    } else {
        pOCCWidget->setPartGraph(&partGraph);
    }

    // 2) 清空旧场景与建模树
    if (auto ctx = pOCCWidget->getInteractiveContext(); !ctx.IsNull()) ctx->RemoveAll(Standard_True);
    pOCCWidget->m_models.clear();

    // 3) 通过 JsonModelLoader 自动导入所有零件
    JsonModelLoader loader(pOCCWidget, featureTreeWidget);
    if (!partGraph.LoadFromJson(fileName.toLocal8Bit().constData(), &loader)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("加载装配信息失败"));
        return;
    }

    // 4) 重建建模树，把每个零件的名字同步到左侧树
    RebuildFeatureTreeFromPartGraph(featureTreeWidget, partGraph);

    partGraph.PrintSummary();

    // 5) 统一视图自适应
    if (auto view = pOCCWidget->get3dView(); !view.IsNull()) { view->FitAll(); view->MustBeResized(); }

    QMessageBox::information(this, QStringLiteral("完成"), QStringLiteral("工程已恢复（零件 + 装配）"));
 }

// 新增: 保存当前装配信息到 JSON
void Ui_MainWindow::saveAssemblyJson()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存装配信息 JSON"),
        pPreFilePath,
        QStringLiteral("JSON (*.json)")
    );

    if (fileName.isEmpty())
        return;

    // 更新最近路径
    pPreFilePath = QFileInfo(fileName).absolutePath();

    // 调用 PartGraph 序列化
    partGraph.SaveToJson(fileName.toLocal8Bit().constData());

    QMessageBox::information(
        this,
        QStringLiteral("完成"),
        QStringLiteral("装配信息已保存")
    );
}