/*
 * Copyright (c) 1995-2003, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Id: ber_int.c,v 1.21 2003-01-06 08:20:27 adam Exp $
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#endif

#include "odr-priv.h"

static int ber_encinteger(ODR o, int val);
static int ber_decinteger(const unsigned char *buf, int *val);

int ber_integer(ODR o, int *val)
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
    for (a = 0; a < (int) sizeof(int) - 1; a++)  /* skip superfluous octets */
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
int ber_decinteger(const unsigned char *buf, int *val)
{
    const unsigned char *b = buf;
    unsigned char fill;
    int res, len, remains;
    union { int i; unsigned char c[sizeof(int)]; } tmp;

    if ((res = ber_declen(b, &len)) < 0)
        return -1;
    if (len > (int) sizeof(int))    /* let's be reasonable, here */
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
