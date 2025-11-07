#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QDesktopWidget>
#include <QtCore/qnamespace.h>
#include <QtCore/QDebug>

#include <QtWidgets/QOpenGLWidget>
#include <QtWidgets/QWidget>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>

#include <QtGui/QGuiApplication>

#include "MainWindow_OSG.h"

QApplication* app;

#pragma warning( disable : 4996 )

int main(int argc, char *argv[])
{
	QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");

	QCoreApplication::addLibraryPath(QStringLiteral("C:\Qt\Qt5.14.2\5.14.2\msvc2015_64\plugins"));

	app = new QApplication(argc, argv);

	QMainWindow* Form = new QMainWindow();

	Ui_MainWindow* main_win = new Ui_MainWindow();

	main_win->setupUi(Form);
	
	main_win->show(Form);

	app->exec();

	return 1;
}
