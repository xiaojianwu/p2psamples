#ifndef LIBPUNCH_H
#define LIBPUNCH_H

#include "libpunch_global.h"

class LIBPUNCH_EXPORT libPunch
{

public:
	static libPunch* instance();
	void init();

private:
	static libPunch* inst;

protected:
	libPunch();
	~libPunch();

public:
	void newSession(int direct);

private:

};

#endif // LIBPUNCH_H
