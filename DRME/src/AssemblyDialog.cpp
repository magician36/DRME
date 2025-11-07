#include "AssemblyDialog.h"
#include "PartAssembler.h"
#include "PartConstraints.h" // 新增：统一约束管理
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <BRep_Tool.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <QDebug>
#include <QMessageBox>
#include <TopoDS.hxx>

AssemblyDialog::AssemblyDialog(
    PartGraph* graph,
    const Handle(AIS_InteractiveContext)& context,
    const Handle(V3d_View)& view,
    QWidget* parent)
    : QDialog(parent), m_graph(graph), m_context(context), m_view(view)
{
    setWindowTitle(QStringLiteral("零件装配"));
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, true);
    resize(420, 480);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QLabel* labelA = new QLabel(QStringLiteral("A表：可装配零件"));
    QLabel* labelB = new QLabel(QStringLiteral("B表：匹配零件"));
    QLabel* labelC = new QLabel(QStringLiteral("C表：孔选择"));

    m_tableA = new QTableWidget(this);
    m_tableB = new QTableWidget(this);
    m_tableC = new QTableWidget(this);

    m_tableA->setColumnCount(2);
    m_tableB->setColumnCount(2);
    m_tableC->setColumnCount(3);

    m_tableA->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("名称") << QStringLiteral("类型"));
    m_tableB->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("名称") << QStringLiteral("匹配件"));
    m_tableC->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("孔类型") << QStringLiteral("孔半径") << QStringLiteral("孔编号"));


    for (auto* t : { m_tableA, m_tableB, m_tableC })
    {
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    // 按钮区
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnFinish = new QPushButton(QStringLiteral("完成"), this);
    m_btnClose = new QPushButton(QStringLiteral("关闭"), this);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnFinish);
    btnLayout->addWidget(m_btnClose);

    // 组合布局
    mainLayout->addWidget(labelA);
    mainLayout->addWidget(m_tableA, 1);
    mainLayout->addWidget(labelB);
    mainLayout->addWidget(m_tableB, 1);
    mainLayout->addWidget(labelC);
    mainLayout->addWidget(m_tableC, 1);
    mainLayout->addLayout(btnLayout);

    // 信号槽
    connect(m_tableA, &QTableWidget::cellClicked, this, &AssemblyDialog::onPartASelected);
    connect(m_tableB, &QTableWidget::cellClicked, this, &AssemblyDialog::onPartBSelected);
    connect(m_tableC, &QTableWidget::cellClicked, this, &AssemblyDialog::onHoleSelected);
    connect(m_btnFinish, &QPushButton::clicked, this, &AssemblyDialog::onFinishClicked);
    connect(m_btnClose, &QPushButton::clicked, this, [=]() { close(); });

    populatePartATable();
}

// ---------------- 填充A表 ----------------
void AssemblyDialog::populatePartATable()
{
    m_tableA->setRowCount(0);
    if (!m_graph) return;

    const auto& parts = m_graph->GetParts();  // 不要拷贝，直接引用即可

    int row = 0;
    for (const auto& [name, info] : parts)
    {
        // 跳过已装配的零件
        if (!info.constraint.targetPart.empty()) continue;

        m_tableA->insertRow(row);

        // ✅ 用 UTF-8 解析 std::string，并且去除首尾空格（安全）
        QString qName = QString::fromUtf8(name.c_str()).trimmed();
        m_tableA->setItem(row, 0, new QTableWidgetItem(qName));

        // 类型转中文显示或英文都可以
        QString typeStr;
        switch (info.type)
        {
        case PartType::Rod:    typeStr = QStringLiteral("Rod");    break;
        case PartType::Slider: typeStr = QStringLiteral("Slider"); break;
        case PartType::Screw:  typeStr = QStringLiteral("Screw");  break;
        }

        m_tableA->setItem(row, 1, new QTableWidgetItem(typeStr));

        // ✅ 调试输出，验证每一行内容
        qDebug() << "[A表]" << qName << typeStr;

        row++;
    }
}

// ---------------- A选中 → 填充B表 ----------------
void AssemblyDialog::onPartASelected(int row, int col)
{
    Q_UNUSED(col);
    clearHighlight();

    if (!m_tableA->item(row, 0)) return;

    // ✅ 从 A 表取出零件名（UTF-8 + 去空格）
    QString qName = m_tableA->item(row, 0)->text().trimmed();
    m_selectedA = qName.toUtf8().constData();

    qDebug() << "\n[A表] 选中零件:" << qName << "→ m_selectedA =" << QString::fromUtf8(m_selectedA.c_str());

    // 高亮 A
    const auto& parts = m_graph->GetParts();
    auto it = parts.find(m_selectedA);
    if (it != parts.end() && !it->second.model.IsNull())
        highlightPart(it->second.model);
    else
        qWarning() << "[A表] 未找到零件:" << qName;

    // --- 根据A刷新B表 ---
    populatePartBTable(m_selectedA);
}

