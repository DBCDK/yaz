/*
 * Copyright (c) 1995, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: d1_marc.c,v $
 * Revision 1.5  1997-09-04 13:48:04  adam
 * Added data1 to marc conversion.
 *
 * Revision 1.4  1996/03/25 10:18:03  quinn
 * Removed trailing whitespace from data elements
 *
 * Revision 1.3  1995/11/01  16:34:57  quinn
 * Making data1 look for tables in data1_tabpath
 *
 * Revision 1.2  1995/11/01  13:54:48  quinn
 * Minor adjustments
 *
 * Revision 1.1  1995/11/01  11:56:08  quinn
 * Added Retrieval (data management) functions en masse.
 *
 *
 */


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <oid.h>
#include <log.h>
#include <marcdisp.h>
#include <readconf.h>
#include <xmalloc.h>
#include <data1.h>
#include <tpath.h>

data1_marctab *data1_read_marctab(char *file)
{
    FILE *f;
    data1_marctab *res = xmalloc(sizeof(*res));
    char line[512], *argv[50];
    int argc;
    
    if (!(f = yaz_path_fopen(data1_tabpath, file, "r")))
    {
	logf(LOG_WARN|LOG_ERRNO, "%s", file);
	return 0;
    }

    res->name = 0;
    res->reference = VAL_NONE;
    res->next = 0;
    res->length_data_entry = 4;
    res->length_starting = 5;
    res->length_implementation = 0;
    strcpy(res->future_use, "4");

    strcpy(res->record_status, "n");
    strcpy(res->implementation_codes, "    ");
    res->indicator_length = 2;
    res->identifier_length = 2;
    strcpy(res->user_systems, "z  ");

    while ((argc = readconf_line(f, line, 512, argv, 50)))
	if (!strcmp(argv[0], "name"))
	{
	    if (argc != 2)
	    {
		logf(LOG_WARN, "%s: Bad name directive");
		continue;
	    }
	    res->name = xmalloc(strlen(argv[1])+1);
	    strcpy(res->name, argv[1]);
	}
	else if (!strcmp(argv[0], "reference"))
	{
	    if (argc != 2)
	    {
		logf(LOG_WARN, "%s: Bad name directive");
		continue;
	    }
	    if ((res->reference = oid_getvalbyname(argv[1])) == VAL_NONE)
	    {
		logf(LOG_WARN, "%s: Unknown tagset ref '%s' in %s", file,
		    argv[1]);
		continue;
	    }
	}
	else if (!strcmp(argv[0], "length-data-entry"))
	{
	    if (argc != 2)
	    {
		logf(LOG_WARN, "%s: Bad data-length-entry");
		continue;
	    }
	    res->length_data_entry = atoi(argv[1]);
	}
	else if (!strcmp(argv[0], "length-starting"))
	{
	    if (argc != 2)
	    {
		logf(LOG_WARN, "%s: Bad length-starting");
		continue;
	    }
	    res->length_starting = atoi(argv[1]);
	}
	else if (!strcmp(argv[0], "length-implementation"))
	{
	    if (argc != 2)
	    {
		logf(LOG_WARN, "%s: Bad length-implentation");
		continue;
	    }
	    res->length_implementation = atoi(argv[1]);
	}
	else if (!strcmp(argv[0], "future-use"))
	{
	    if (argc != 2)
	    {
		logf(LOG_WARN, "%s: Bad future-use");
		continue;
	    }
	    strncpy(res->future_use, argv[1], 2);
	}
	else
	    logf(LOG_WARN, "%s: Bad directive '%s'", file, argv[0]);

    fclose(f);
    return res;
}

/*
 * Locate some data under this node. This routine should handle variants
 * prettily.
 */
static char *get_data(data1_node *n, int *len)
{
    char *r;

    while (n->which != DATA1N_data && n->child)
	n = n->child;
    if (n->which != DATA1N_data || n->u.data.what != DATA1I_text)
    {
	r = "[Structured/included data]";
	*len = strlen(r);
	return r;
    }

    *len = n->u.data.len;
    while (*len && isspace(n->u.data.data[*len - 1]))
	(*len)--;
    return n->u.data.data;
}

static void memint (char *p, int val, int len)
{
    static char buf[9];

    if (len == 1)
        *p = val + '0';
    else
    {
        sprintf (buf, "%08d", val);
        memcpy (p, buf+8-len, len);
    }
}

