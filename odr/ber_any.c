/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: ber_any.c,v $
 * Revision 1.5  1995-02-14 20:39:54  quinn
 * Fixed bugs in completeBER and (serious one in) ber_oid.
 *
 * Revision 1.4  1995/02/14  11:54:33  quinn
 * Adjustments.
 *
 * Revision 1.3  1995/02/10  18:57:24  quinn
 * More in the way of error-checking.
 *
 * Revision 1.2  1995/02/10  15:55:28  quinn
 * Bug fixes, mostly.
 *
 * Revision 1.1  1995/02/09  15:51:45  quinn
 * Works better now.
 *
 */

#include <odr.h>

int ber_any(ODR o, Odr_any **p)
{
    int res;

    switch (o->direction)
    {
    	case ODR_DECODE:
	    if ((res = completeBER(o->bp, 1000)) <= 0)        /* FIX THIS */
	    	return 0;
	    (*p)->buf = nalloc(o, res);
	    memcpy((*p)->buf, o->bp, res);
	    (*p)->len = (*p)->size = res;
	    o->bp += res;
	    o->left -= res;
	    return 1;
    	case ODR_ENCODE:
	    if ((*p)->len > o->left)
	    	return 0;
	    memcpy(o->bp , (*p)->buf, (*p)->len);
	    o->bp += (*p)->len;
	    o->left -= (*p)->len;
	    return 1;
    	default: return 0;
    }
}

/*
 * Return length of BER-package or 0.
 */
int completeBER(unsigned char *buf, int len)
{
    int res, ll, class, tag, cons;
    unsigned char *b = buf;
    
    if (!len)
    	return 0;
    if (!buf[0] && !buf[1])
    	return 0;
    if ((res = ber_dectag(b, &class, &tag, &cons)) <= 0)
    	return 0;
    if (res > len)
    	return 0;
    b += res;
    len -= res;
    if ((res = ber_declen(b, &ll)) <= 0)
    	return 0;
    if (res > len)
    	return 0;
    b += res;
    len -= res;
    if (ll >= 0)
    	return (len >= ll ? ll + (b-buf) : 0);
    if (!cons)
    	return 0;    
    /* constructed - cycle through children */
    while (len >= 2)
    {
	if (*b == 0 && *(b + 1) == 0)
	    break;
	if (!(res = completeBER(b, len)))
	    return 0;
	b += res;
	len -= res;
    }
    return (b - buf) + 2;
}
