/*
 * Copyright (c) 1995-2003, Index Data
 * See the file LICENSE for details.
 *
 * $Id: tstodr.c,v 1.3 2003-05-20 19:55:30 adam Exp $
 *
 */
#include <stdio.h>
#include <yaz/odr.h>
#include "tstodrcodec.h"

void tst_MySequence1(ODR encode, ODR decode)
{
    char *ber_buf;
    int ber_len;
    Yc_MySequence *s = odr_malloc(encode, sizeof(*s));
    Yc_MySequence *t;

    s->first = odr_intdup(encode, 12345);
    s->second = odr_malloc(encode, sizeof(*s->second));
    s->second->buf = "hello";
    s->second->len = 5;
    s->second->size = 0;
    s->third = odr_intdup(encode, 1);
    s->fourth = odr_nullval();
    s->fifth = odr_intdup(encode, YC_MySequence_enum1);

    if (!yc_MySequence(encode, &s, 0, 0))
        exit(1);
    
    ber_buf = odr_getbuf(encode, &ber_len, 0);

    odr_setbuf(decode, ber_buf, ber_len, 0);

    if (!yc_MySequence(decode, &t, 0, 0))
        exit(2);
    if (!t->first || *t->first != 12345)
        exit(3);
    if (!t->second || !t->second->buf || t->second->len != 5)
        exit(4);
    if (memcmp(t->second->buf, "hello", t->second->len))
        exit(5);
    if (!t->third || *t->third != 1)
        exit(6);
    if (!t->fourth)
        exit(7);
    if (!t->fifth || *t->fifth != YC_MySequence_enum1)
        exit(8);
}

void tst_MySequence2(ODR encode, ODR decode)
{
    char *ber_buf;
    int ber_len;
    Yc_MySequence *s = odr_malloc(encode, sizeof(*s));
    Yc_MySequence *t;

    s->first = 0;  /* deliberately miss this .. */
    s->second = odr_malloc(encode, sizeof(*s->second));
    s->second->buf = "hello";
    s->second->len = 5;
    s->second->size = 0;
    s->third = odr_intdup(encode, 1);
    s->fourth = odr_nullval();
    s->fifth = odr_intdup(encode, YC_MySequence_enum1);

    if (yc_MySequence(encode, &s, 0, 0)) /* should fail */
        exit(9);
    if (odr_geterror(encode) != OREQUIRED)
        exit(10);
    if (strcmp(odr_getaddinfo(encode), "first"))
        exit(11);
    odr_reset(encode);

    if (odr_geterror(encode) != ONONE)
        exit(12);
    if (strcmp(odr_getaddinfo(encode), ""))
        exit(13);
}

int main(int argc, char **argv)
{
    ODR odr_encode = odr_createmem(ODR_ENCODE);
    ODR odr_decode = odr_createmem(ODR_DECODE);

    tst_MySequence1(odr_encode, odr_decode);
    tst_MySequence2(odr_encode, odr_decode);

    odr_destroy(odr_encode);
    odr_destroy(odr_decode);
    exit(0);
}
