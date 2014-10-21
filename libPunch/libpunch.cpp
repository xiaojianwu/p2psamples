#include "libpunch.h"

#include "icesession.h"

#include <QEventLoop>

libPunch::libPunch()
{

}

libPunch::~libPunch()
{

}


libPunch *libPunch::inst = 0;
libPunch *libPunch::instance()
{
	if (!inst)
	{
		inst = new libPunch;
		ICESession::StaticInit();
	}
	return inst;
}

void libPunch::init()
{

	ICESession ice;
	ice.Init(ICESession::DirectionControlling);

	QEventLoop loop;
	QObject::connect(&ice,
		SIGNAL(CandidatesAvailable(const SIPInfo&)),
		&loop, SLOT(quit()));
	loop.exec();

	const SIPInfo& candidates = ice.candidates();
	qDebug() << candidates;
}