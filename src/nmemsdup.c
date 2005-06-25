/*
 * Copyright (C) 1995-2005, Index Data ApS
 * See the file LICENSE for details.
 *
 * $Id: nmemsdup.c,v 1.5 2005-06-25 15:46:04 adam Exp $
 */

/**
 * \file nmemsdup.c
 * \brief Implements NMEM dup utilities
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <yaz/nmem.h>

char *nmem_strdup (NMEM mem, const char *src)
{
    char *dst = (char *)nmem_malloc (mem, strlen(src)+1);
    strcpy (dst, src);
    return dst;
}

char *nmem_strdupn (NMEM mem, const char *src, size_t n)
{
    char *dst = (char *)nmem_malloc (mem, n+1);
    memcpy (dst, src, n);
    dst[n] = '\0';
    return dst;
}

int *nmem_intdup(NMEM mem, int v)
{
    int *dst = (int*) nmem_malloc (mem, sizeof(int));
    *dst = v;
    return dst;
}

void nmem_strsplit_blank(NMEM nmem, const char *dstr, char ***darray, int *num)
{
    nmem_strsplit(nmem, " ", dstr, darray, num);
}

void nmem_strsplit(NMEM nmem, const char *delim, const char *dstr,
                   char ***darray, int *num)
{
    const char *cp = dstr;
    for (*num = 0; *cp; (*num)++)
    {
        while (*cp && strchr(delim, *cp))
            cp++;
        if (!*cp)
            break;
        while (*cp && !strchr(delim, *cp))
            cp++;
    }
    if (!*num)
        *darray = 0;
    else
    {
        size_t i = 0;
        *darray = nmem_malloc(nmem, *num * sizeof(**darray));
        for (cp = dstr; *cp; )
        {
            const char *cp0;
            while (*cp && strchr(delim, *cp))
                cp++;
            if (!*cp)
                break;
            cp0 = cp;
            while (*cp && !strchr(delim, *cp))
                cp++;
            (*darray)[i++] = nmem_strdupn(nmem, cp0, cp - cp0);
        }
    }
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

