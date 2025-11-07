#include "PartOperations.h"
#include "MainWindow_OSG.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QColorDialog>
#include <QVariant>
#include <QDebug>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

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

    auto ctx  = occW->getInteractiveContext();
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

    QMdiSubWindow* subWin = mdiArea->currentSubWindow();
    if (!subWin) return;

    OCCTWidget* occW = (OCCTWidget*)subWin->widget();
    if (!occW) return;

    auto ctx  = occW->getInteractiveContext();
    auto view = occW->get3dView();

    if (a.type == PartAction::DeleteParts)
    {
        // 重做删除：把撤销时重新插入的节点再次移除
        for (auto& info : a.deletedParts)
        {
            if (info.item)
            {
                QTreeWidgetItem* parent = info.item->parent();
                if (parent)
                    parent->removeChild(info.item);
                if (info.shape)
                    ctx->Erase(Handle(AIS_ColoredShape)(info.shape), Standard_False);
                delete info.item;      // 释放临时插回的节点
                info.item = nullptr;   // 标记为空
            }
        }
        ctx->UpdateCurrentViewer();
        view->FitAll();
        view->MustBeResized();
    }
    else if (a.type == PartAction::ReplacePart)
    {
        // 重做替换：旧 -> 新
        if (a.replace.item)
        {
            if (a.replace.oldShape)
                ctx->Erase(Handle(AIS_ColoredShape)(a.replace.oldShape), Standard_False);
            if (a.replace.newShape)
                ctx->Display(Handle(AIS_ColoredShape)(a.replace.newShape), Standard_False);

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

    // 回到 undo 栈
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

    TopoDS_Shape newShape = ImportStp(std::string(stepFile.toLocal8Bit().constData()));
    if (newShape.IsNull()) return;  // 读取失败

    QMdiSubWindow* subWin = mdiArea->currentSubWindow();
    if (!subWin) return;

    OCCTWidget* occW = (OCCTWidget*)subWin->widget();
    if (!occW) return;

    auto ctx  = occW->getInteractiveContext();
    auto view = occW->get3dView();

    PartAction act; act.type = PartAction::ReplacePart;
    act.replace.item = item;
    act.replace.oldShape = oldAis;
    act.replace.oldName  = item->text(0);
    act.replace.oldStatus = item->text(1);
    act.replace.newName = QFileInfo(stepFile).fileName();
    act.replace.newStatus = QStringLiteral("已更换");

    AIS_ColoredShape* newAis = new AIS_ColoredShape(newShape);
    act.replace.newShape = newAis;

    if (oldAis)
        ctx->Erase(Handle(AIS_ColoredShape)(oldAis), false);
    ctx->Display(Handle(AIS_ColoredShape)(newAis), true);

    item->setText(0, act.replace.newName);
    item->setText(1, act.replace.newStatus);
    item->setData(0, Qt::UserRole, QVariant::fromValue<void*>((void*)newAis));

    // 若当前预览 shape 正是旧的 shape，保持视图引用更新
    if (occW->ais_shape == oldAis)
    {
        occW->ais_shape  = newAis;
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

    auto ctx  = occW->getInteractiveContext();
    auto view = occW->get3dView();

    PartAction act; act.type = PartAction::DeleteParts;
    act.deletedParts.reserve(items.size());

    for (QTreeWidgetItem* item : items)
    {
        PartAction::DeletedPartInfo info;
        info.parent = item->parent();
        info.index  = info.parent ? info.parent->indexOfChild(item) : -1; // 记录原位置
        info.text0  = item->text(0);
        info.text1  = item->text(1);

        QVariant ptrVar = item->data(0, Qt::UserRole);
        info.shape = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
        info.item  = item;

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
// 2. 将多个 shape 合并为一个 Compound（如果多于 1 个）
// 3. 调用 OutputColorShape 写入 STEP 文件
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

            // 提取 AIS_ColoredShape 指针中的 TopoDS_Shape
            for (auto* it : src)
            {
                QVariant ptrVar = it->data(0, Qt::UserRole);
                AIS_ColoredShape* ais = reinterpret_cast<AIS_ColoredShape*>(ptrVar.value<void*>());
                if (ais) shapes.push_back(ais->Shape());
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
