/*
 * Copyright (c) 1995-2003, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Id: ber_null.c,v 1.13 2003-01-06 08:20:27 adam Exp $
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "odr-priv.h"

/*
 * BER-en/decoder for NULL type.
 */
int ber_null(ODR o)
{
    switch (o->direction)
    {
    case ODR_ENCODE:
        if (odr_putc(o, 0X00) < 0)
            return 0;
#ifdef ODR_DEBUG
        fprintf(stderr, "[NULL]\n");
#endif
        return 1;
    case ODR_DECODE:
        if (*(o->bp++) != 0X00)
        {
            o->error = OPROTO;
            return 0;
        }
#ifdef ODR_DEBUG
        fprintf(stderr, "[NULL]\n");
#endif
        return 1;
    case ODR_PRINT: return 1;
    default: o->error = OOTHER; return 0;
    }
}
