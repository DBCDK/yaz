/*
 * Copyright (c) 1997-2004, Index Data
 * See the file LICENSE for details.
 *
 * $Id: atoin.c,v 1.3 2004-10-15 00:18:59 adam Exp $
 */

/** 
 * \file atoin.c
 * \brief Implements atoi_n function.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <yaz/yaz-util.h>

/**
 * atoi_n: like atoi but reads at most len characters.
 */
int atoi_n (const char *buf, int len)
{
    int val = 0;

    while (--len >= 0)
    {
        if (isdigit (*(const unsigned char *) buf))
            val = val*10 + (*buf - '0');
	buf++;
    }
    return val;
}

