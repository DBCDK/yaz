/*
 * Copyright (c) 1995-1998, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: d1_attset.c,v $
 * Revision 1.9  1998-05-18 13:07:03  adam
 * Changed the way attribute sets are handled by the retriaval module.
 * Extended Explain conversion / schema.
 * Modified server and client to work with ASN.1 compiled protocol handlers.
 *
 * Revision 1.8  1998/02/11 11:53:35  adam
 * Changed code so that it compiles as C++.
 *
 * Revision 1.7  1997/09/17 12:10:34  adam
 * YAZ version 1.4.
 *
 * Revision 1.6  1997/09/05 09:50:56  adam
 * Removed global data1_tabpath - uses data1_get_tabpath() instead.
 *
 * Revision 1.5  1996/05/09 07:27:43  quinn
 * Multiple local attributes values supported.
 *
 * Revision 1.4  1996/02/21  15:23:36  quinn
 * Reversed fclose and return;
 *
 * Revision 1.3  1995/12/13  17:14:26  quinn
 * *** empty log message ***
 *
 * Revision 1.2  1995/11/01  16:34:55  quinn
 * Making data1 look for tables in data1_tabpath
 *
 * Revision 1.1  1995/11/01  11:56:07  quinn
 * Added Retrieval (data management) functions en masse.
 *
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <log.h>
#include <d1_attset.h>
#include <data1.h>
#include <tpath.h>

data1_att *data1_getattbyname(data1_handle dh, data1_attset *s, char *name)
{
    data1_att *r;
    data1_attset_child *c;
    
    /* scan local set */
    for (r = s->atts; r; r = r->next)
	if (!data1_matchstr(r->name, name))
	    return r;
    for (c = s->children; c; c = c->next)
    {
	assert (c->child);
	/* scan included sets */
	if ((r = data1_getattbyname (dh, c->child, name)))
	    return r;
    }
    return 0;
}

data1_attset *data1_read_attset(data1_handle dh, const char *file)
{
    char line[512], *r, cmd[512], args[512];
    data1_attset *res = 0;
    data1_attset_child **childp;
    data1_att **attp;
    FILE *f;
    NMEM mem = data1_nmem_get (dh);

    if (!(f = yaz_path_fopen(data1_get_tabpath(dh), file, "r")))
	return NULL;
    res = (data1_attset *)nmem_malloc(mem, sizeof(*res));
    res->name = 0;
    res->reference = VAL_NONE;
    res->atts = 0;
    res->children = 0;
    childp = &res->children;
    attp = &res->atts;

    for (;;)
    {
	while ((r = fgets(line, 512, f)))
	{
	    while (*r && isspace(*r))
		r++;
	    if (*r && *r != '#')
		break;
	}
	if (!r)
	{
	    fclose(f);
	    return res;
	}
	if (sscanf(r, "%s %[^\n]", cmd, args) < 2)
	    *args = '\0';
	if (!strcmp(cmd, "att"))
	{
	    int num, rr;
	    char name[512], localstr[512];
	    data1_att *t;
	    data1_local_attribute *locals;

	    if ((rr = sscanf(args, "%511d %s %511s", &num, name, localstr)) < 2)
	    {
		logf(LOG_WARN, "Not enough arguments to att in '%s' in %s",
		    args, file);
		fclose(f);
		return 0;
	    }
	    if (rr < 3) /* no local attributes given */
	    {
		locals = (data1_local_attribute *)nmem_malloc(mem, sizeof(*locals));
		locals->local = num;
		locals->next = 0;
	    }
	    else /* parse the string "local{,local}" */
	    {
		char *p = localstr;
		data1_local_attribute **ap = &locals;
		do
		{
		    *ap = (data1_local_attribute *)nmem_malloc(mem, sizeof(**ap));
		    (*ap)->local = atoi(p);
		    (*ap)->next = 0;
		    ap = &(*ap)->next;
		}
		while ((p = strchr(p, ',')) && *(++p));
	    }
	    t = *attp = (data1_att *)nmem_malloc(mem, sizeof(*t));
	    t->parent = res;
	    t->name = nmem_strdup(mem, name);
	    t->value = num;
	    t->locals = locals;
	    t->next = 0;
	    attp = &t->next;
	}
	else if (!strcmp(cmd, "name"))
	{
	    char name[512];

	    if (!sscanf(args, "%s", name))
	    {
		logf(LOG_WARN, "%s malformed name directive in %s", file);
		fclose(f);
		return 0;
	    }
	}
	else if (!strcmp(cmd, "reference"))
	{
	    char name[512];

	    if (!sscanf(args, "%s", name))
	    {
		logf(LOG_WARN, "%s malformed reference directive in %s", file);
		fclose(f);
		return 0;
	    }
	    if ((res->reference = oid_getvalbyname(name)) == VAL_NONE)
	    {
		logf(LOG_WARN, "Unknown attset name '%s' in %s", name, file);
		fclose(f);
		return 0;
	    }
	}
	else if (!strcmp(cmd, "ordinal"))
	{
	    int dummy;
	    if (!sscanf(args, "%d", &dummy))
	    {
		logf(LOG_WARN, "%s malformed ordinal directive in %s", file);
		fclose(f);
		return 0;
	    }
	}
	else if (!strcmp(cmd, "include"))
	{
	    char name[512];

	    if (!sscanf(args, "%s", name))
	    {
		logf(LOG_WARN, "%s malformed reference directive in %s", file);
		fclose(f);
		return 0;
	    }
	    *childp = (data1_attset_child *)
		nmem_malloc (mem, sizeof(**childp));
	    if (!((*childp)->child = data1_get_attset (dh, name)))
	    {
		logf(LOG_WARN, "Inclusion failed in %s", file);
		fclose(f);
		return 0;
	    }
	    (*childp)->next = 0;
	    childp = &(*childp)->next;
	}
	else
	{
	    logf(LOG_WARN, "Unknown directive '%s' in %s", cmd, file);
	    fclose(f);
	    return 0;
	}
    }
}
