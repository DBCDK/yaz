/*  Copyright (C) 2006, Index Data ApS
 *  See the file LICENSE for details.
 *
 *  $Id: nfatest1.c,v 1.2 2006-05-03 13:47:35 heikki Exp $
 *
 */


#include <stdio.h>
#include <string.h>
#include <yaz/nfa.h>
#include <yaz/nmem.h>
#include <yaz/test.h>

#define VERBOSE 1

char *printfunc(void *result) {
    static char buf[200];
    sprintf(buf,"\"%s\"", (char*) result);
    return buf;
}

void test_match(yaz_nfa *n, 
        yaz_nfa_char *buf, int buflen, 
        int expcode, char *expstr) {
    yaz_nfa_char *c=buf;
    yaz_nfa_char *cp1,*cp2;
    void *resptr=0;
    int i,bi;
    i=yaz_nfa_match(n,&c,buflen,&resptr);
#if VERBOSE    
    printf("\n'%s' returned %d. Moved c by %d, and resulted in '%s'\n",
            expstr, i, (c-buf),(char*)resptr);
#endif
    YAZ_CHECK_EQ(i,expcode);
    if (i!=1)
        YAZ_CHECK_EQ(strcmp(expstr,(char*)resptr),0);
    i=0;
    bi=0;
    while(bi!=2){
        bi=yaz_nfa_get_backref(n,i,&cp1,&cp2);
        if (bi==0 && ( cp1 || cp2 ) ) {
#if VERBOSE    
            printf("  got backref %d of %d chars (%p to %p): '",
                    i, cp2-cp1+1, cp1, cp2);
            while (cp2-cp1 >= 0 )
                printf("%c", *cp1++);
            printf("'\n");
#endif
        }
        i++;
    }
}

