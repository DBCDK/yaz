/* $Id: cqlstring.c,v 1.2 2004-10-03 22:34:07 adam Exp $
   Copyright (C) 2002-2004
   Index Data Aps

This file is part of the YAZ toolkit.

See the file LICENSE for details.
*/

/**
 * \file cqlstring.c
 * \brief Implements query stream reader that reads from a C string.
 */


#include <yaz/cql.h>

struct cql_buf_info {
    const char *str;
    int off;
};

int getbuf(void *vp)
{
    struct cql_buf_info *bi = (struct cql_buf_info *) vp;
    if (bi->str[bi->off] == 0)
        return 0;
    return bi->str[bi->off++];
}

void ungetbuf(int b, void *vp)
{
    struct cql_buf_info *bi = (struct cql_buf_info *) vp;
    if (b)
        (bi->off--);
}

int cql_parser_string(CQL_parser cp, const char *str)
{
    struct cql_buf_info b;

    b.str = str;
    b.off = 0;
    
    return cql_parser_stream(cp, getbuf, ungetbuf, &b);
}

