/*
 * Copyright (c) 1995-1998, Index Data.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation, in whole or in part, for any purpose, is hereby granted,
 * provided that:
 *
 * 1. This copyright and permission notice appear in all copies of the
 * software and its documentation. Notices of copyright or attribution
 * which appear at the beginning of any file must remain unchanged.
 *
 * 2. The name of Index Data or the individual authors may not be used to
 * endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED, OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL INDEX DATA BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR
 * NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * $Log: backend.h,v $
 * Revision 1.17  1998-01-29 13:15:35  adam
 * Implemented sort for the backend interface.
 *
 * Revision 1.16  1997/09/17 12:10:31  adam
 * YAZ version 1.4.
 *
 */

#ifndef BACKEND_H
#define BACKEND_H

#include <yconfig.h>
#include <proto.h>
#include <statserv.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bend_searchrequest
{
    char *setname;             /* name to give to this set */
    int replace_set;           /* replace set, if it already exists */
    int num_bases;             /* number of databases in list */
    char **basenames;          /* databases to search */
    Z_Query *query;            /* query structure */
    ODR stream;                /* encoding stream */
} bend_searchrequest;

typedef struct bend_searchresult
{
    int hits;                  /* number of hits */
    int errcode;               /* 0==OK */
    char *errstring;           /* system error string or NULL */
} bend_searchresult;

YAZ_EXPORT bend_searchresult *bend_search(void *handle, bend_searchrequest *r,
                                          int *fd);
YAZ_EXPORT bend_searchresult *bend_searchresponse(void *handle);

typedef struct bend_fetchrequest
{
    char *setname;             /* set name */
    int number;                /* record number */
    oid_value format;          /* One of the CLASS_RECSYN members */
    Z_RecordComposition *comp; /* Formatting instructions */
    ODR stream;                /* encoding stream - memory source if required */
} bend_fetchrequest;

typedef struct bend_fetchresult
{
    char *basename;            /* name of database that provided record */
    int len;                   /* length of record or -1 if structured */
    char *record;              /* record */
    int last_in_set;           /* is it?  */
    oid_value format;          /* format */
    int errcode;               /* 0==success */
    char *errstring;           /* system error string or NULL */
} bend_fetchresult;

YAZ_EXPORT bend_fetchresult *bend_fetch(void *handle, bend_fetchrequest *r,
                                        int *fd);
YAZ_EXPORT bend_fetchresult *bend_fetchresponse(void *handle);

typedef struct bend_scanrequest
{
    int num_bases;      /* number of elements in databaselist */
    char **basenames;   /* databases to search */
    oid_value attributeset;
    Z_AttributesPlusTerm *term;
    int term_position;  /* desired index of term in result list */
    int num_entries;    /* number of entries requested */
    ODR stream;         /* encoding stream - memory source if required */
} bend_scanrequest;

struct scan_entry {
    char *term;
    int occurrences;
};

typedef enum {
    BEND_SCAN_SUCCESS,   /* ok */
    BEND_SCAN_PARTIAL   /* not all entries could be found */
} bend_scan_status;

typedef struct bend_scanresult
{
    int num_entries;
    struct scan_entry *entries;
    int term_position;
    bend_scan_status status;
    int errcode;
    char *errstring;
} bend_scanresult;

YAZ_EXPORT bend_scanresult *bend_scan(void *handle, bend_scanrequest *r,
                                      int *fd);
YAZ_EXPORT bend_scanresult *bend_scanresponse(void *handle);

typedef struct bend_deleterequest
{
    char *setname;
} bend_deleterequest;

typedef struct bend_deleteresult
{
    int errcode;               /* 0==success */
    char *errstring;           /* system error string or NULL */
} bend_deleteresult;

YAZ_EXPORT bend_deleteresult *bend_delete(void *handle,
                                          bend_deleterequest *r, int *fd);
YAZ_EXPORT bend_deleteresult *bend_deleteresponse(void *handle);

YAZ_EXPORT void bend_close(void *handle);

typedef struct bend_sortrequest 
{
    int num_input_setnames;
    char **input_setnames;
    char *output_setname;
    Z_SortKeySpecList *sort_sequence;
    ODR stream;
} bend_sortrequest;

typedef struct bend_sortresult
{
    int sort_status;
    int errcode;
    char *errstring;
} bend_sortresult;

typedef struct bend_initrequest
{
    char *configname;
    Z_IdAuthentication *auth;
    ODR stream;                /* encoding stream */
    
    int (*bend_sort) (void *handle, bend_sortrequest *req,
		      bend_sortresult *res);
} bend_initrequest;

typedef struct bend_initresult
{
    int errcode;               /* 0==OK */
    char *errstring;           /* system error string or NULL */
    void *handle;              /* private handle to the backend module */
} bend_initresult;

YAZ_EXPORT bend_initresult MDF *bend_init(bend_initrequest *r);   

#ifdef __cplusplus
}
#endif

#endif
