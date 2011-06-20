/* This file is part of the YAZ toolkit.
 * Copyright (C) 1995-2011 Index Data
 * See the file LICENSE for details.
 */
/**
 * \file cql2ccl.c
 * \brief Implements CQL to XCQL conversion.
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <yaz/cql.h>

static int cql_to_ccl_r(struct cql_node *cn, 
                        void (*pr)(const char *buf, void *client_data),
                        void *client_data);

static void pr_term(struct cql_node *cn,
                    void (*pr)(const char *buf, void *client_data),
                    void *client_data)
{
    while (cn)
    {
        const char *cp;
        cp = cn->u.st.term;
        while (*cp)
        {
            char x[2];
            if (*cp == '*')
                x[0] = '?';
            else
                x[0] = *cp;
            x[1] = 0;
            pr(x, client_data);
            cp++;
        }
        if (cn->u.st.extra_terms)
            pr(" ", client_data);
        cn = cn->u.st.extra_terms;
    }
}

static int node(struct cql_node *cn, 
                void (*pr)(const char *buf, void *client_data),
                void *client_data)
{
    const char *ccl_field = 0;
    const char *split_op = 0;
    const char *ccl_rel = 0;
    const char *rel = cn->u.st.relation;

    if (cn->u.st.index && strcmp(cn->u.st.index,
                                 "cql.serverChoice"))
        ccl_field = cn->u.st.index;

    if (!rel)
        ;
    else if (!strcmp(rel, "<") || !strcmp(rel, "<=")
             || !strcmp(rel, ">") || !strcmp(rel, ">=")
             || !strcmp(rel, "<>") || !strcmp(rel, "="))
        ccl_rel = rel;
    else if (!strcmp(rel, "all"))
    {
        ccl_rel = "=";
        split_op = "and";
    }
    else if (!strcmp(rel, "any"))
    {
        ccl_rel = "=";
        split_op = "or";
    }
    else if (!strcmp(rel, "==") || !strcmp(rel, "adj"))
    {
        ccl_rel = "=";
    }
    else
    {
        /* unsupported relation */
        return -1;
    }
    if (!split_op)
    {
        if (ccl_field && ccl_rel)
        {
            pr(ccl_field, client_data);
            pr(ccl_rel, client_data);
        }
        pr_term(cn, pr, client_data);
    }
    else
    {
        const char *cp = cn->u.st.term;
        
        while (1)
        {
            if (*cp == '\0')
                break;
            if (ccl_field && ccl_rel)
            {
                pr(ccl_field, client_data);
                pr(ccl_rel, client_data);
            }
            while (*cp && *cp != ' ')
            {
                char x[2];
                if (*cp == '*')
                    x[0] = '?';
                else
                    x[0] = *cp;
                x[1] = '\0';
                pr(x, client_data);
                cp++;
            }
            while (*cp == ' ')
                cp++;
            if (*cp == '\0')
                break;
            pr(" ", client_data);
            pr(split_op, client_data);
            pr(" ", client_data);            
        }
    }
    return 0;
}


static int bool(struct cql_node *cn, 
                void (*pr)(const char *buf, void *client_data),
                void *client_data)
{
    int r;

    pr("(", client_data);
    r = cql_to_ccl_r(cn->u.boolean.left, pr, client_data);
    if (r)
        return r;
    
    pr(" ", client_data);
    pr(cn->u.boolean.value, client_data);
    pr(" ", client_data);

    r = cql_to_ccl_r(cn->u.boolean.right, pr, client_data);
    pr(")", client_data);
    return r;
}

static int cql_to_ccl_r(struct cql_node *cn, 
                        void (*pr)(const char *buf, void *client_data),
                        void *client_data)
{
    if (!cn)
        return -1;

    switch (cn->which)
    {
    case CQL_NODE_ST:
        return node(cn, pr, client_data);
    case CQL_NODE_BOOL:
        return bool(cn, pr, client_data);
    case CQL_NODE_SORT:
        return cql_to_ccl_r(cn->u.sort.search, pr, client_data);
    }
    return -1;
}

int cql_to_ccl(struct cql_node *cn, 
               void (*pr)(const char *buf, void *client_data),
               void *client_data)
{
    return cql_to_ccl_r(cn, pr, client_data);
}

void cql_to_ccl_stdio(struct cql_node *cn, FILE *f)
{
    cql_to_ccl(cn, cql_fputs, f);
}

int cql_to_ccl_buf(struct cql_node *cn, char *out, int max)
{
    struct cql_buf_write_info info;
    int r;
    info.off = 0;
    info.max = max;
    info.buf = out;
    r = cql_to_ccl(cn, cql_buf_write_handler, &info);
    if (info.off >= 0)
        info.buf[info.off] = '\0';
    else
        return -2; /* buffer overflow */
    return r;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

