/*
 * Copyright (c) 1995-2002, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Id: marcdump.c,v 1.19 2002-12-16 13:13:53 adam Exp $
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif
#if HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include <yaz/marcdisp.h>
#include <yaz/yaz-util.h>
#include <yaz/xmalloc.h>
#include <yaz/options.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

static void usage(const char *prog)
{
    fprintf (stderr, "Usage: %s [-c cfile] [-f from] [-t to] [-x] [-O] [-X] [-v] file...\n",
             prog);
} 

int main (int argc, char **argv)
{
    int r;
    char *arg;
    int verbose = 0;
    FILE *inf;
    char buf[100001];
    char *prog = *argv;
    int no = 0;
    int xml = 0;
    FILE *cfile = 0;
    char *from = 0, *to = 0;

    
#if HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif
#if HAVE_LANGINFO_H
    to = nl_langinfo(CODESET);
#endif

    while ((r = options("vc:xOXf:t:", argv, argc, &arg)) != -2)
    {
	int count;
	no++;
        switch (r)
        {
        case 'f':
            from = arg;
            break;
        case 't':
            to = arg;
            break;
	case 'c':
	    if (cfile)
		fclose (cfile);
	    cfile = fopen (arg, "w");
	    break;
        case 'x':
            xml = YAZ_MARC_SIMPLEXML;
            break;
        case 'O':
            xml = YAZ_MARC_OAIMARC;
            break;
        case 'X':
            xml = YAZ_MARC_MARCXML;
            break;
        case 0:
	    inf = fopen (arg, "r");
	    count = 0;
	    if (!inf)
	    {
		fprintf (stderr, "%s: cannot open %s:%s\n",
			 prog, arg, strerror (errno));
		exit(1);
	    }
	    if (cfile)
		fprintf (cfile, "char *marc_records[] = {\n");
            if (1)
            {
                yaz_marc_t mt = yaz_marc_create();
                yaz_iconv_t cd = 0;

                if (from && to)
                {
                    cd = yaz_iconv_open(to, from);
                    if (!cd)
                    {
                        fprintf(stderr, "conversion from %s to %s "
                                "unsupported\n", from, to);
                        exit(2);
                    }
                }
                yaz_marc_xml(mt, xml);
                yaz_marc_debug(mt, verbose);
                while (1)
                {
                    int len;
                    char *result;
                    int rlen;
                    
                    r = fread (buf, 1, 5, inf);
                    if (r < 5)
                        break;
                    len = atoi_n(buf, 5);
                    if (len < 25 || len > 100000)
                        break;
                    len = len - 5;
                    r = fread (buf + 5, 1, len, inf);
                    if (r < len)
                        break;
                    r = yaz_marc_decode_buf (mt, buf, -1, &result, &rlen);
                    if (r <= 0)
                        break;
                    if (!cd)
                        fwrite (result, rlen, 1, stdout);
                    else
                    {
                        char outbuf[12];
                        size_t inbytesleft = rlen;
                        const char *inp = result;
                        
                        while (inbytesleft)
                        {
                            int i;
                            size_t outbytesleft = sizeof(outbuf);
                            char *outp = outbuf;
                            size_t r = yaz_iconv (cd, (char**) &inp,
                                                  &inbytesleft, 
                                                  &outp, &outbytesleft);
                            if (r == (size_t) (-1))
                            {
                                int e = yaz_iconv_error(cd);
                                if (e != YAZ_ICONV_E2BIG)
                                    break;
                            }
                            fwrite (outbuf, outp - outbuf, 1, stdout);
                        }
                    }

                    if (cfile)
                    {
                        char *p = buf;
                        int i;
                        if (count)
                            fprintf (cfile, ",");
                        fprintf (cfile, "\n");
                        for (i = 0; i < r; i++)
                        {
                            if ((i & 15) == 0)
                                fprintf (cfile, "  \"");
                            fprintf (cfile, "\\x%02X", p[i] & 255);
                            
                            if (i < r - 1 && (i & 15) == 15)
                                fprintf (cfile, "\"\n");
                            
			}
                        fprintf (cfile, "\"\n");
                    }
                }
                count++;
                if (cd)
                    yaz_iconv_close(cd);
	    }
	    if (cfile)
		fprintf (cfile, "};\n");
            break;
        case 'v':
	    verbose++;
            break;
        default:
            usage(prog);
            exit (1);
        }
    }
    if (cfile)
	fclose (cfile);
    if (!no)
    {
        usage(prog);
	exit (1);
    }
    exit (0);
}
