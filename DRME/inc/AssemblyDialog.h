#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include "AIS_ModelWithAxis.h"
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include "PartGraph.h"

class AssemblyDialog : public QDialog
{
    Q_OBJECT

public:
    AssemblyDialog(
        PartGraph* graph,
        const Handle(AIS_InteractiveContext)& context,
        const Handle(V3d_View)& view,
        QWidget* parent = nullptr);

    ~AssemblyDialog() override;

private slots://  Qt信号槽
    void onPartASelected(int row, int col);
    void onPartBSelected(int row, int col);
    void onHoleSelected(int row, int col);
    void onFinishClicked();

private:
    void populatePartATable();
    void populatePartBTable(const std::string& partA);
    void populateHoleTable(const std::string& partB);
    void clearHighlight();
    void highlightPart(const Handle(AIS_ModelWithAxis)& model);
    void highlightHoleFace(const Handle(AIS_ModelWithAxis)& model, const gp_Ax1& axis, double radius);

private:
    PartGraph* m_graph;
    Handle(AIS_InteractiveContext) m_context;
    Handle(V3d_View) m_view;

    QTableWidget* m_tableA;
    QTableWidget* m_tableB;
    QTableWidget* m_tableC;
    QPushButton* m_btnFinish;
    QPushButton* m_btnClose;

    std::string m_selectedA;
    std::string m_selectedB;
    int m_selectedHoleIndex = -1;

	std::string m_sliderName;       //当前选配对中的滑块名称

    Handle(AIS_InteractiveObject) m_highlightedFace;
};