// ---------------- 填充B表 ----------------
void AssemblyDialog::populatePartBTable(const std::string& partA)
{
    m_tableB->setRowCount(0);
    if (!m_graph) return;

    const auto& parts = m_graph->GetParts();
    auto itA = parts.find(partA);
    if (itA == parts.end()) {
        qWarning() << "[B表] 未找到零件A:" << QString::fromUtf8(partA.c_str());
        return;
    }

    const PartInfo& infoA = itA->second;
    int row = 0;

    qDebug() << "[B表] 为零件A生成匹配列表:" << QString::fromUtf8(partA.c_str());

    for (const auto& [name, infoB] : parts)
    {
        if (name == partA) continue; // 跳过自身

        bool canPair = false;
        if ((infoA.type == PartType::Rod && infoB.type == PartType::Slider) ||
            (infoA.type == PartType::Slider && infoB.type == PartType::Rod) ||
            (infoA.type == PartType::Screw && infoB.type == PartType::Slider) ||
            (infoA.type == PartType::Slider && infoB.type == PartType::Screw))
            canPair = true;

        if (!canPair) continue;

        // === 添加到表格 ===
        m_tableB->insertRow(row);
        QString qName = QString::fromUtf8(name.c_str()).trimmed();
        QString typeStr = (infoB.type == PartType::Rod)
            ? QStringLiteral("Rod")
            : (infoB.type == PartType::Slider)
            ? QStringLiteral("Slider")
            : QStringLiteral("Screw");

        m_tableB->setItem(row, 0, new QTableWidgetItem(qName));
        m_tableB->setItem(row, 1, new QTableWidgetItem(typeStr));

        qDebug() << "[B表] 添加匹配项:" << qName << typeStr;
        row++;
    }
}

// ---------------- B选中 → 填充C表 ----------------
void AssemblyDialog::onPartBSelected(int row, int col)
{
    Q_UNUSED(col);
    clearHighlight();

    if (!m_tableB->item(row, 0)) return;

    // 从 B 表取出零件名（UTF-8 + 去空格）
    QString qName = m_tableB->item(row, 0)->text().trimmed();
    m_selectedB = qName.toUtf8().constData();

    qDebug() << "\n[B表] 选中零件:" << qName << "→ m_selectedB =" << QString::fromUtf8(m_selectedB.c_str());

    const auto& parts = m_graph->GetParts();

    // --- 高亮 B ---
    auto it = parts.find(m_selectedB);
    if (it != parts.end() && !it->second.model.IsNull())
        highlightPart(it->second.model);
    else
        qWarning() << "[B表] 未找到零件:" << qName;

    // --- 确定哪个是滑块 ---
    std::string sliderName;
    auto itA = parts.find(m_selectedA);
    if (itA != parts.end() && itA->second.type == PartType::Slider)
        sliderName = m_selectedA;
    else if (it != parts.end() && it->second.type == PartType::Slider)
        sliderName = m_selectedB;

    if (sliderName.empty()) {
        qWarning() << "[C表] 未找到滑块类型零件，无法显示孔";
        m_tableC->setRowCount(0);
        return;
    }

    m_sliderName = sliderName;  //  保存当前滑块名称

    // === 刷新 C 表 ===
    populateHoleTable(sliderName);
}

// ---------------- 填充孔列表 ----------------
void AssemblyDialog::populateHoleTable(const std::string& partB)
{
    qDebug() << "\n[C表] 填充孔信息 for part =" << QString::fromUtf8(partB.c_str());
    m_tableC->setRowCount(0);
    if (!m_graph) return;

    const auto& parts = m_graph->GetParts();
    auto it = parts.find(partB);
    if (it == parts.end()) {
        qWarning() << "[C表] 未找到零件:" << QString::fromUtf8(partB.c_str());
        return;
    }

    const PartInfo& info = it->second;
    qDebug() << "[C表] 找到孔数量:" << info.holes.size();

    int idx = 0;
    for (size_t i = 0; i < info.holes.size(); ++i)
    {
        HoleType holeType = info.holes[i].first;
        double radius = (i < info.holeRadii.size()) ? info.holeRadii[i] : 0.0;

        m_tableC->insertRow(idx);

        QString hType = (holeType == HoleType::RodHole ? QStringLiteral("RodHole") : QStringLiteral("ScrewHole"));
        m_tableC->setItem(idx, 0, new QTableWidgetItem(hType));
        m_tableC->setItem(idx, 1, new QTableWidgetItem(QString::number(radius, 'f', 3)));
        m_tableC->setItem(idx, 2, new QTableWidgetItem(QString::number(idx)));

        qDebug() << "[C表] 添加孔" << idx << "类型:" << hType << "半径:" << radius;
        idx++;
    }
}

