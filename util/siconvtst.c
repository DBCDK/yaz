/*
 * Copyright (c) 1997-2002, Index Data
 * See the file LICENSE for details.
 *
 * $Id: siconvtst.c,v 1.4 2002-08-28 19:33:53 adam Exp $
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <yaz/yaz-util.h>

#define CHUNK 8

void convert (FILE *inf, yaz_iconv_t cd)
{
    char inbuf0[CHUNK], *inbuf = inbuf0;
    char outbuf0[CHUNK], *outbuf = outbuf0;
    size_t outbytesleft = CHUNK;
    size_t inbytesleft = CHUNK;
    int mustread = 1;

    while (1)
    {
        size_t r;
        if (mustread)
        {
            r = fread (inbuf, 1, inbytesleft, inf);
            if (inbytesleft != r)
            {
                if (ferror(inf))
                {
                    fprintf (stderr, "yaziconv: error reading file\n");
                    exit (6);
                }
                if (r == 0)
                {
                    if (outbuf != outbuf0)
                        fwrite (outbuf0, 1, outbuf - outbuf0, stdout);
                    break;
                }
                inbytesleft = r;
            }
        }
        r = yaz_iconv (cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        if (r == (size_t)(-1))
        {
            int e = yaz_iconv_error(cd);
            if (e == YAZ_ICONV_EILSEQ)
            {
                fprintf (stderr, "invalid sequence\n");
                return ;
            }
            else if (e == YAZ_ICONV_EINVAL) /* incomplete input */
            { 
                size_t i;
                for (i = 0; i<inbytesleft; i++)
                    inbuf0[i] = inbuf[i];
                inbuf = inbuf0 + i;
                inbytesleft = CHUNK - inbytesleft;
                mustread = 1;
            }
            else if (e == YAZ_ICONV_E2BIG) /* no more output space */
            {
                fwrite (outbuf0, 1, outbuf - outbuf0, stdout);
                outbuf = outbuf0;
                outbytesleft = CHUNK;
                mustread = 0;
            }
            else
            {
                fprintf (stderr, "yaziconv: unknown error\n");
                exit (7);
            }
        }
        else
        {
            inbuf = inbuf0;
            inbytesleft = CHUNK;

            fwrite (outbuf0, 1, outbuf - outbuf0, stdout);
            outbuf = outbuf0;
            outbytesleft = CHUNK;

            mustread = 1;
        }
    }
}

int main (int argc, char **argv)
{
    int ret;
    char *from = 0;
    char *to = 0;
    char *arg;
    yaz_iconv_t cd;
    FILE *inf = stdin;

    while ((ret = options ("f:t:", argv, argc, &arg)) != -2)
    {
        switch (ret)
        {
        case 0:
            inf = fopen (arg, "rb");
            if (!inf)
            {
                fprintf (stderr, "yaziconv: cannot open %s", arg);
                exit (2);
            }
            break;
        case 'f':
            from = arg;
            break;
        case 't':
            to = arg;
            break;
        default:
            fprintf (stderr, "yaziconv: Usage\n"
                     "siconv -f encoding -t encoding [file]\n");
            exit(1);
        }
    }
    if (!to)
    {
        fprintf (stderr, "yaziconv: -t encoding missing\n");
        exit (3);
    }
    if (!from)
    {
        fprintf (stderr, "yaziconv: -f encoding missing\n");
        exit (4);
    }
    cd = yaz_iconv_open (to, from);
    if (!cd)
    {
        fprintf (stderr, "yaziconv: unsupported encoding\n");
        exit (5);
    }

    convert (inf, cd);
    yaz_iconv_close (cd);
    return 0;
}
