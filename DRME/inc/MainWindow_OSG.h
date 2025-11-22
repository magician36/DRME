/********************************************************************************
** Form generated from reading UI file 'MainWindow_OSG.ui'
**
** Created by: Qt User Interface Compiler version 5.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef MAINWINDOW_OSG_H
#define MAINWINDOW_OSG_H
#pragma once
#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include <QtWidgets/QMdiSubWindow>
#include <QtWidgets/QFileDialog>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QDockWidget>

#include <QtWidgets/QStackedWidget>

#include "OCCTWidget.h"
#include <QtGui/QOpenGLFunctions>

#include "OCCBrepDataProcess.h"
#include "OCC2Mesh.h"
#include "OCCSubWindow.h"

#include "ClosableTabWidget.h"

#include "PartGraph.h"  
using namespace std;

QT_BEGIN_NAMESPACE

class Ui_MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	QAction *action;
	QAction *action_2;
	QAction *action_4;
	QAction *action_8;
	QAction *action_9;
	QAction *action_10;
	QAction *action_11;
	QAction *action_12;
	QAction *action_14;
	QAction *action_15;
	QAction *action_17;
	QAction *action_18;
	QAction *action_19;
	QAction *action_20;
	QAction *action_21;
	QAction *actionNew;
	QAction *actionOpen;
	QAction *action_3;
	QAction *action_5;
	QAction *action_6;
	QAction *action_7;
	QAction *action_13;
	QAction *actionTop;
	QAction *actionLeft;
	QAction *action_16;
	QAction *action_22;

	QAction* actionUndo;
	QAction* actionRedo;
	QAction* actionLoadAssembly; // 已有: 加载装配信息 JSON
	QAction* actionSaveAssembly; // 新增: 保存装配信息 JSON

	QWidget *centralwidget;
	QMdiArea *mdiArea;
	QMenuBar *menubar;
	QMenu *menu;
	QMenu *menu_2;
	QMenu *menu_3;
	QMenu *menu_7;
	QMenu *menu_8;
	QMenu *menu_9;
	QMenu *menu_4;
	QMenu *menu_5;
	QMenu *menu_10;
	QMenu *menu_11;

	QToolBar *toolBar;
	QStatusBar *statusBar;
	QToolBar *toolBar_2;

	vector< OCCSubWindow*> pOCCWindows;

	QDockWidget* FeatureItems;
	QTreeWidget* featureTreeWidget;

	QString pPreFilePath;
	QMainWindow *pMainWindow;
	ClosableTabWidget* pQTabWidget = new ClosableTabWidget;
	PartGraph partGraph;   ///< 存储所有导入零件的几何与属性信息

	// Structures for undo/redo of part operations
	struct PartAction {
		enum Type { DeleteParts, ReplacePart } type;
		struct DeletedPartInfo {
			QTreeWidgetItem* parent = nullptr;
			int index = -1;
			QString text0;
			QString text1;
			AIS_ColoredShape* shape = nullptr;
			QTreeWidgetItem* item = nullptr;
		};
		struct ReplaceInfo {
			QTreeWidgetItem* item = nullptr;
			AIS_ColoredShape* oldShape = nullptr;
			AIS_ColoredShape* newShape = nullptr;
			QString oldName;
			QString newName;
			QString oldStatus;
			QString newStatus;
		};
		std::vector<DeletedPartInfo> deletedParts; // for DeleteParts
		ReplaceInfo replace; // for ReplacePart
	};
	std::vector<PartAction> undoStack;
	std::vector<PartAction> redoStack;
	void pushUndo(const PartAction& a);
	void doUndo();
	void doRedo();

	std::map<QMdiSubWindow*, QTreeWidgetItem*> windowRootMap; // 每个窗口的根节点

	QTreeWidgetItem* ensureWindowRoot(QMdiSubWindow* win);

	Ui_MainWindow();

    void setupUi(QMainWindow *MainWindow)
    {
		if (MainWindow->objectName().isEmpty())
			MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
		MainWindow->resize(800, 600);
		action = new QAction(MainWindow);
		action->setObjectName(QString::fromUtf8("action"));
		action_2 = new QAction(MainWindow);
		action_2->setObjectName(QString::fromUtf8("action_2"));
		action_4 = new QAction(MainWindow);
		action_4->setObjectName(QString::fromUtf8("action_4"));
		action_8 = new QAction(MainWindow);
		action_8->setObjectName(QString::fromUtf8("action_8"));
		action_9 = new QAction(MainWindow);
		action_9->setObjectName(QString::fromUtf8("action_9"));
		action_10 = new QAction(MainWindow);
		action_10->setObjectName(QString::fromUtf8("action_10"));
		action_11 = new QAction(MainWindow);
		action_11->setObjectName(QString::fromUtf8("action_11"));
		action_12 = new QAction(MainWindow);
		action_12->setObjectName(QString::fromUtf8("action_12"));
		action_14 = new QAction(MainWindow);
		action_14->setObjectName(QString::fromUtf8("action_14"));
		action_15 = new QAction(MainWindow);
		action_15->setObjectName(QString::fromUtf8("action_15"));
		action_17 = new QAction(MainWindow);
		action_17->setObjectName(QString::fromUtf8("action_17"));
		action_18 = new QAction(MainWindow);
		action_18->setObjectName(QString::fromUtf8("action_18"));
		action_19 = new QAction(MainWindow);
		action_19->setObjectName(QString::fromUtf8("action_19"));
		action_20 = new QAction(MainWindow);
		action_20->setObjectName(QString::fromUtf8("action_20"));
		action_21 = new QAction(MainWindow);
		action_21->setObjectName(QString::fromUtf8("action_21"));
		actionNew = new QAction(MainWindow);
		actionNew->setObjectName(QString::fromUtf8("actionNew"));
		actionOpen = new QAction(MainWindow);
		actionOpen->setObjectName(QString::fromUtf8("actionOpen"));
		action_3 = new QAction(MainWindow);
		action_3->setObjectName(QString::fromUtf8("action_3"));
		action_5 = new QAction(MainWindow);
		action_5->setObjectName(QString::fromUtf8("action_5"));
		action_6 = new QAction(MainWindow);
		action_6->setObjectName(QString::fromUtf8("action_6"));
		action_7 = new QAction(MainWindow);
		action_7->setObjectName(QString::fromUtf8("action_7"));
		action_13 = new QAction(MainWindow);
		action_13->setObjectName(QString::fromUtf8("action_13"));
		actionTop = new QAction(MainWindow);
		actionTop->setObjectName(QString::fromUtf8("actionTop"));
		actionLeft = new QAction(MainWindow);
		actionLeft->setObjectName(QString::fromUtf8("actionLeft"));
		action_16 = new QAction(MainWindow);
		action_16->setObjectName(QString::fromUtf8("action_16"));
		action_22 = new QAction(MainWindow);
		action_22->setObjectName(QString::fromUtf8("action_22"));

		// create undo/redo actions
		actionUndo = new QAction(MainWindow);
		actionUndo->setObjectName(QString::fromUtf8("actionUndo"));
		actionRedo = new QAction(MainWindow);
		actionRedo->setObjectName(QString::fromUtf8("actionRedo"));
		actionLoadAssembly = new QAction(MainWindow); // 已有
		actionLoadAssembly->setObjectName(QString::fromUtf8("actionLoadAssembly"));
		actionSaveAssembly = new QAction(MainWindow); // 新增: 保存
		actionSaveAssembly->setObjectName(QString::fromUtf8("actionSaveAssembly"));

		centralwidget = new QWidget(MainWindow);
		centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
		mdiArea = new QMdiArea(centralwidget);
		mdiArea->setObjectName(QString::fromUtf8("mdiArea"));
		mdiArea->setGeometry(QRect(290, 150, 200, 160));
		MainWindow->setCentralWidget(centralwidget);
		menubar = new QMenuBar(MainWindow);
		menubar->setObjectName(QString::fromUtf8("menubar"));
		menubar->setGeometry(QRect(0, 0, 800, 22));
		menu = new QMenu(menubar);
		menu->setObjectName(QString::fromUtf8("menu"));
		menu_2 = new QMenu(menubar);
		menu_2->setObjectName(QString::fromUtf8("menu_2"));
		menu_3 = new QMenu(menubar);
		menu_3->setObjectName(QString::fromUtf8("menu_3"));
		menu_7 = new QMenu(menu_3);
		menu_7->setObjectName(QString::fromUtf8("menu_7"));
		menu_8 = new QMenu(menu_3);
		menu_8->setObjectName(QString::fromUtf8("menu_8"));
		menu_9 = new QMenu(menu_3);
		menu_9->setObjectName(QString::fromUtf8("menu_9"));
		menu_4 = new QMenu(menubar);
		menu_4->setObjectName(QString::fromUtf8("menu_4"));
		menu_5 = new QMenu(menubar);
		menu_5->setObjectName(QString::fromUtf8("menu_5"));
		menu_10 = new QMenu(menubar);
		menu_10->setObjectName(QString::fromUtf8("menu_10"));
		menu_11 = new QMenu(menubar);
		menu_11->setObjectName(QString::fromUtf8("menu_11"));
		MainWindow->setMenuBar(menubar);
		toolBar = new QToolBar(MainWindow);
		toolBar->setObjectName(QString::fromUtf8("toolBar"));
		MainWindow->addToolBar(Qt::TopToolBarArea, toolBar);
		statusBar = new QStatusBar(MainWindow);
		statusBar->setObjectName(QString::fromUtf8("statusBar"));
		MainWindow->setStatusBar(statusBar);
		toolBar_2 = new QToolBar(MainWindow);
		toolBar_2->setObjectName(QString::fromUtf8("toolBar_2"));
		MainWindow->addToolBar(Qt::TopToolBarArea, toolBar_2);
		MainWindow->insertToolBarBreak(toolBar_2);

		menubar->addAction(menu->menuAction());
		menubar->addAction(menu_2->menuAction());
		menubar->addAction(menu_3->menuAction());
		menubar->addAction(menu_4->menuAction());
		menubar->addAction(menu_5->menuAction());
		menubar->addAction(menu_10->menuAction());
		menubar->addAction(menu_11->menuAction());
		menu->addAction(action_2);
		menu->addAction(action);
		menu->addAction(action_13);
		menu->addAction(action_21);

		// add undo/redo into same menu
		menu->addAction(actionUndo);
		menu->addAction(actionRedo);
		menu->addAction(actionLoadAssembly);
		menu->addAction(actionSaveAssembly); // 新增: 保存装配信息

		menu_2->addAction(action_16);
		menu_2->addAction(action_22);
		menu_3->addAction(menu_7->menuAction());
		menu_3->addAction(menu_8->menuAction());
		menu_3->addAction(menu_9->menuAction());
		menu_7->addAction(action_8);
		menu_7->addAction(action_9);
		menu_7->addAction(action_10);
		menu_7->addAction(action_11);
		menu_7->addAction(action_12);
		menu_8->addAction(action_14);
		menu_8->addAction(action_15);
		menu_9->addAction(action_17);
		menu_9->addAction(action_18);
		menu_4->addAction(action_19);
		menu_4->addAction(action_20);
		menu_5->addAction(action_3);
		menu_5->addAction(action_5);
		menu_10->addAction(action_6);
		menu_10->addAction(action_7);
		toolBar->addAction(actionNew);
		toolBar->addAction(actionOpen);
		toolBar_2->addAction(actionTop);
		toolBar_2->addAction(actionLeft);

		// add to toolbar for quick access
		toolBar->addAction(actionUndo);
		toolBar->addAction(actionRedo);
		toolBar->addAction(actionLoadAssembly);
		toolBar->addAction(actionSaveAssembly); // 新增: 工具栏按钮

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);

		pMainWindow = MainWindow;
		Init(MainWindow);

		connect(this->action_2, &QAction::triggered, this, &Ui_MainWindow::NewDoc);
		connect(this->mdiArea, &QMdiArea::subWindowActivated, this,&Ui_MainWindow::SubWindowOperation);
		connect(this->action, &QAction::triggered, this, &Ui_MainWindow::openShape);
		connect(this->actionOpen, &QAction::triggered, this, &Ui_MainWindow::openShape);
		connect(this->actionNew, &QAction::triggered, this, &Ui_MainWindow::NewDoc);
		connect(this->action_19, &QAction::triggered, this, &Ui_MainWindow::mesh3d);
		connect(this->action_8, &QAction::triggered, this, &Ui_MainWindow::FuelRoadDesign);
		connect(this->action_16, &QAction::triggered, this, &Ui_MainWindow::ViewCascade);
		connect(this->action_22, &QAction::triggered, this, &Ui_MainWindow::ViewTiled);
		connect(action_13, &QAction::triggered, this, &Ui_MainWindow::saveModel); // 新增: 保存按钮绑定

		connect(actionUndo, &QAction::triggered, this, &Ui_MainWindow::doUndo);
		connect(actionRedo, &QAction::triggered, this, &Ui_MainWindow::doRedo);
		connect(actionLoadAssembly, &QAction::triggered, this, &Ui_MainWindow::loadAssemblyJson);
		connect(actionSaveAssembly, &QAction::triggered, this, &Ui_MainWindow::saveAssemblyJson); // 新增: 保存装配信息 JSON

		connect(pQTabWidget, &QTabWidget::currentChanged, this, [this](int index) {
			
			OCCTWidget* pOCCWidget = (OCCTWidget*)pQTabWidget->currentWidget();

			if (pOCCWidget != nullptr)
			{
				pOCCWidget->get3dView()->FitAll();
				pOCCWidget->get3dView()->MustBeResized();
			}
		});
		
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
		MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\346\225\260\345\200\274\345\217\215\345\272\224\345\240\206\344\270\211\347\273\264\345\273\272\346\250\241\345\274\225\346\223\216", nullptr));
		action->setText(QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200", nullptr));
		action_2->setText(QCoreApplication::translate("MainWindow", "\346\226\260\345\273\272", nullptr));
		action_4->setText(QCoreApplication::translate("MainWindow", "\347\207\203\346\226\231\346\243\222", nullptr));
		action_8->setText(QCoreApplication::translate("MainWindow", "\347\207\203\346\226\231\346\243\222", nullptr));
		action_9->setText(QCoreApplication::translate("MainWindow", "\346\216\247\345\210\266\346\243\222", nullptr));
		action_10->setText(QCoreApplication::translate("MainWindow", "\347\207\203\346\226\231\347\273\204\344\273\266", nullptr));
		action_11->setText(QCoreApplication::translate("MainWindow", "\345\233\264\346\235\277\345\217\215\345\260\204\345\261\202", nullptr));
		action_12->setText(QCoreApplication::translate("MainWindow", "\345\240\206\345\206\205\346\236\204\344\273\266", nullptr));
		action_14->setText(QCoreApplication::translate("MainWindow", "\347\256\241\351\201\223", nullptr));
		action_15->setText(QCoreApplication::translate("MainWindow", "\350\256\276\345\244\207", nullptr));
		action_17->setText(QCoreApplication::translate("MainWindow", "\346\240\274\346\236\266", nullptr));
		action_18->setText(QCoreApplication::translate("MainWindow", "\346\265\201\351\207\217\345\210\206\351\205\215\345\255\224\346\235\277", nullptr));
		action_19->setText(QCoreApplication::translate("MainWindow", "\344\270\211\350\247\222\347\275\221\346\240\274", nullptr));
		action_20->setText(QCoreApplication::translate("MainWindow", "\345\205\255\351\235\242\344\275\223\347\275\221\346\240\274", nullptr));
		action_21->setText(QCoreApplication::translate("MainWindow", "\345\205\263\351\227\255", nullptr));
		actionNew->setText(QCoreApplication::translate("MainWindow", "New", nullptr));
		actionOpen->setText(QCoreApplication::translate("MainWindow", "Open", nullptr));
		action_3->setText(QCoreApplication::translate("MainWindow", "\344\270\255\345\255\220\345\255\246", nullptr));
		action_5->setText(QCoreApplication::translate("MainWindow", "\347\203\255\345\267\245\346\260\264\345\212\233\345\255\246", nullptr));
		action_6->setText(QCoreApplication::translate("MainWindow", "\347\250\263\346\200\201", nullptr));
		action_7->setText(QCoreApplication::translate("MainWindow", "\347\236\254\346\200\201", nullptr));
		action_13->setText(QCoreApplication::translate("MainWindow", "\344\277\235\345\255\230", nullptr));
		actionTop->setText(QCoreApplication::translate("MainWindow", "Top", nullptr));
		actionLeft->setText(QCoreApplication::translate("MainWindow", "Left", nullptr));
		action_16->setText(QCoreApplication::translate("MainWindow", "\347\272\247\350\201\224", nullptr));
		action_22->setText(QCoreApplication::translate("MainWindow", "\345\271\263\351\223\272", nullptr));

		// Fix: use proper UTF-8 Chinese instead of mojibake escape sequence
		actionUndo->setText(QStringLiteral("撤销"));
		actionRedo->setText(QStringLiteral("重做"));
		actionLoadAssembly->setText(QStringLiteral("加载装配信息"));
		actionSaveAssembly->setText(QStringLiteral("保存装配信息")); // 新增: 保存装配信息

		menu->setTitle(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266", nullptr));
		menu_2->setTitle(QCoreApplication::translate("MainWindow", "\350\247\206\345\233\276", nullptr));
		menu_3->setTitle(QCoreApplication::translate("MainWindow", "\345\273\272\346\250\241", nullptr));
		menu_7->setTitle(QCoreApplication::translate("MainWindow", "\345\240\206\350\212\257", nullptr));
		menu_8->setTitle(QCoreApplication::translate("MainWindow", "\344\270\200\345\233\236\350\267\257", nullptr));
		menu_9->setTitle(QCoreApplication::translate("MainWindow", "\347\211\271\346\256\212\351\203\250\344\273\266", nullptr));
		menu_4->setTitle(QCoreApplication::translate("MainWindow", "\347\275\221\346\240\274", nullptr));
		menu_5->setTitle(QCoreApplication::translate("MainWindow", "\345\255\246\347\247\221\346\261\202\350\247\243", nullptr));
		menu_10->setTitle(QCoreApplication::translate("MainWindow", "\345\217\257\350\247\206\345\214\226", nullptr));
		menu_11->setTitle(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
		toolBar->setWindowTitle(QCoreApplication::translate("MainWindow", "toolBar", nullptr));
		toolBar_2->setWindowTitle(QCoreApplication::translate("MainWindow", "toolBar_2", nullptr));
    } // retranslateUi

	void show(QMainWindow* Form);
	void Init(QMainWindow *MainWindow);
	OCCSubWindow* AddSubWindow();
	void SubWindowOperation();
	void openShape();
	void NewDoc();
	void mesh3d();
	void FuelRoadDesign();
	void ViewCascade();
	void ViewTiled();

	// 保存模型（支持选中或全部导出）
	void saveModel();          // <--- 标注: 保存功能声明位置
	// 批量删除零件（支持撤销）
	void deleteParts(const QList<QTreeWidgetItem*>& items); // <--- 标注: 删除功能声明位置
	// 替换单个零件（支持撤销）
	void replacePart(QTreeWidgetItem* item, const QString& stepFile); // <--- 标注: 替换功能声明位置
	void loadAssemblyJson(); // 已有: 从 JSON 加载装配工程
	void saveAssemblyJson(); // 新增: 保存当前装配到 JSON
};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // MAINWINDOW_OSG_H

