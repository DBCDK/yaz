/*
 * Copyright (c) 1995-1997, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * NT Service interface by
 *    Chas Woodfield, Fretwell Downing Datasystems.
 *
 * $Log: ztest.c,v $
 * Revision 1.5  1997-11-07 13:31:58  adam
 * Added NT Service name part of statserv_options_block. Moved NT
 * service utility to server library.
 *
 * Revision 1.4  1997/09/17 12:10:43  adam
 * YAZ version 1.4.
 *
 * Revision 1.3  1997/09/09 10:10:20  adam
 * Another MSV5.0 port. Changed projects to include proper
 * library/include paths.
 * Server starts server in test-mode when no options are given.
 *
 * Revision 1.2  1997/09/04 13:50:31  adam
 * Bug fix in ztest.
 *
 */

/*
 * Demonstration of simple server
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include <backend.h>
#include <xmalloc.h>
#include <proto.h>

Z_GenericRecord *read_grs1(FILE *f, ODR o);

bend_initresult *bend_init(bend_initrequest *q)
{
    bend_initresult *r = odr_malloc (q->stream, sizeof(*r));
    static char *dummy = "Hej fister";

    r->errcode = 0;
    r->errstring = 0;
    r->handle = dummy;
    return r;
}

bend_searchresult *bend_search(void *handle, bend_searchrequest *q, int *fd)
{
    bend_searchresult *r = odr_malloc (q->stream, sizeof(*r));

    r->errcode = 0;
    r->errstring = 0;
    r->hits = rand() % 22;

    return r;
}

static int atoin (const char *buf, int n)
{
    int val = 0;
    while (--n >= 0)
    {
        if (isdigit(*buf))
            val = val*10 + (*buf - '0');
        buf++;
    }
    return val;
}

char *marc_read(FILE *inf)
{
    char length_str[5];
    size_t size;
    char *buf;

    if (fread (length_str, 1, 5, inf) != 5)
        return NULL;
    size = atoin (length_str, 5);
    if (size <= 6)
        return NULL;
    if (!(buf = xmalloc (size+1)))
        return NULL;
    if (fread (buf+5, 1, size-5, inf) != (size-5))
    {
        xfree (buf);
        return NULL;
    }
    memcpy (buf, length_str, 5);
    buf[size] = '\0';
    return buf;
}

static char *dummy_database_record (int num)
{
    FILE *inf = fopen ("dummy-records", "r");
    char *buf = 0;

    if (!inf)
	return NULL;
    while (--num >= 0)
    {
	if (buf)
	   xfree(buf);
	if (num == 98)
	{
	    assert(buf = xmalloc(2101));
	    memset(buf, 'A', 2100);
	    buf[2100] = '\0';
	    break;
	}
	else
	    buf = marc_read (inf);
	if (!num || !buf)
	    break;
    }
    fclose(inf);
    if (num < 0)
    	return 0;
    return buf;
}

static Z_GenericRecord *dummy_grs_record (int num, ODR o)
{
    FILE *f = fopen("dummy-grs", "r");
    char line[512];
    Z_GenericRecord *r = 0;
    int n;

    if (!f)
	return 0;
    while (fgets(line, 512, f))
	if (*line == '#' && sscanf(line, "#%d", &n) == 1 && n == num)
	{
	    r = read_grs1(f, o);
	    break;
	}
    fclose(f);
    return r;
}

bend_fetchresult *bend_fetch(void *handle, bend_fetchrequest *q, int *num)
{
    bend_fetchresult *r = odr_malloc (q->stream, sizeof(*r));
    static char *bbb = 0;

    r->errstring = 0;
    r->last_in_set = 0;
    r->basename = "DUMMY";
    if (bbb)
    {
        xfree(bbb);
	bbb = 0;
    }
    r->format = q->format;  
    if (q->format == VAL_SUTRS)
    {
    	char buf[100];

	sprintf(buf, "This is dummy SUTRS record number %d\n", q->number);
	assert(r->record = bbb = xmalloc(strlen(buf)+1));
	strcpy(bbb, buf);
	r->len = strlen(buf);
    }
    else if (q->format == VAL_GRS1)
    {
	r->len = -1;
	r->record = (char*) dummy_grs_record(q->number, q->stream);
	if (!r->record)
	{
	    r->errcode = 13;
	    return r;
	}
    }
    else if (!(r->record = bbb = dummy_database_record(q->number)))
    {
    	r->errcode = 13;
	r->format = VAL_USMARC;
	return r;
    }
    else
	r->len = strlen(r->record);
    r->errcode = 0;
    return r;
}

bend_deleteresult *bend_delete(void *handle, bend_deleterequest *q, int *num)
{
    return 0;
}

#if 0
bend_scanresult *bend_scan(void *handle, bend_scanrequest *q, int *num)
{
    static struct scan_entry list[200];
    static char buf[200][200];
    static bend_scanresult r;
    int i;

    r.term_position = q->term_position;
    r.num_entries = q->num_entries;
    r.entries = list;
    for (i = 0; i < r.num_entries; i++)
    {
    	list[i].term = buf[i];
	sprintf(list[i].term, "term-%d", i+1);
	list[i].occurrences = rand() % 100000;
    }
    r.errcode = 0;
    r.errstring = 0;
    return &r;
}
#else
/*
 * silly dummy-scan what reads words from a file.
 */
