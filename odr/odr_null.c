/*
 * Copyright (c) 1995, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: odr_null.c,v $
 * Revision 1.7  1995-09-27 15:02:59  quinn
 * Modified function heads & prototypes.
 *
 * Revision 1.6  1995/05/22  11:32:03  quinn
 * Fixing Interface to odr_null.
 *
 * Revision 1.5  1995/05/16  08:50:56  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.4  1995/03/08  12:12:26  quinn
 * Added better error checking.
 *
 * Revision 1.3  1995/02/10  18:57:25  quinn
 * More in the way of error-checking.
 *
 * Revision 1.2  1995/02/09  15:51:49  quinn
 * Works better now.
 *
 * Revision 1.1  1995/02/02  16:21:54  quinn
 * First kick.
 *
 */

#include <odr.h>

/*
 * Top level null en/decoder.
 * Returns 1 on success, 0 on error.
 */
int MDF odr_null(ODR o, Odr_null **p, int opt)
{
    int res, cons = 0;

    if (o->error)
    	return 0;
    if (o->t_class < 0)
    {
    	o->t_class = ODR_UNIVERSAL;
    	o->t_tag = ODR_NULL;
    }
    if ((res = ber_tag(o, p, o->t_class, o->t_tag, &cons, opt)) < 0)
    	return 0;
    if (!res)
    	return opt;
    if (o->direction == ODR_PRINT)
    {
    	fprintf(o->print, "%sNULL\n", odr_indent(o));
    	return 1;
    }
    if (cons)
    {
    	o->error = OPROTO;
    	return 0;
    }
    if (o->direction == ODR_DECODE)
    	*p = ODR_NULLVAL;
    return ber_null(o);
}
