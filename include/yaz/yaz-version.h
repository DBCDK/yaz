/*
 * Copyright (c) 1995-2004, Index Data.
 * See the file LICENSE for details.
 *
 * $Id: yaz-version.h,v 1.43 2004-03-20 07:02:22 adam Exp $
 */

/*
 * Current software version.
 */
#ifndef YAZ_VERSION

#include <yaz/yconfig.h>

#define YAZ_VERSION "2.0.18"
#define YAZ_VERSIONL 0x020012

#define YAZ_DATE 1

#ifdef WIN32
#ifdef NDEBUG
#define YAZ_OS "WIN32 Release"
#else
#define YAZ_OS "WIN32 Debug"
#endif
#endif

YAZ_BEGIN_CDECL

YAZ_EXPORT unsigned long yaz_version(char *version_str, char *sys_str);

YAZ_END_CDECL

#endif

