/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: ber_bool.c,v $
 * Revision 1.4  1995-03-21 10:17:27  quinn
 * Fixed little bug in decoder.
 *
 * Revision 1.3  1995/03/08  12:12:06  quinn
 * Added better error checking.
 *
 * Revision 1.2  1995/02/09  15:51:45  quinn
 * Works better now.
 *
 * Revision 1.1  1995/02/02  16:21:51  quinn
 * First kick.
 *
 */

#include <stdio.h>
#include <odr.h>


int ber_boolean(ODR o, int *val)
{
    int res, len;

    switch (o->direction)
    {
    	case ODR_ENCODE:
	    if (!o->left)
	    {
	    	o->error = OSPACE;
		return 0;
	    }
	    if (ber_enclen(o->bp, 1, 1, 1) != 1)
	    {
	    	o->error = OOTHER;
		return 0;
	    }
	    o->bp++;
	    o->left--;
	    *(o->bp++) = (unsigned char) *val;
#ifdef ODR_DEBUG
	    fprintf(stderr, "[val=%d]\n", *val);
#endif
	    o->left--;
	    return 1;
	case ODR_DECODE:
	    if ((res = ber_declen(o->bp, &len)) < 0)
	    {
	    	o->error = OPROTO;
		return 0;
	    }
	    if (len != 1)
	    {
	    	o->error = OPROTO;
		return 0;
	    }
	    o->bp+= res;
	    o->left -= res;
	    *val = *o->bp;
	    o->bp++;
	    o->left--;
#ifdef ODR_DEBUG
	    fprintf(stderr, "[val=%d]\n", *val);
#endif
	    return 1;
	case ODR_PRINT:
	    return 1;
	default: o->error = OOTHER; return 0;
    }
}
