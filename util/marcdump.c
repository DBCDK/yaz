/*
 * Copyright (c) 1995-2002, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Id: marcdump.c,v 1.16 2002-03-18 21:33:48 adam Exp $
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <yaz/wrbuf.h>
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
    fprintf (stderr, "Usage: %s [-c cfile] [-x] [-v] file...\n", prog);
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

    while ((r = options("vc:x", argv, argc, &arg)) != -2)
    {
	int count;
	no++;
        switch (r)
        {
	case 'c':
	    if (cfile)
		fclose (cfile);
	    cfile = fopen (arg, "w");
	    break;
        case 'x':
            xml = 1;
            break;
        case 0:
	    inf = fopen (arg, "r");
	    count = 0;
	    if (!inf)
	    {
		fprintf (stderr, "%s: cannot open %s:%s\n",
			 prog, arg, strerror (errno));
		exit (1);
	    }
	    if (cfile)
		fprintf (cfile, "char *marc_records[] = {\n");
	    while (1)
	    {
                WRBUF wr = wrbuf_alloc();

		int len;
		
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
                r = yaz_marc_decode (buf, wr, verbose, -1, xml);
		if (r <= 0)
		    break;
                fwrite (wrbuf_buf(wr), wrbuf_len(wr), 1, stdout);
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
		count++;
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
