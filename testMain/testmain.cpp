#include "testmain.h"

#include "libpunch.h"

testMain::testMain(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	connect(ui.pushButton_O, SIGNAL(clicked()), this, SLOT(onInit()));
}

testMain::~testMain()
{

}

void testMain::onInit()
{
	libPunch::instance()->init();
}
