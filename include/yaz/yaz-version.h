/*
 * Current software version.
 *
 * $Id: yaz-version.h,v 1.16 2002-05-22 14:27:17 adam Exp $
 */
#ifndef YAZ_VERSION

#define YAZ_VERSION "1.8.8dev"
#define YAZ_VERSIONL 0x010808

#define YAZ_DATE 1

#ifdef WIN32
#ifdef NDEBUG
#define YAZ_OS "WIN32 Release"
#else
#define YAZ_OS "WIN32 Debug"
#endif
#endif

#endif

