/*
 * Copyright (c) 1995-2003, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Id: odr_bool.c,v 1.1 2003-10-27 12:21:33 adam Exp $
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "odr-priv.h"

/*
 * Top level boolean en/decoder.
 * Returns 1 on success, 0 on error.
 */
int odr_bool(ODR o, int **p, int opt, const char *name)
{
    int res, cons = 0;

    if (o->error)
    	return 0;
    if (o->t_class < 0)
    {
    	o->t_class = ODR_UNIVERSAL;
    	o->t_tag = ODR_BOOLEAN;
    }
    if ((res = ber_tag(o, p, o->t_class, o->t_tag, &cons, opt, name)) < 0)
    	return 0;
    if (!res)
    	return odr_missing(o, opt, name);
    if (o->direction == ODR_PRINT)
    {
	odr_prname(o, name);
    	fprintf(o->print, "%s\n", (**p ? "TRUE" : "FALSE"));
    	return 1;
    }
    if (cons)
    	return 0;
    if (o->direction == ODR_DECODE)
    	*p = (int *)odr_malloc(o, sizeof(int));
    return ber_boolean(o, *p);
}
