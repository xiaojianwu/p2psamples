#ifndef LIBPUNCH_GLOBAL_H
#define LIBPUNCH_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef LIBPUNCH_LIB
# define LIBPUNCH_EXPORT Q_DECL_EXPORT
#else
# define LIBPUNCH_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBPUNCH_GLOBAL_H
