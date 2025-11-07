#pragma once
#include <vector>
#include <QString>
#include <QTreeWidgetItem>
class AIS_ColoredShape;

// 零件操作动作，供 Ui_MainWindow 撤销 / 重做使用
struct PartAction
{
    // 操作类型：删除多个零件 / 替换单个零件
    enum Type { DeleteParts, ReplacePart } type;

    // 删除操作中每个被删除零件的快照信息
    struct DeletedPartInfo
    {
        QTreeWidgetItem* parent = nullptr;  // 原父节点指针
        int               index  = -1;      // 在父节点中的原始序号
        QString           text0;            // 列0（名称/类型）文本
        QString           text1;            // 列1（状态）文本
        AIS_ColoredShape* shape  = nullptr; // 关联的 AIS 形状指针
        QTreeWidgetItem*  item   = nullptr; // 当前（或重建后）的树节点指针
    };

    // 替换操作的信息（仅单个节点）
    struct ReplaceInfo
    {
        QTreeWidgetItem*  item      = nullptr;  // 目标树节点
        AIS_ColoredShape* oldShape  = nullptr;  // 旧形状
        AIS_ColoredShape* newShape  = nullptr;  // 新形状
        QString           oldName;              // 旧名称
        QString           newName;              // 新名称
        QString           oldStatus;            // 旧状态文本
        QString           newStatus;            // 新状态文本
    };

    std::vector<DeletedPartInfo> deletedParts; // 当 type == DeleteParts 时使用
    ReplaceInfo                  replace;      // 当 type == ReplacePart 时使用
};
