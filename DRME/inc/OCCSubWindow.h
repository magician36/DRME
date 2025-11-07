#pragma once
#include <QtWidgets/QMdiSubWindow>
#include "OCCTWidget.h"

using namespace std;

//´°¿Ú¹ÜÀí
class OCCSubWindow : public QMdiSubWindow {
public:

	bool bInitialize = false;

	void showEvent(QShowEvent* showEvent)
	{
		//OCCTWidget* pOCCWidget = (OCCTWidget*)this->widget();

		//if (bIntialize)
		//{
		//	//pOCCWidget->get3dView()->FitAll();
		//	pOCCWidget->get3dView()->MustBeResized();
		//}


	}

	void closeEvent(QCloseEvent* closeEvent)
	{
		QMdiArea* mdiArea = this->mdiArea();
		mdiArea->removeSubWindow(this);
		this->deleteLater();

		printf("close\n");

		OCCSubWindow* pOCCWindow = (OCCSubWindow*)mdiArea->currentSubWindow();

		if (pOCCWindow != nullptr)
		{
			pOCCWindow->showMaximized();

			OCCTWidget* pOCCWidget = (OCCTWidget*)pOCCWindow->widget();

			if (pOCCWindow->bInitialize)
			{
				//pOCCWidget->get3dView()->FitAll();
				pOCCWidget->get3dView()->MustBeResized();
			}
		}
	}


};


