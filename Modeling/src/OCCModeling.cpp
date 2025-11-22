#include "OCCModeling.h"
#include "OCCBrepDataProcess.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <QtWidgets/qinputdialog.h>

void OCCModeling::LoadModelToWidget(
    const QString& filePath,
    OCCTWidget* pOCCWidget,
    QTreeWidget* featureTree,
    PartGraph& partGraph)
{
    if (!pOCCWidget || filePath.isEmpty())
        return;

    QByteArray utf8File = filePath.toUtf8();
    std::string sFileName(utf8File.constData());

    TopoDS_Shape aPartShape = ImportShape(sFileName);
    if (aPartShape.IsNull()) {
        qWarning() << "导入的TopoDS_Shape为空！";
        return;
    }

    // === 创建带轴线的 AIS 模型 ===
    std::string axisFile = sFileName + "_axes.json";
    Handle(AIS_ModelWithAxis) model = new AIS_ModelWithAxis(aPartShape, axisFile);
    pOCCWidget->m_models.push_back(model);

    // === 绑定操控器 ===
    if (pOCCWidget->aManipulator.IsNull())
        pOCCWidget->aManipulator = new AIS_Manipulator();
    pOCCWidget->aManipulator->Attach(model);

    // === 显示模型 ===
    Handle(AIS_InteractiveContext) ctx = pOCCWidget->getInteractiveContext();
    if (!ctx.IsNull()) {
        ctx->Display(model, Standard_True);
        ctx->Activate(AIS_Shape::SelectionMode(TopAbs_SOLID), true);
    }

    Handle(V3d_View) view = pOCCWidget->get3dView();
    if (!view.IsNull()) {
        view->FitAll();
        view->MustBeResized();
    }

    pOCCWidget->ais_shape = model.get();
    pOCCWidget->aViewShape = aPartShape;

    // === 更新建模树 ===
    if (featureTree) {
        QTreeWidgetItem* root = featureTree->topLevelItem(0);
        if (root) {
            QTreeWidgetItem* importedItem = new QTreeWidgetItem(root);
            importedItem->setText(0, QFileInfo(filePath).fileName());
            importedItem->setText(1, QStringLiteral("已导入"));
            importedItem->setData(0, Qt::UserRole, QVariant::fromValue((void*)model.get()));
            root->addChild(importedItem);
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
    if (selectedType.contains(QStringLiteral("Slider")))
        partType = PartType::Slider;
    else if (selectedType.contains(QStringLiteral("Screw"))) 
        partType = PartType::Screw;

    // === 登记到 PartGraph ===
    QString partName = QFileInfo(filePath).baseName();
    QByteArray utf8Name = partName.toUtf8();
    std::string safeName(utf8Name.constData());

    const auto& radii = model->GetAllRadii();
    partGraph.AddPart(safeName, partType, model, (radii.empty() ? 0.0 : radii.front()), sFileName); // 传入 sourcePath
    partGraph.SaveToJson("C:/Users/Administrator/Desktop/stp/PartLibrary.json");
    partGraph.PrintSummary();

    qDebug() << "[OCCModeling] 模型加载完成并已记录 PartGraph。";
}
