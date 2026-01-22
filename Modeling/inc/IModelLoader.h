#pragma once
#include <string>
#include <AIS_ModelWithAxis.h>
#include "PartGraph.h"

// 模型加载接口：根据 sourcePath 重建 AIS_ModelWithAxis
class IModelLoader {
public:
    virtual ~IModelLoader() = default;
    // 新接口: Create
    virtual Handle(AIS_ModelWithAxis) Create(
        const std::string& name,
        PartType type,
        double mainRadius,
        const std::string& sourcePath) = 0;
    // 兼容旧接口名: LoadModel (默认调用 Create)
    virtual Handle(AIS_ModelWithAxis) LoadModel(
        const std::string& name,
        PartType type,
        double mainRadius,
        const std::string& sourcePath) { return Create(name,type,mainRadius,sourcePath); }
};
