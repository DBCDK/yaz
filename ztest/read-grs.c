/*
 * Copyright (c) 1995-1999, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: read-grs.c,v $
 * Revision 1.5  1999-11-30 13:47:12  adam
 * Improved installation. Moved header files to include/yaz.
 *
 * Revision 1.4  1999/08/27 09:40:32  adam
 * Renamed logf function to yaz_log. Removed VC++ project files.
 *
 * Revision 1.3  1999/03/31 11:18:25  adam
 * Implemented odr_strdup. Added Reference ID to backend server API.
 *
 * Revision 1.2  1998/02/11 11:53:36  adam
 * Changed code so that it compiles as C++.
 *
 * Revision 1.1  1997/09/01 08:55:53  adam
 * New windows NT/95 port using MSV5.0. Test server ztest now in
 * separate directory. When using NT, this test server may operate
 * as an NT service. Note that the service.[ch] should be part of
 * generic, but it isn't yet.
 *
 * Revision 1.1  1995/08/17 12:45:23  quinn
 * Fixed minor problems with GRS-1. Added support in c&s.
 *
 *
 */

/*
 * Little toy-thing to read a GRS-1 record from a file.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include <yaz/proto.h>
#include <yaz/log.h>

#define GRS_MAX_FIELDS 50

Z_GenericRecord *read_grs1(FILE *f, ODR o)
{
    char line[512], *buf;
    int type, ivalue;
    char value[512];
    Z_GenericRecord *r = 0;

    for (;;)
    {
	Z_TaggedElement *t;
	Z_ElementData *c;

	while (fgets(buf = line, 512, f))
	{
	    while (*buf && isspace(*buf))
		buf++;
	    if (!*buf || *buf == '#')
		continue;
	    break;
	}
	if (*buf == '}')
	    return r;
	if (sscanf(buf, "(%d,%[^)])", &type, value) != 2)
	{
	    yaz_log(LOG_WARN, "Bad data in '%s'", buf);
	    return 0;
	}
	if (!type && *value == '0')
	    return r;
	if (!(buf = strchr(buf, ')')))
	    return 0;
	buf++;
	while (*buf && isspace(*buf))
	    buf++;
	if (!*buf)
	    return 0;
	if (!r)
	{
	    r = (Z_GenericRecord *)odr_malloc(o, sizeof(*r));
	    r->elements = (Z_TaggedElement **)
                odr_malloc(o, sizeof(Z_TaggedElement*) * GRS_MAX_FIELDS);
	    r->num_elements = 0;
	}
	r->elements[r->num_elements] = t = (Z_TaggedElement *)
            odr_malloc(o, sizeof(Z_TaggedElement));
	t->tagType = (int *)odr_malloc(o, sizeof(int));
	*t->tagType = type;
	t->tagValue = (Z_StringOrNumeric *)
            odr_malloc(o, sizeof(Z_StringOrNumeric));
	if ((ivalue = atoi(value)))
	{
	    t->tagValue->which = Z_StringOrNumeric_numeric;
	    t->tagValue->u.numeric = (int *)odr_malloc(o, sizeof(int));
	    *t->tagValue->u.numeric = ivalue;
	}
	else
	{
	    t->tagValue->which = Z_StringOrNumeric_string;
	    t->tagValue->u.string = (char *)odr_malloc(o, strlen(value)+1);
	    strcpy(t->tagValue->u.string, value);
	}
	t->tagOccurrence = 0;
	t->metaData = 0;
	t->appliedVariant = 0;
	t->content = c = (Z_ElementData *)odr_malloc(o, sizeof(Z_ElementData));
	if (*buf == '{')
	{
	    c->which = Z_ElementData_subtree;
	    c->u.subtree = read_grs1(f, o);
	}
	else
	{
	    c->which = Z_ElementData_string;
	    buf[strlen(buf)-1] = '\0';
	    c->u.string = odr_strdup(o, buf);
	}
	r->num_elements++;
    }
}
