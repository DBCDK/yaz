/*
 * Copyright (C) 1995-2008, Index Data ApS
 * See the file LICENSE for details.
 *
 * $Id: siconv.c,v 1.50 2008-03-12 08:53:28 adam Exp $
 */
/**
 * \file
 * \brief ISO-5428 character mapping (iconv)
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "iconv-p.h"

unsigned long yaz_read_UCS4(yaz_iconv_t cd, unsigned char *inp,
                            size_t inbytesleft, size_t *no_read)
{
    unsigned long x = 0;
    
    if (inbytesleft < 4)
    {
        yaz_iconv_set_errno(cd, YAZ_ICONV_EINVAL); /* incomplete input */
        *no_read = 0;
    }
    else
    {
        x = (inp[0]<<24) | (inp[1]<<16) | (inp[2]<<8) | inp[3];
        *no_read = 4;
    }
    return x;
}

unsigned long yaz_read_UCS4LE(yaz_iconv_t cd, unsigned char *inp,
                              size_t inbytesleft, size_t *no_read)
{
    unsigned long x = 0;
    
    if (inbytesleft < 4)
    {
        yaz_iconv_set_errno(cd, YAZ_ICONV_EINVAL); /* incomplete input */
        *no_read = 0;
    }
    else
    {
        x = (inp[3]<<24) | (inp[2]<<16) | (inp[1]<<8) | inp[0];
        *no_read = 4;
    }
    return x;
}

size_t yaz_write_UCS4(yaz_iconv_t cd, unsigned long x,
                      char **outbuf, size_t *outbytesleft)
{
    unsigned char *outp = (unsigned char *) *outbuf;
    if (*outbytesleft >= 4)
    {
        *outp++ = (unsigned char) (x>>24);
        *outp++ = (unsigned char) (x>>16);
        *outp++ = (unsigned char) (x>>8);
        *outp++ = (unsigned char) x;
        (*outbytesleft) -= 4;
    }
    else
    {
        yaz_iconv_set_errno(cd, YAZ_ICONV_E2BIG);
        return (size_t)(-1);
    }
    *outbuf = (char *) outp;
    return 0;
}

size_t yaz_write_UCS4LE(yaz_iconv_t cd, unsigned long x,
                        char **outbuf, size_t *outbytesleft)
{
    unsigned char *outp = (unsigned char *) *outbuf;
    if (*outbytesleft >= 4)
    {
        *outp++ = (unsigned char) x;
        *outp++ = (unsigned char) (x>>8);
        *outp++ = (unsigned char) (x>>16);
        *outp++ = (unsigned char) (x>>24);
        (*outbytesleft) -= 4;
    }
    else
    {
        yaz_iconv_set_errno(cd, YAZ_ICONV_E2BIG);
        return (size_t)(-1);
    }
    *outbuf = (char *) outp;
    return 0;
}



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
