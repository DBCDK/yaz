/*
 * Copyright (c) 1995-1999, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: odr_bit.c,v $
 * Revision 1.12  1999-04-20 09:56:48  adam
 * Added 'name' paramter to encoder/decoder routines (typedef Odr_fun).
 * Modified all encoders/decoders to reflect this change.
 *
 * Revision 1.11  1998/02/11 11:53:34  adam
 * Changed code so that it compiles as C++.
 *
 * Revision 1.10  1995/09/29 17:12:22  quinn
 * Smallish
 *
 * Revision 1.9  1995/09/27  15:02:58  quinn
 * Modified function heads & prototypes.
 *
 * Revision 1.8  1995/05/16  08:50:51  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.7  1995/03/17  10:17:48  quinn
 * Added memory management.
 *
 * Revision 1.6  1995/03/08  12:12:19  quinn
 * Added better error checking.
 *
 * Revision 1.5  1995/02/10  18:57:25  quinn
 * More in the way of error-checking.
 *
 * Revision 1.4  1995/02/09  15:51:47  quinn
 * Works better now.
 *
 * Revision 1.3  1995/02/07  14:13:45  quinn
 * Bug fixes.
 *
 * Revision 1.2  1995/02/03  17:04:37  quinn
 * *** empty log message ***
 *
 * Revision 1.1  1995/02/02  20:38:50  quinn
 * Updates.
 *
 *
 */

#include <odr.h>
#include <string.h>

/*
 * Top level bitstring string en/decoder.
 * Returns 1 on success, 0 on error.
 */
int odr_bitstring(ODR o, Odr_bitmask **p, int opt, const char *name)
{
    int res, cons = 0;

    if (o->error)
    	return 0;
    if (o->t_class < 0)
    {
    	o->t_class = ODR_UNIVERSAL;
    	o->t_tag = ODR_BITSTRING;
    }
    if ((res = ber_tag(o, p, o->t_class, o->t_tag, &cons, opt)) < 0)
    	return 0;
    if (!res)
    	return opt;
    if (o->direction == ODR_PRINT)
    {
	odr_prname(o, name);
    	fprintf(o->print, "BITSTRING(len=%d)\n",(*p)->top + 1);
    	return 1;
    }
    if (o->direction == ODR_DECODE)
    {
    	*p = (Odr_bitmask *)odr_malloc(o, sizeof(Odr_bitmask));
    	memset((*p)->bits, 0, ODR_BITMASK_SIZE);
    	(*p)->top = -1;
    }
    return ber_bitstring(o, *p, cons);
}
