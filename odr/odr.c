/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: odr.c,v $
 * Revision 1.3  1995-03-07 10:21:31  quinn
 * odr_errno-->odr_error
 *
 * Revision 1.2  1995/03/07  10:19:05  quinn
 * Addded some method functions to the ODR type.
 *
 * Revision 1.1  1995/03/07  09:23:15  quinn
 * Installing top-level API and documentation.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <odr.h>

char *odr_errlist[] =
{
    "No (unknown) error",
    "Memoy allocation failed",
    "System error",
    "No space in buffer",
    "Required data element missing",
    "Unexpected tag",
    "Other error"
};

void odr_perror(ODR o, char *message)
{
    fprintf(stderr, "%s: %s\n", message, odr_errlist[o->error]);
}

int odr_error(ODR o)
{
    return o->error;
}

void odr_setprint(ODR o, FILE *file)
{
    o->print = file;
}

ODR odr_createmem(int direction)
{
    struct odr *r;

    if (!(r = malloc(sizeof(*r))))
    	return 0;
    r->direction = direction;
    r->print = stdout;
    r->buf = 0;
    r->buflen = 0;
    odr_reset(r);
    return r;
}

void odr_reset(ODR o)
{
    o->error = ONONE;
    o->bp = 0;
    o->left = o->buflen;
    o->t_class = -1;
    o->t_tag = -1;
    o->indent = 0;
    o->stackp = 0;
}
    
void odr_destroy(ODR o)
{
    free(o);
}

void odr_setbuf(ODR o, char *buf, int len)
{
    o->buf = o->bp = (unsigned char *) buf;
    o->buflen = o->left = len;
}

char *odr_getbuf(ODR o, int *len)
{
    *len = o->bp - o->buf;
    return (char *) o->buf;
}    
