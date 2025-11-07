# 装配功能设计文档

## 概述
本设计将装配逻辑从 PartGraph 中分离，创建独立的 PartAssembler 类来处理零件装配。

## 文件结构

### 新增文件
1. **Modeling\inc\PartAssembler.h** - 装配器类头文件
2. **Modeling\src\PartAssembler.cpp** - 装配器类实现文件

### 修改文件
1. **Modeling\inc\PartGraph.h** - 添加 friend 声明
2. **DRME\src\AssemblyDialog.cpp** - 集成装配器

## 设计思路

### 1. 装配流程
```
用户操作流程：
A表（选择零件A）→ B表（选择可装配零件B）→ C表（选择装配孔）→ 点击完成按钮

装配过程：
1. 获取零件B上选定孔的轴线（考虑当前变换）
2. 在零件A上查找匹配类型的孔（优先匹配孔类型，否则使用主轴）
3. 如果轴线方向相反，翻转源轴
4. 构造稳定的参考坐标系（避免万向锁）
5. 使用 gp_Trsf::SetDisplacement 计算精确变换
6. 应用变换到零件A及其所有孔
7. 更新约束关系
```

### 2. 核心类设计

#### PartAssembler 类
```cpp
class PartAssembler
{
public:
    // 构造函数
    PartAssembler(PartGraph* graph, const Handle(AIS_InteractiveContext)& context);
    
    // 主装配函数
    bool AssembleParts(const std::string& partA, const std::string& partB, int holeIndexB);

private:
    // 构造稳定参考坐标系
    gp_Ax2 BuildFrame(const gp_Ax1& axis) const;
    
    // 查找源孔轴
    bool FindSourceAxis(const PartInfo& partInfo, HoleType targetHoleType, gp_Ax1& outAxis) const;
    
    // 应用变换
    bool ApplyTransformation(PartInfo& partInfo, const gp_Trsf& trsf);
    
    // 更新约束
    void UpdateConstraints(PartInfo& partA, const std::string& partBName, HoleType holeType);
    
    // 记录日志
    void LogAssemblyInfo(...) const;
};
```

### 3. 关键技术点

#### 3.1 稳定坐标系构造
```cpp
gp_Ax2 BuildFrame(const gp_Ax1& axis)
{
    // Z轴 = 孔轴方向
    gp_Dir zDir = axis.Direction();
    
    // 选择参考方向（避免与Z轴平行）
    gp_Dir refDir = (std::abs(zDir.Z()) < 0.9) ? gp_Dir(0,0,1) : gp_Dir(1,0,0);
    
    // 计算X轴 = Z × ref
    gp_Vec vx = gp_Vec(zDir).Crossed(gp_Vec(refDir));
    
    // 如果叉积太小，换参考方向
    if (vx.Magnitude() < 1e-6) {
        refDir = gp_Dir(0,1,0);
        vx = gp_Vec(zDir).Crossed(gp_Vec(refDir));
    }
    
    return gp_Ax2(axis.Location(), zDir, gp_Dir(vx));
}
```

#### 3.2 变换计算
```cpp
// 构造源和目标坐标系
gp_Ax2 frameSource = BuildFrame(sourceAxis);
gp_Ax2 frameTarget = BuildFrame(targetAxis);

// 精确计算变换（位置+旋转）
gp_Trsf trsf;
trsf.SetDisplacement(frameSource, frameTarget);
```

#### 3.3 孔轴更新
```cpp
// 应用变换后，所有孔轴都要更新到新位置
for (auto& hole : partInfo.holes) {
    gp_Ax1 axis = hole.second;
    axis.Transform(trsf);
    hole.second = axis;
}
```

### 4. 与对话框集成

#### AssemblyDialog::onFinishClicked()
```cpp
void AssemblyDialog::onFinishClicked()
{
    // 检查选择完整性
    if (m_selectedA.empty() || m_selectedB.empty() || m_selectedHoleIndex < 0) {
        // 提示用户
        return;
    }
    
    // 创建装配器
    PartAssembler assembler(m_graph, m_context);
    
    // 确定移动零件和固定零件
    std::string movingPart = (m_sliderName == m_selectedA) ? m_selectedB : m_selectedA;
    std::string fixedPart = (m_sliderName == m_selectedA) ? m_selectedA : m_selectedB;
    
    // 执行装配
    bool success = assembler.AssembleParts(movingPart, fixedPart, m_selectedHoleIndex);
    
    if (success) {
        // 成功提示，刷新界面
        populatePartATable();
        // 清空选择
    } else {
        // 失败提示
    }
}
```

## 特性

### 支持重复装配
- 同一零件可以多次装配到不同孔位
- 每次装配都会覆盖之前的变换
- 孔轴位置随变换自动更新

### 方向自适应
- 自动检测轴线方向
- 如果方向相反，自动翻转源轴

### 异常处理
- 参数有效性检查
- 变换异常捕获
- 详细错误日志

### 调试信息
- 装配过程的关键信息输出
- 位移距离和角度计算
- Qt 和 标准输出双重日志

## 优势

1. **代码分离**: 装配逻辑独立，易于维护和测试
2. **可扩展**: 可以轻松添加新的装配模式
3. **鲁棒性**: 完善的错误处理和参数检查
4. **精确性**: 使用 OpenCASCADE 标准变换方法
5. **可重用**: 装配器可以在不同场景中使用

## 使用示例

```cpp
// 创建装配器
PartAssembler assembler(&partGraph, context);

// 装配零件A到零件B的第2个孔
bool success = assembler.AssembleParts("零件A", "零件B", 2);

if (success) {
    std::cout << "装配成功" << std::endl;
}
```

## 注意事项

1. 装配后零件A的所有孔轴都会更新到新坐标系
2. 约束关系会被自动记录
3. 已装配的零件会从A表中过滤掉
4. 装配过程中会自动刷新3D视图

## 未来改进方向

1. 支持装配撤销/重做
2. 支持装配历史记录
3. 支持装配动画
4. 支持碰撞检测
5. 支持装配约束验证
