#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QMimeData>
#include <QDrag>

class PartLibraryDockWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PartLibraryDockWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        m_tree = new QTreeWidget(this);
        m_tree->setColumnCount(1);
        m_tree->setHeaderLabel(QStringLiteral("零件库"));
        m_tree->setExpandsOnDoubleClick(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

        // 支持拖拽启动
        m_tree->setDragEnabled(true);
        connect(m_tree, &QTreeWidget::itemPressed, this, [this](QTreeWidgetItem* item) {
            if (!item) return;
            // 只有叶子节点（文件）才可拖动
            if (!item->childCount())
                startDragFromItem(item);
            });
        m_tree->setDefaultDropAction(Qt::CopyAction);

        layout->addWidget(m_tree);

    }

    void setBaseDir(const QString& base) { m_baseDir = base; }
    void reload()
    {
        m_tree->clear();

        // 顶级三类：棒 / 滑块 / 螺钉
        QTreeWidgetItem* rodRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("棒 (Rod)"));
        QTreeWidgetItem* sliderRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("滑块 (Slider)"));
        QTreeWidgetItem* screwRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("螺钉 (Screw)"));

        m_tree->addTopLevelItems({ rodRoot, sliderRoot, screwRoot });

        // 约定：固定目录结构
        addCategory(rodRoot, QDir(m_baseDir + QStringLiteral("/Rod")));
        addCategory(sliderRoot, QDir(m_baseDir + QStringLiteral("/Slider")));
        addCategory(screwRoot, QDir(m_baseDir + QStringLiteral("/Screw")));


        // 展开一级
        m_tree->expandItem(rodRoot);
        m_tree->expandItem(sliderRoot);
        m_tree->expandItem(screwRoot);
    }

signals:
    void fileActivated(const QString& stepPath); // 双击项时发出

protected:
    // 开始拖拽：把文件路径放进 mime data
    void startDragFromItem(QTreeWidgetItem* it)
    {
        if (!it) return;
        const QString path = it->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;

        auto* mime = new QMimeData();
        mime->setText(path);                // 把路径放入拖拽数据
        auto* drag = new QDrag(m_tree);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
    }

private:
    void addCategory(QTreeWidgetItem* root, const QDir& dir)
    {
        if (!dir.exists()) return;
        const QStringList exts{ QStringLiteral("*.stp"), QStringLiteral("*.step") };

        const auto files = dir.entryInfoList(exts, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo& fi : files)
        {
            QString name = fi.completeBaseName();   // 获取不带扩展名的文件名
            auto* leaf = new QTreeWidgetItem(root, QStringList() << name);
            leaf->setData(0, Qt::UserRole, fi.absoluteFilePath());
            leaf->setToolTip(0, fi.absoluteFilePath());
        }
        // 支持子目录递归（可选）
        const auto subs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& sub : subs)
        {
            auto* subRoot = new QTreeWidgetItem(root, QStringList() << QString(sub.fileName()));
            addCategory(subRoot, QDir(sub.absoluteFilePath()));
        }
    }

    QTreeWidget* m_tree = nullptr;
    QString m_baseDir;
};
#pragma once
