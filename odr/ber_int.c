/*
 * Copyright (c) 1995, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: ber_int.c,v $
 * Revision 1.8  1995-09-27 15:02:55  quinn
 * Modified function heads & prototypes.
 *
 * Revision 1.7  1995/05/16  08:50:44  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.6  1995/04/18  08:15:14  quinn
 * Added dynamic memory allocation on encoding (whew). Code is now somewhat
 * neater. We'll make the same change for decoding one day.
 *
 * Revision 1.5  1995/03/27  15:01:44  quinn
 * Added include of sys/types to further portability
 *
 * Revision 1.4  1995/03/08  12:12:07  quinn
 * Added better error checking.
 *
 * Revision 1.3  1995/02/09  15:51:46  quinn
 * Works better now.
 *
 * Revision 1.2  1995/02/07  17:52:58  quinn
 * A damn mess, but now things work, I think.
 *
 * Revision 1.1  1995/02/02  16:21:52  quinn
 * First kick.
 *
 */

#include <odr.h>
#include <sys/types.h>
#include <netinet/in.h>  /* for htons... */
#include <string.h>

static int ber_encinteger(ODR o, int val);
static int ber_decinteger(unsigned char *buf, int *val);

int MDF ber_integer(ODR o, int *val)
{
    int res;

    switch (o->direction)
    {
    	case ODR_DECODE:
	    if ((res = ber_decinteger(o->bp, val)) <= 0)
	    {
	    	o->error = OPROTO;
	    	return 0;
	    }
	    o->bp += res;
	    o->left -= res;
	    return 1;
    	case ODR_ENCODE:
	    if ((res = ber_encinteger(o, *val)) < 0)
	    	return 0;
	    return 1;
    	case ODR_PRINT: return 1;
    	default: o->error = OOTHER;  return 0;
    }
}

/*
 * Returns: number of bytes written or -1 for error (out of bounds).
 */
int ber_encinteger(ODR o, int val)
{
    int lenpos;
    int a, len;
    union { int i; unsigned char c[sizeof(int)]; } tmp;

    lenpos = odr_tell(o);
    if (odr_putc(o, 0) < 0)  /* dummy */
    	return -1;

    tmp.i = htonl(val);   /* ensure that that we're big-endian */
    for (a = 0; a < sizeof(int) - 1; a++)  /* skip superfluous octets */
    	if (!((tmp.c[a] == 0 && !(tmp.c[a+1] & 0X80)) ||
	    (tmp.c[a] == 0XFF && (tmp.c[a+1] & 0X80))))
	    break;
    len = sizeof(int) - a;
    if (odr_write(o, (unsigned char*) tmp.c + a, len) < 0)
    	return -1;
    odr_seek(o, ODR_S_SET, lenpos);
    if (ber_enclen(o, len, 1, 1) != 1)
    	return -1;
    odr_seek(o, ODR_S_END, 0);
#ifdef ODR_DEBUG
    fprintf(stderr, "[val=%d]", val);
#endif
    return 0;
}

/*
 * Returns: Number of bytes read or 0 if no match, -1 if error.
 */
int ber_decinteger(unsigned char *buf, int *val)
{
    unsigned char *b = buf, fill;
    int res, len, remains;
    union { int i; unsigned char c[sizeof(int)]; } tmp;

    if ((res = ber_declen(b, &len)) < 0)
    	return -1;
    if (len > sizeof(int))    /* let's be reasonable, here */
    	return -1;
    b+= res;

    remains = sizeof(int) - len;
    memcpy(tmp.c + remains, b, len);
    if (*b & 0X80)
    	fill = 0XFF;
    else
    	fill = 0X00;
    memset(tmp.c, fill, remains);
    *val = ntohl(tmp.i);

    b += len;
#ifdef ODR_DEBUG
    fprintf(stderr, "[val=%d]", *val);
#endif
    return b - buf;
}