bend_scanresult *bend_scan(void *handle, bend_scanrequest *q, int *num)
{
    bend_scanresult *r = odr_malloc (q->stream, sizeof(*r));
    static FILE *f = 0;
    static struct scan_entry list[200];
    static char entries[200][80];
    int hits[200];
    char term[80], *p;
    int i, pos;

    r->errstring = 0;
    r->entries = list;
    r->status = BEND_SCAN_SUCCESS;
    if (!f && !(f = fopen("dummy-words", "r")))
    {
	perror("dummy-words");
	exit(1);
    }
    if (q->term->term->which != Z_Term_general)
    {
    	r->errcode = 229; /* unsupported term type */
	return r;
    }
    if (q->term->term->u.general->len >= 80)
    {
    	r->errcode = 11; /* term too long */
	return r;
    }
    if (q->num_entries > 200)
    {
    	r->errcode = 31;
	return r;
    }
    memcpy(term, q->term->term->u.general->buf, q->term->term->u.general->len);
    term[q->term->term->u.general->len] = '\0';
    for (p = term; *p; p++)
    	if (islower(*p))
	    *p = toupper(*p);

    fseek(f, 0, 0);
    r->num_entries = 0;
    for (i = 0, pos = 0; fscanf(f, " %79[^:]:%d", entries[pos], &hits[pos]) == 2;
	i++, pos < 199 ? pos++ : (pos = 0))
    {
    	if (!r->num_entries && strcmp(entries[pos], term) >= 0) /* s-point fnd */
	{
	    if ((r->term_position = q->term_position) > i + 1)
	    {
	    	r->term_position = i + 1;
		r->status = BEND_SCAN_PARTIAL;
	    }
	    for (; r->num_entries < r->term_position; r->num_entries++)
	    {
	    	int po;

		po = pos - r->term_position + r->num_entries + 1; /* find pos */
		if (po < 0)
		    po += 200;
		list[r->num_entries].term = entries[po];
		list[r->num_entries].occurrences = hits[po];
	    }
	}
	else if (r->num_entries)
	{
	    list[r->num_entries].term = entries[pos];
	    list[r->num_entries].occurrences = hits[pos];
	    r->num_entries++;
	}
	if (r->num_entries >= q->num_entries)
	    break;
    }
    if (feof(f))
    	r->status = BEND_SCAN_PARTIAL;
    return r;
}

#endif

void bend_close(void *handle)
{
    return;
}

int main(int argc, char **argv)
{
    return statserv_main(argc, argv);
}