static int is_indicator (data1_marctab *p, data1_node *subf)
{
#if 1
    if (p->indicator_length != 2 ||
	(subf->which == DATA1N_tag && strlen(subf->u.tag.tag) == 2))
	return 1;
#else
    if (subf->which == DATA1N_tag && subf->child->which == DATA1N_tag)
	return 1;
#endif
    return 0;
}

static int nodetomarc(data1_marctab *p, data1_node *n, int selected,
    char **buf, int *size)
{
    int len = 26;
    int dlen;
    int base_address = 25;
    int entry_p, data_p;
    char *op;
    data1_node *field, *subf;

    for (field = n->child; field; field = field->next)
    {
	if (field->which != DATA1N_tag)
	{
	    logf(LOG_WARN, "Malformed field composition for marc output.");
	    return -1;
	}
	if (selected && !field->u.tag.node_selected)
	    continue;
        len += 4 + p->length_data_entry + p->length_starting
            + p->length_implementation;
        base_address += 3 + p->length_data_entry + p->length_starting
            + p->length_implementation;
	if (strncmp(field->u.tag.tag, "00", 2))
            len += p->indicator_length;      /* this is fairly bogus */
	subf = field->child;
	
	/*  we'll allow no indicator if length is not 2 */
	if (is_indicator (p, subf))
	    subf = subf->child;

        for (; subf; subf = subf->next)
        {
	    if (subf->which != DATA1N_tag)
	    {
		logf(LOG_WARN,
		    "Malformed subfield composition for marc output.");
		return -1;
	    }
            if (strncmp(field->u.tag.tag, "00", 2))
                len += p->identifier_length;
	    get_data(subf, &dlen);
            len += dlen;
        }
    }

    if (!*buf)
	*buf = xmalloc(*size = len);
    else if (*size <= len)
	*buf = xrealloc(*buf, *size = len);
	
    op = *buf;
    memint (op, len, 5);
    memcpy (op+5, p->record_status, 1);
    memcpy (op+6, p->implementation_codes, 4);
    memint (op+10, p->indicator_length, 1);
    memint (op+11, p->identifier_length, 1);
    memint (op+12, base_address, 5);
    memcpy (op+17, p->user_systems, 3);
    memint (op+20, p->length_data_entry, 1);
    memint (op+21, p->length_starting, 1);
    memint (op+22, p->length_implementation, 1);
    memcpy (op+23, p->future_use, 1);
    
    entry_p = 24;
    data_p = base_address;

    for (field = n->child; field; field = field->next)
    {
        int data_0 = data_p;
	char *indicator_data = "    ";
	if (selected && !field->u.tag.node_selected)
	    continue;

	subf = field->child;

	if (is_indicator (p, subf))
	{
            indicator_data = subf->u.tag.tag;
	    subf = subf->child;
	}
        if (strncmp(field->u.tag.tag, "00", 2))   /* bogus */
        {
            memcpy (op + data_p, indicator_data, p->indicator_length);
            data_p += p->indicator_length;
        }
        for (; subf; subf = subf->next)
        {
	    char *data;

            if (strncmp(field->u.tag.tag, "00", 2))
            {
                op[data_p] = ISO2709_IDFS;
                memcpy (op + data_p+1, subf->u.tag.tag, p->identifier_length-1);
                data_p += p->identifier_length;
            }
	    data = get_data(subf, &dlen);
            memcpy (op + data_p, data, dlen);
            data_p += dlen;
        }
        op[data_p++] = ISO2709_FS;

        memcpy (op + entry_p, field->u.tag.tag, 3);
        entry_p += 3;
        memint (op + entry_p, data_p - data_0, p->length_data_entry);
        entry_p += p->length_data_entry;
        memint (op + entry_p, data_0 - base_address, p->length_starting);
        entry_p += p->length_starting;
        entry_p += p->length_implementation;
    }
    op[entry_p++] = ISO2709_FS;
    assert (entry_p == base_address);
    op[data_p++] = ISO2709_RS;
    assert (data_p == len);
    return len;
}

char *data1_nodetomarc(data1_marctab *p, data1_node *n, int selected, int *len)
{
    static char *buf = 0;
    static int size = 0;

    *len = nodetomarc(p, n, selected, &buf, &size);
    return buf;
}
