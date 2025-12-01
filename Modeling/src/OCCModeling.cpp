#include "OCCModeling.h"
#include "OCCBrepDataProcess.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <QtWidgets/qinputdialog.h>

#include "BasicFunction.h"              // 这里面有 ImportStp 和 LoadStlLightweight

void OCCModeling::LoadModelToWidget(
    const QString& filePath,
    OCCTWidget* pOCCWidget,
    QTreeWidget* featureTree,
    PartGraph& partGraph)
{
    if (!pOCCWidget || filePath.isEmpty())
        return;

    // 统一路径处理
    const std::string sFileName = filePath.toUtf8().constData();
    const QString ext = QFileInfo(filePath).suffix().toLower();

    const bool isStl = (ext == "stl");
    const bool isStep = (ext == "stp" || ext == "step");

    Handle(AIS_InteractiveContext) ctx = pOCCWidget->getInteractiveContext();
    if (ctx.IsNull())
        return;


    // =====================================================
    //                 STL: 轻量显示模式
    // =====================================================
    if (isStl)
    { 
        // 最终显示的模型对象
        Handle(AIS_InteractiveObject) modelIO =
            LoadStlLightweight(sFileName, ctx);
     
        if (modelIO.IsNull()) {
            qWarning() << QStringLiteral("[OCCModeling] STL 加载失败");
            return;
        }

        // --- 记录到 widget 模型列表 ---
        pOCCWidget->m_models.push_back(modelIO);

		// --- 强制刷新视图几何 ---
        ctx->UpdateCurrentViewer();

        // --- 视图更新 ---
        Handle(V3d_View) view = pOCCWidget->get3dView();
        if (!view.IsNull()) {
            view->FitAll();
            view->Redraw();
        }

        // --- 更新建模树 ---
        if (featureTree) {
            QTreeWidgetItem* root = featureTree->topLevelItem(0);
            if (root) {
                QTreeWidgetItem* item = new QTreeWidgetItem(root);
                item->setText(0, QFileInfo(filePath).fileName());
                item->setText(1, QStringLiteral("骨头 (参考)"));
                item->setData(0, Qt::UserRole, QVariant::fromValue((void*)modelIO.get()));
                item->setData(0, Qt::UserRole + 1, QStringLiteral("Bone"));
                root->addChild(item);
            }
        }

        qDebug() << QStringLiteral("[OCCModeling] STL 显示完毕.");
        return; // STL 完整结束，不进入 PartGraph
    }

    // =====================================================
    //            STEP：真实几何 + 轴线提取 + 装配
    // =====================================================
    if (isStep)
    {
        TopoDS_Shape shape = ImportStp(sFileName);
        if (shape.IsNull()) {
            qWarning() << "导入的 STEP Shape 为空！";
            return;
        }

        std::string axisFile = sFileName + "_axes.json";
        Handle(AIS_ModelWithAxis) model = new AIS_ModelWithAxis(shape, axisFile);

        Handle(AIS_InteractiveObject) modelIO = model; // 统一模型对象

        pOCCWidget->m_models.push_back(model);

        // --- 绑定操控器 ---
        if (pOCCWidget->aManipulator.IsNull())
            pOCCWidget->aManipulator = new AIS_Manipulator();
        pOCCWidget->aManipulator->Attach(model);

        // --- 显示 ---
        ctx->Display(model, Standard_True);
        ctx->Activate(AIS_Shape::SelectionMode(TopAbs_SOLID), true);

        // --- 视图更新 ---
        Handle(V3d_View) view = pOCCWidget->get3dView();
        if (!view.IsNull()) {
            view->FitAll();
            view->MustBeResized();
        }

        // --- 更新建模树 ---
        if (featureTree) {
            QTreeWidgetItem* root = featureTree->topLevelItem(0);
            if (root) {
                QTreeWidgetItem* item = new QTreeWidgetItem(root);
                item->setText(0, QFileInfo(filePath).fileName());
                item->setText(1, QStringLiteral("已导入"));
                item->setData(0, Qt::UserRole, QVariant::fromValue((void*)model.get()));
                item->setData(0, Qt::UserRole + 1, QStringLiteral("Part"));
                root->addChild(item);
            }
        }

        // === 询问零件类型 ===
        QStringList types = { QStringLiteral("Rod"), QStringLiteral("Slider"), QStringLiteral("Screw") };
        bool ok = false;
        QString selectedType = QInputDialog::getItem(
            pOCCWidget,
            QStringLiteral("零件类型"),
            QStringLiteral("请选择该零件的类型："),
            types, 0, false, &ok);
        if (!ok) return;

        PartType partType = PartType::Rod;
        if (selectedType == QStringLiteral("Slider")) partType = PartType::Slider;
        else if (selectedType == QStringLiteral("Screw")) partType = PartType::Screw;

        // === 登记到 PartGraph ===
        QString partName = QFileInfo(filePath).baseName();
        std::string safeName = partName.toUtf8().constData();

        auto radii = model->GetAllRadii();
        double mainR = (radii.empty() ? 0.0 : radii.front());

        partGraph.AddPart(safeName, partType, model, mainR, sFileName);
        partGraph.SaveToJson("C:/Users/Administrator/Desktop/stp/PartLibrary.json");
        partGraph.PrintSummary();

        qDebug() << "[OCCModeling] STEP 模型加载完成并已记录 PartGraph。";
        return;
    }

    // =====================================================
    // 不支持的格式
    // =====================================================
    QMessageBox::warning(nullptr, "格式不支持",
        "仅支持 STEP (*.stp, *.step) 和 STL (*.stl) 文件！");
}
