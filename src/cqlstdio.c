/* $Id: cqlstdio.c,v 1.2 2004-10-03 22:34:07 adam Exp $
   Copyright (C) 2002-2004
   Index Data Aps

This file is part of the YAZ toolkit.

See the file LICENSE for details.
*/

/**
 * \file cqlstdio.c
 * \brief Implements query stream reading using FILE handle.
 */

#include <yaz/cql.h>

int getbyte_stream(void *client_data)
{
    FILE *f = (FILE*) client_data;

    int c = fgetc(f);
    if (c == EOF)
        return 0;
    return c;
}

void ungetbyte_stream (int c, void *client_data)
{
    FILE *f = (FILE*) client_data;

    if (c == 0)
        c = EOF;
    ungetc(c, f);
}

int cql_parser_stdio(CQL_parser cp, FILE *f)
{
    return cql_parser_stream(cp, getbyte_stream, ungetbyte_stream, f);
}


