/*
 * Copyright (c) 1995-2004, Index Data.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation, in whole or in part, for any purpose, is hereby granted,
 * provided that:
 *
 * 1. This copyright and permission notice appear in all copies of the
 * software and its documentation. Notices of copyright or attribution
 * which appear at the beginning of any file must remain unchanged.
 *
 * 2. The name of Index Data or the individual authors may not be used to
 * endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED, OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL INDEX DATA BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR
 * NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * $Id: log.h,v 1.17 2004-11-03 22:33:17 adam Exp $
 */

/**
 * \file log.h
 * \brief Header for logging utility
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <yaz/yconfig.h>
#include <yaz/xmalloc.h>

YAZ_BEGIN_CDECL

#define LOG_FATAL  0x00000001
#define LOG_DEBUG  0x00000002
#define LOG_WARN   0x00000004
#define LOG_LOG    0x00000008
#define LOG_ERRNO  0x00000010 /* append strerror to message */
#define LOG_FILE   0x00000020
#define LOG_APP    0x00000040 /* Application level events (new-connection) */
#define LOG_MALLOC 0x00000080 /* debugging mallocs */
#define LOG_NOTIME 0x00000100 /* do not output date and time */
#define LOG_APP2   0x00000200 /* Application-level events, such as api calls */
#define LOG_APP3   0x00000400 /* For more application-level events */
#define LOG_FLUSH  0x00000800 /* Flush log after every write (DEBUG does too) */

#define LOG_ALL   (0xffff&~LOG_MALLOC&~LOG_NOTIME)

#define LOG_DEFAULT_LEVEL (LOG_FATAL | LOG_ERRNO | LOG_LOG | LOG_WARN)

#define LOG_LAST_BIT LOG_FLUSH  /* the last bit used for regular log bits */
                                /* the rest are for dynamic modules */

#define logf yaz_log

/** 
 * yaz_log_init is a shorthand for initializing the log level and prefixes */
YAZ_EXPORT void yaz_log_init(int level, const char *prefix, const char *name);

/** yaz_log_init_file sets the file name used for yaz_log */
YAZ_EXPORT void yaz_log_init_file(const char *fname);

/** yaz_log_init_level sets the logging level. Use an OR of the bits above */
YAZ_EXPORT void yaz_log_init_level(int level);

/** yaz_log_init_prefix sets the log prefix */
YAZ_EXPORT void yaz_log_init_prefix(const char *prefix);

/** yaz_log_init_prefix2 sets an optional second prefix */
YAZ_EXPORT void yaz_log_init_prefix2(const char *prefix);

/** 
 * yaz_log_time_format sets the format of the timestamp. See man 3 strftime 
 * Calling with "old" sets to the old format "11:55:06-02/11"
 * Calling with NULL or "" sets to the new format "20041102-115719"
 * If not called at all, the old format is used, for backward compatibility
 */
YAZ_EXPORT void yaz_log_time_format(const char *fmt);

/** 
 * yaz_log_init_max_size sets the max size for a log file. 
 * zero means no limit. Negative means built-in limit (1GB)
 */
YAZ_EXPORT void yaz_log_init_max_size(int mx);

/**
 * yaz_log writes an entry in the log. Defaults to stderr if not initialized
 * to a file with yaz_log_init_file. The level must match the level set via
 * yaz_log_init_level, optionally defined via yaz_log_mask_str. */
YAZ_EXPORT void yaz_log(int level, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 3)))
#endif
	;

/** 
 * yaz_log_mask_str converts a comma-separated list of log levels to a bit 
 * mask. Starts from default level, and adds bits as specified, unless 'none'
 * is specified, which clears the list. If a name matches the name of a
 * LOG_BIT above, that one is set. Otherwise a new value is picked, and given
 * to that name, to be found with yaz_log_module_level */
YAZ_EXPORT int yaz_log_mask_str(const char *str);

/** yaz_log_mask_str_x is like yaz_log_mask_str, but with a given start value*/
YAZ_EXPORT int yaz_log_mask_str_x(const char *str, int level);

/** 
 * yaz_log_module_level returns a log level mask corresponding to the module
 * name. If that had been specified on the -v arguments (that is, passed to
 * yaz_log_mask_str), then a non-zero mask is returned. If not, we get a 
 * zero. This can later be used in yaz_log for the level argument
 */
YAZ_EXPORT int yaz_log_module_level(const char *name);

/** yaz_log_file returns the file handle for yaz_log. */
YAZ_EXPORT FILE *yaz_log_file(void);

YAZ_EXPORT void log_event_start(void (*func)(int level, const char *msg, void *info),
	void *info);
YAZ_EXPORT void log_event_end(void (*func)(int level, const char *msg, void *info),
	void *info);

YAZ_EXPORT void yaz_log_reopen(void);
YAZ_END_CDECL

#endif
