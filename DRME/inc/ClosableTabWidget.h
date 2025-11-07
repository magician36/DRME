#ifndef CLOSABLETABWIDGET_H
#define CLOSABLETABWIDGET_H
#pragma once
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTabBar> 
#include <QtWidgets/QStyle>
#include <QtCore/QEvent>
#include <QtGui/QMouseEvent>

#include "OCCTWidget.h"

class ClosableTabWidget : public QTabWidget {
	Q_OBJECT
public:
	explicit ClosableTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {
		// 初始化时添加示例Tab（可选）
	}

	// 添加带关闭按钮的Tab
	void addTabWithClose(QWidget *widget, const QString &title) {
		int index = addTab(widget, title); // 添加Tab
		addCloseButton(index); // 为新Tab添加关闭按钮
	}

private slots:
	// 关闭按钮点击事件
	void onCloseButtonClicked() {
		QToolButton *btn = qobject_cast<QToolButton*>(sender());
		if (!btn) return;

		// 获取按钮所在Tab的索引
		int index = tabBar()->tabAt(btn->mapTo(tabBar(), QPoint(0, 0)));
		if (index != -1) {
			removeTab(index); // 删除Tab
							  // 重新为剩余Tab添加关闭按钮（解决索引变化问题）
			refreshCloseButtons();
		}
	}

private:
	// 为指定索引的Tab添加关闭按钮
	void addCloseButton(int index) {
		QToolButton *closeBtn = new QToolButton(this);
		closeBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
		closeBtn->setStyleSheet(QStringLiteral("border: none; margin: 0px; padding: 0.5px;")); // 美化按钮
		connect(closeBtn, &QToolButton::clicked, this, &ClosableTabWidget::onCloseButtonClicked);
		tabBar()->setTabButton(index, QTabBar::RightSide, closeBtn); // 设置按钮到Tab右侧
	}

	// 刷新所有Tab的关闭按钮（删除Tab后调用）
	void refreshCloseButtons() {
		// 1. 移除所有旧按钮（必须先解除 TabBar 引用，再删除对象）
		for (int i = 0; i < count(); ++i) {
			QWidget* oldBtn = tabBar()->tabButton(i, QTabBar::RightSide);
			if (oldBtn) {
				// 关键：先让 TabBar 放弃对按钮的所有权
				tabBar()->setTabButton(i, QTabBar::RightSide, nullptr);
				// 安全删除按钮（父对象为当前窗口，自动处理内存）
				delete oldBtn;
				oldBtn = nullptr; // 避免野指针
			}
		}

		// 2. 重新为所有 Tab 添加关闭按钮
		for (int i = 0; i < count(); ++i) {
			addCloseButton(i); // 调用之前实现的添加按钮逻辑
		}

		// 3. 强制刷新 TabBar 状态（解决部分场景下按钮不显示问题）
		tabBar()->update();

		/*OCCTWidget* pOCCWidget = (OCCTWidget*)this->currentWidget();

		if (pOCCWidget!=nullptr)
		{
			pOCCWidget->get3dView()->FitAll();
			pOCCWidget->get3dView()->MustBeResized();
		}*/


	}

};

#endif // CLOSABLETABWIDGET_H