// ---------------- C选中孔 → 高亮圆柱面 ----------------
void AssemblyDialog::onHoleSelected(int row, int col)
{
    Q_UNUSED(col);
    clearHighlight();

    // 保证 C 表点击时有有效数据
    if (row < 0) return;
    if (m_selectedA.empty() || m_selectedB.empty()) return;

    m_selectedHoleIndex = row;

    qDebug() << "[C表] 点击孔行:" << row << "滑块名称(来自B/A逻辑):" << QString::fromStdString(m_sliderName);
    
    // 找出滑块信息
	const auto& parts = m_graph->GetParts();
    auto it = parts.find(m_sliderName);

    if (it == parts.end()) {
        qWarning() << "[onHoleSelected] 未找到滑块:" << QString::fromStdString(m_sliderName);
        return;
    }

    const PartInfo& partInfo = it->second;
    if (partInfo.model.IsNull()) return;

    // 安全检查孔索引
    if (row < 0 || row >= (int)partInfo.holes.size()) return;

    // 获取当前孔信息
    const auto& hole = partInfo.holes[row];
    const auto& axis = hole.second;
    double radius = (row < partInfo.holeRadii.size()) ? partInfo.holeRadii[row] : 0.0;

    // === 高亮孔的圆柱面 ===
    highlightHoleFace(partInfo.model, axis, radius);
}

// ---------------- 完成按钮 ----------------
void AssemblyDialog::onFinishClicked()
{
    if (m_selectedA.empty() || m_selectedB.empty() || m_selectedHoleIndex < 0)
    {
        QMessageBox::warning(this, QStringLiteral("装配"), 
                           QStringLiteral("请先选择完整的装配路径（A表→B表→C表）"));
        return;
    }

    qDebug() << "[装配完成] 准备调用装配函数 →"
        << "A:" << QString::fromStdString(m_selectedA)
        << "B:" << QString::fromStdString(m_selectedB)
        << "孔:" << m_selectedHoleIndex;

    const auto& parts = m_graph->GetParts();
    auto itA = parts.find(m_selectedA);
    auto itB = parts.find(m_selectedB);
    if (itA == parts.end() || itB == parts.end()) {
        QMessageBox::critical(this, QStringLiteral("装配"), 
                            QStringLiteral("找不到选中的零件！"));
        return;
    }

    const PartInfo& A = itA->second;
    const PartInfo& B = itB->second;

    ConstraintManager mgr(m_graph, m_context); // 使用新的统一管理

    // === 专门处理 "滑块 + 螺钉" 组合 ===
    {
        std::string sliderNameSpecial, screwNameSpecial; const PartInfo *pSlider = nullptr, *pScrew = nullptr;
        if (A.type == PartType::Slider && B.type == PartType::Screw) { sliderNameSpecial = m_selectedA; screwNameSpecial = m_selectedB; pSlider = &A; pScrew = &B; }
        else if (B.type == PartType::Slider && A.type == PartType::Screw) { sliderNameSpecial = m_selectedB; screwNameSpecial = m_selectedA; pSlider = &B; pScrew = &A; }
        if (pSlider && pScrew) {
            auto res = mgr.AssembleScrewToSlider(screwNameSpecial, sliderNameSpecial, m_selectedHoleIndex);
            if (!res.success) {
                QMessageBox::critical(this, QStringLiteral("装配"), QString::fromStdString(res.message));
                return;
            }
            QMessageBox::information(this, QStringLiteral("装配"), QStringLiteral("拼接完成，螺钉约束已建立。"));
            populatePartATable();
            m_selectedA.clear(); m_selectedB.clear(); m_selectedHoleIndex = -1; m_sliderName.clear();
            m_tableB->setRowCount(0); m_tableC->setRowCount(0); clearHighlight();
            accept();
            return;
        }
    }

    // 判定哪一个是滑块、哪一个是棒
    std::string sliderName, rodName; const PartInfo *pSlider = nullptr, *pRod = nullptr;
    if (A.type == PartType::Slider && B.type == PartType::Rod) { sliderName = m_selectedA; rodName = m_selectedB; pSlider = &A; pRod = &B; }
    else if (B.type == PartType::Slider && A.type == PartType::Rod) { sliderName = m_selectedB; rodName = m_selectedA; pSlider = &B; pRod = &A; }

    if (!sliderName.empty()) {
        auto res = mgr.AssembleSliderToRod(sliderName, rodName, m_selectedHoleIndex);
        if (!res.success) {
            QMessageBox::critical(this, QStringLiteral("装配"), QString::fromStdString(res.message));
            return;
        }
        QMessageBox::information(this, QStringLiteral("装配"), QStringLiteral("拼接完成，滑块→棒 约束已建立。"));
    }
    else {
        // 普通装配（非约束类型组合）仍执行几何对齐但不写约束
        PartAssembler assembler(m_graph, m_context);
        std::string movingPart = (m_sliderName == m_selectedA) ? m_selectedB : m_selectedA;
        std::string fixedPart = m_sliderName.empty() ? m_selectedA : m_sliderName; // 兜底
        bool success = assembler.AssembleParts(movingPart, fixedPart, m_selectedHoleIndex);
        if (!success) {
            QMessageBox::critical(this, QStringLiteral("装配"), QStringLiteral("装配失败，请检查选择。"));
            return;
        }
        QMessageBox::information(this, QStringLiteral("装配"), QStringLiteral("装配完成(未建立约束)。"));
    }

    // 刷新界面
    populatePartATable();
    m_selectedA.clear(); m_selectedB.clear(); m_selectedHoleIndex = -1; m_sliderName.clear();
    m_tableB->setRowCount(0); m_tableC->setRowCount(0); clearHighlight();
    accept();
}

