#ifndef TESTMAIN_H
#define TESTMAIN_H

#include <QtWidgets/QMainWindow>
#include "ui_testmain.h"

class testMain : public QMainWindow
{
	Q_OBJECT

public:
	testMain(QWidget *parent = 0);
	~testMain();


public slots:
	void onInit();

private:
	Ui::testMainClass ui;
};

#endif // TESTMAIN_H
