#pragma once
#include <QString>
#include "AIS_ModelWithAxis.h"
#include "OCCTWidget.h"
#include "PartGraph.h"
#include <QtWidgets/QTreeWidget>

// 高层模型加载接口
class OCCModeling
{
public:
    static void LoadModelToWidget(
        const QString& filePath,
        OCCTWidget* pOCCWidget,
        QTreeWidget* featureTree,
        PartGraph& partGraph);
};