// ---------------- 工具函数 ----------------
void AssemblyDialog::clearHighlight()
{
    if (m_context.IsNull() || m_view.IsNull()) return;
    if (m_view->Window().IsNull()) return;  // ✅ 新增安全检查

    try {
        m_context->ClearSelected(Standard_True);
        if (!m_highlightedFace.IsNull()) {
            m_context->Erase(m_highlightedFace, Standard_False);
            m_highlightedFace.Nullify();
        }
        //m_view->Redraw();
    }
    catch (...) {
        qWarning() << "[AssemblyDialog] clearHighlight() failed due to OpenGL context issue";
    }
}

void AssemblyDialog::highlightPart(const Handle(AIS_ModelWithAxis)& model)
{
    qDebug() << "=== highlightPart called ===";
    qDebug() << "m_context isNull =" << m_context.IsNull();
    qDebug() << "m_view isNull =" << m_view.IsNull();
    if (!m_view.IsNull())
        qDebug() << "m_view->Window().IsNull =" << m_view->Window().IsNull();
    qDebug() << "model isNull =" << model.IsNull();
    qDebug() << "model raw pointer =" << (void*)model.get();

    if (m_context.IsNull() || m_view.IsNull()) return;
    if (m_view->Window().IsNull()) return;  // ✅ 新增安全检查

    if (model.IsNull()) {
        qWarning() << "[highlightPart] model is NULL!";
        return;
    }
    try {
        m_context->AddOrRemoveSelected(model, Standard_True);
        //m_view->Redraw();
    }
    catch (...) {
        qWarning() << "[AssemblyDialog] highlightPart() failed due to OpenGL context issue";
    }
}

// 高亮孔的圆柱面
void AssemblyDialog::highlightHoleFace(const Handle(AIS_ModelWithAxis)& model, const gp_Ax1& axis, double radius)
{
    if (m_context.IsNull() || m_view.IsNull()) return;
    if (m_view->Window().IsNull()) return;  // ✅ 新增安全检查
    if (model.IsNull()) return;

    try {
        TopoDS_Shape shape = model->Shape();
        double minDist = 1e9;
        TopoDS_Face bestFace;

        for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
            TopoDS_Face face = TopoDS::Face(ex.Current());
            TopLoc_Location loc;
            Handle(Geom_Surface) surf = BRep_Tool::Surface(face, loc);
            Handle(Geom_CylindricalSurface) cyl = Handle(Geom_CylindricalSurface)::DownCast(surf);
            if (cyl.IsNull()) continue;

            gp_Ax1 ax = cyl->Axis();
            ax.Transform(loc.Transformation());
            double r = cyl->Radius();

            double dist = axis.Location().Distance(ax.Location()) + std::abs(r - radius);
            if (dist < minDist) {
                minDist = dist;
                bestFace = face;
            }
        }

        if (!bestFace.IsNull()) {
            m_highlightedFace = new AIS_Shape(bestFace);
            m_context->Display(m_highlightedFace, Standard_False);
            m_context->SetColor(m_highlightedFace, Quantity_NOC_YELLOW, Standard_False);
            m_context->Redisplay(m_highlightedFace, Standard_True);
            //m_view->Redraw();
        }
    }
    catch (...) {
        qWarning() << "[AssemblyDialog] highlightHoleFace() failed due to OpenGL context issue";
    }
}

AssemblyDialog::~AssemblyDialog()
{
    clearHighlight();
}
