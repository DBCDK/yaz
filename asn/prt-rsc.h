/*
 * Copyright (c) 1995, Index Data.
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
 * $Log: prt-rsc.h,v $
 * Revision 1.1  2000-10-03 12:55:50  adam
 * Removed several auto-generated files from CVS.
 *
 * Revision 1.1  1999/11/30 13:47:11  adam
 * Improved installation. Moved header files to include/yaz.
 *
 * Revision 1.7  1999/04/20 09:56:48  adam
 * Added 'name' paramter to encoder/decoder routines (typedef Odr_fun).
 * Modified all encoders/decoders to reflect this change.
 *
 * Revision 1.6  1997/05/14 06:53:50  adam
 * C++ support.
 *
 * Revision 1.5  1995/09/29 17:12:11  quinn
 * Smallish
 *
 * Revision 1.4  1995/09/27  15:02:52  quinn
 * Modified function heads & prototypes.
 *
 * Revision 1.3  1995/08/17  12:45:17  quinn
 * Fixed minor problems with GRS-1. Added support in c&s.
 *
 * Revision 1.2  1995/06/02  09:49:50  quinn
 * Add access control
 *
 * Revision 1.1  1995/06/01  11:24:52  quinn
 * Resource Control
 *
 *
 */

#ifndef PRT_RSC_H
#define PRT_RSC_H

#include <yaz/yconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------- Resource-1 -------------------- */

#define Z_EstimateType_currentSearchRecords           1
#define Z_EstimateType_finalSearchRecords             2
#define Z_EstimateType_currentPresentRecords          3
#define Z_EstimateType_finalPresentRecords            4
#define Z_EstimateType_currentOpTimeProcessing        5
#define Z_EstimateType_finalOpTimeProcessing          6
#define Z_EstimateType_currentAssocTime               7
#define Z_EstimateType_currentOperationCost           8
#define Z_EstimateType_finalOperationCost             9
#define Z_EstimateType_currentAssocCost              10
#define Z_EstimateType_finalOpTimeElapsed            11
#define Z_EstimateType_percentComplete               12
#define Z_EstimateType_currentSearchAssCost          13
#define Z_EstimateType_currentPresentAssCost         14
#define Z_EstimateType_currentConnectAssCost         15
#define Z_EstimateType_currentOtherAssCost           16

typedef struct Z_Estimate1
{
    int *type;
    int *value;
    int *currencyCode;             /* OPTIONAL */
} Z_Estimate1;

typedef struct Z_ResourceReport1
{
    int num_estimates;
    Z_Estimate1 **estimates;
    char *message;
} Z_ResourceReport1;

int z_ResourceReport1(ODR o, Z_ResourceReport1 **p, int opt, const char *name);

/* -------------------- Resource-2 -------------------- */

typedef struct Z_Estimate2
{
    Z_StringOrNumeric *type;      /* same as in estimate1, if numeric */
    Z_IntUnit *value;
} Z_Estimate2;

typedef struct Z_ResourceReport2
{
    int num_estimates;
    Z_Estimate2 **estimates;             /* OPTIONAL */
    char *message;                       /* OPTIONAL */
} Z_ResourceReport2;

int z_ResourceReport2(ODR o, Z_ResourceReport2 **p, int opt, const char *name);

#ifdef __cplusplus
}
#endif

#endif
