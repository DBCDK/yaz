#include <stdio.h>
#include <odr.h>
#include <odr_use.h>

typedef Odr_bitmask Z_ReferenceId;

typedef struct Z_InitRequest
{
    Z_ReferenceId *referenceId;    /* OPTIONAL */
    Odr_bitmask *options;
    Odr_bitmask *protocolVersion;
    int *preferredMessageSize;
    int *maximumRecordSize;
    char *idAuthentication;      /* OPTIONAL */
    char *implementationId;      /* OPTIONAL */
    char *implementationName;    /* OPTIONAL */
    char *implementationVersion; /* OPTIONAL */
} Z_InitRequest;

int z_ReferenceId(ODR o, Z_ReferenceId **p, int opt)
{
    return odr_implicit(o, odr_octetstring, (Odr_oct**) p, ODR_CONTEXT, 2, opt);
}

int z_InitRequest(ODR o, Z_InitRequest **p, int opt)
{
    Z_InitRequest *pp;

    if (!odr_sequence_begin(o, p, sizeof(Z_InitRequest)))
    	return opt;
    pp = *p;
    return
    	z_ReferenceId(o, &pp->referenceId, 1) &&
    	odr_implicit(o, odr_bitstring, &pp->protocolVersion, ODR_CONTEXT, 
	    3, 0) &&
	odr_implicit(o, odr_bitstring, &pp->options, ODR_CONTEXT, 4, 0) &&
	odr_implicit(o, odr_integer, &pp->preferredMessageSize, ODR_CONTEXT,
	    5, 0) &&
	odr_implicit(o, odr_integer, &pp->maximumRecordSize, ODR_CONTEXT,
	    6, 0) &&
	odr_implicit(o, odr_visiblestring, &pp->idAuthentication, ODR_CONTEXT,
	    7, 1) &&
	odr_implicit(o, odr_visiblestring, &pp->implementationId, ODR_CONTEXT,
	    110, 1) &&
	odr_implicit(o, odr_visiblestring, &pp->implementationName, ODR_CONTEXT,
	    111, 1) &&
	odr_implicit(o, odr_visiblestring, &pp->implementationVersion,
	    ODR_CONTEXT, 112, 1) &&
	odr_sequence_end(o);
}

struct A
{
    int which;
    union
    {
    	int *b;   /* integer */
    	char *c;  /* visstring */
    } u;
};

int f_A(ODR o, struct A **p, int opt)
{
    int res;
    Odr_arm arm[] =
    {
    	{ -1, -1, -1, 0, (Odr_fun) odr_integer },
    	{ ODR_IMPLICIT, ODR_CONTEXT, 200, 1, (Odr_fun) odr_visiblestring },
    	{ -1, -1, -1, -1, 0 }
    };

    if (o->direction == ODR_DECODE && !*p)
    	*p = nalloc(o, sizeof(**p));
    res = odr_choice(o, arm, &(*p)->u, &(*p)->which);
    if (!res)
    {
    	*p = 0;
    	return opt;
    }
    return 1;
}

#if 0
int main()
{
    int i;
    unsigned char buf[4048];
    struct odr o;
    Z_InitRequest ireq, *ireqp, *ireq2p;
    Odr_bitmask options, protocolVersion;
    char *iId = "YAZ", *iName = "Yet Another Z39.50 Implementation",
    	*iVersion = "0.1";
    int maximumRS = 4096, preferredMS = 2048;
    static Odr_oid oid[] = {1, 2, 3, 4, -1}, *oidp1, *oidp2;

    oidp1 = oid;

    ODR_MASK_ZERO(&protocolVersion);
    ODR_MASK_SET(&protocolVersion, 0);
    ODR_MASK_SET(&protocolVersion, 1);

    ODR_MASK_ZERO(&options);
    ODR_MASK_SET(&options, 0);
    ODR_MASK_SET(&options, 1);
    ODR_MASK_SET(&options, 2);

    ireq.referenceId = 0;
    ireq.protocolVersion = &protocolVersion;
    ireq.options = &options;
    ireq.preferredMessageSize = &preferredMS;
    ireq.maximumRecordSize = &maximumRS;
    ireq.idAuthentication = 0;
    ireq.implementationId = iId;
    ireq.implementationName = iName;
    ireq.implementationVersion = iVersion;
    ireqp = &ireq;

    o.buf = buf;
    o.bp=o.buf;
    o.left = o.buflen = 1024;
    o.direction = ODR_ENCODE;
    o.t_class = -1;

    odr_oid(&o, &oidp1, 0);

    o.direction = ODR_DECODE;
    o.bp = o.buf;

    odr_oid(&o, &oidp2, 0);
}    
#endif

int main()
{
    struct A ch, *chp1, *chp2;
    int b = 9999;
    char *c = "Nu tester vi en satans forpulet CHOICE!";
    struct odr o;
    unsigned char buf[4096];
    
    ch.u.c = c;	
    ch.which = 1;
    chp1 = &ch;

    o.buf = buf;
    o.bp=o.buf;
    o.left = o.buflen = 1024;
    o.direction = ODR_ENCODE;
    o.t_class = -1;

    f_A(&o, &chp1, 0);

    o.direction = ODR_DECODE;
    o.bp = o.buf;

    f_A(&o, &chp2, 0);

    return 0;
}
