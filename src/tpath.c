/*
 * Copyright (C) 1995-2005, Index Data ApS
 * See the file LICENSE for details.
 *
 * $Id: tpath.c,v 1.6 2005-06-25 15:46:06 adam Exp $
 */
/**
 * \file tpath.c
 * \brief Implements path fopen
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <yaz/tpath.h>
#include <yaz/log.h>

FILE *yaz_path_fopen(const char *path, const char *name, const char *mode)
{
    return yaz_fopen (path, name, mode, 0);
}

int yaz_fclose (FILE *f)
{
    return fclose (f);
}

FILE *yaz_fopen(const char *path, const char *name, const char *mode,
                const char *base)
{
    char spath[1024];

    for(;;)
    {
        FILE *f;

        const char *path_sep = 0;
        size_t len = 0;
        size_t slen = 0;
        
        *spath = '\0';
        if (path)
        {
            /* somewhat dirty since we have to consider Windows
             * drive letters..
             */
            if (strchr ("/\\.", *path))
            {
                path_sep = strchr (path+1, ':');
            }
            else if (path[0] && path[1])
                path_sep = strchr (path+2, ':');
            if (path_sep)
                len = path_sep - path;
            else
                len = strlen(path);
            if (!strchr ("/\\", *path) && base)
            {
                strcpy (spath, base);
                slen = strlen(spath);
                spath[slen++] = '/';
            }
            memcpy (spath+slen, path, len);
            slen += len;
            if (!strchr("/\\", spath[slen-1]))
                spath[slen++] = '/';
        }
        strcpy (spath+slen, name);
        if ((f = fopen(spath, mode)))
            return f;
        
        if (!path_sep)
            break;
        path = path_sep+1;
    }
    return 0;
}

int yaz_is_abspath (const char *p)
{
    if (*p == '/')
        return 1;
#ifdef WIN32
    if (*p == '\\')
        return 1;
    if (*p && p[1] == ':' && isalpha(*p))
        return 1;
#endif
    return 0;
}
/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