void construction_test() {
    yaz_nfa* n= yaz_nfa_init();
    yaz_nfa_char *cp, *cp1, *cp2;
    yaz_nfa_state *s,*s0,*s1,*s2,*s3,*s4,*s5;
    int i;
    yaz_nfa_char seq1[]={'p','r','e','f','i','x',0};
    yaz_nfa_char seq2[]={'p','r','e','l','i','m',0};
    yaz_nfa_char tst1[]={'c',0};
    yaz_nfa_char tst2[]={'c','k',0};
    yaz_nfa_char tst3[]={'c','x',0};
    yaz_nfa_char tst4[]={'z','k',0};
    yaz_nfa_char tst5[]={'y','k','l','k','k','l','k','d',0};
    yaz_nfa_char tst6[]={'x','z','k','a','b',0};
    void *p;

    YAZ_CHECK(n);

    s=yaz_nfa_get_first(n);
    YAZ_CHECK(!s);

    s0=yaz_nfa_add_state(n);

    s=yaz_nfa_get_first(n);
    YAZ_CHECK(s);
    s=yaz_nfa_get_next(n,s);
    YAZ_CHECK(!s);

    s1=yaz_nfa_add_state(n);
    i=yaz_nfa_set_result(n,s1,"first");
    YAZ_CHECK_EQ(i,0);

    i=yaz_nfa_set_result(n,s1,"DUPLICATE");
    YAZ_CHECK_EQ(i,1);

    p=yaz_nfa_get_result(n,s1);
    YAZ_CHECK(p);
    YAZ_CHECK( strcmp((char*)p,"first")==0 );

    i=yaz_nfa_set_result(n,s1,0);
    YAZ_CHECK_EQ(i,0);
    p=yaz_nfa_get_result(n,s1);
    YAZ_CHECK(!p);
    i=yaz_nfa_set_result(n,s1,"first");
    YAZ_CHECK_EQ(i,0);
    
    s2=yaz_nfa_add_state(n);
    s3=yaz_nfa_add_state(n);
    yaz_nfa_set_result(n,s3,"a-k,x-z");

    s=yaz_nfa_get_first(n);
    YAZ_CHECK(s);
    s=yaz_nfa_get_next(n,s);
    YAZ_CHECK(s);

    
    yaz_nfa_add_transition(n,s0,s1,'a','k');
    yaz_nfa_add_transition(n,s1,s1,'k','k');
    yaz_nfa_add_transition(n,s0,s2,'p','p');
    yaz_nfa_add_transition(n,s1,s3,'x','z');

    s=yaz_nfa_add_range(n, 0, 'k','s' );
    yaz_nfa_set_result(n,s,"K-S");

    s4=yaz_nfa_add_range(n, s2, 'l','r' );
    s5=yaz_nfa_add_range(n, s2, 'l','r' );
    YAZ_CHECK((s4==s5));
    s=yaz_nfa_add_range(n, 0, 'c','c' );

    s=yaz_nfa_add_range(n, 0, 'z','z' );
    yaz_nfa_add_empty_transition(n,s,s);
    yaz_nfa_set_result(n,s,"loop");

    s=yaz_nfa_add_range(n, 0, 'y','y' );
    yaz_nfa_set_backref_point(n,s,1,1);
    s1=yaz_nfa_add_state(n);
    yaz_nfa_add_empty_transition(n,s,s1);
    s=s1;
    yaz_nfa_add_transition(n,s,s,'k','l');
    s=yaz_nfa_add_range(n, s, 'd','d' );
    yaz_nfa_set_result(n,s,"y k+ d");
    yaz_nfa_set_backref_point(n,s,1,0);

    s=yaz_nfa_add_sequence(n, 0, seq1 ); 
    yaz_nfa_set_result(n,s,"PREFIX");
    s=yaz_nfa_add_sequence(n, 0, seq2 ); 
    yaz_nfa_set_result(n,s,"PRELIM");

    s=yaz_nfa_add_range(n, 0, 'x','x' );
    yaz_nfa_set_backref_point(n,s,2,1);
    s1=yaz_nfa_add_sequence(n,s,tst4);
    yaz_nfa_set_backref_point(n,s1,2,0);
    yaz_nfa_set_result(n,s1,"xzk");

    /* check return codes before doing any matches */
    i=yaz_nfa_get_backref(n, 0, &cp1, &cp2 );
    YAZ_CHECK_EQ(i,1);
    i=yaz_nfa_get_backref(n, 3, &cp1, &cp2 );
    YAZ_CHECK_EQ(i,2);
    i=yaz_nfa_get_backref(n, 1, &cp1, &cp2 );
    YAZ_CHECK_EQ(i,1);

    
#if VERBOSE    
    yaz_nfa_dump(0,n, printfunc);
#endif

    test_match(n,seq2,3,YAZ_NFA_OVERRUN,"K-S");
    test_match(n,seq2,6,YAZ_NFA_SUCCESS,"PRELIM");
    test_match(n,tst1,3,YAZ_NFA_SUCCESS,"first");
    test_match(n,tst2,3,YAZ_NFA_SUCCESS,"first");
    test_match(n,tst3,3,YAZ_NFA_SUCCESS,"a-k,x-z");
    test_match(n,tst4,9,YAZ_NFA_LOOP,"loop");
    test_match(n,tst5,9,YAZ_NFA_SUCCESS,"y k+ d");

    cp=tst6;
    i=yaz_nfa_match(n,&cp,8,&p);
    YAZ_CHECK_EQ(i,YAZ_NFA_SUCCESS); 
    i=yaz_nfa_get_backref(n, 2, &cp1, &cp2 );
    YAZ_CHECK_EQ(i,0);
#if VERBOSE    
    printf("backref from %p to %p is %d long\n",
            cp1,cp2, cp2-cp1 );
#endif

    yaz_nfa_destroy(n);
}

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv);
    nmem_init ();
    construction_test();
    nmem_exit ();
    YAZ_CHECK_TERM;
}

