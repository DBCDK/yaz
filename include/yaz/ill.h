/*
 * Copyright (c) 1999-2001, Index Data
 * See the file LICENSE for details.
 *
 * $Log: ill.h,v $
 * Revision 1.5  2001-02-20 11:25:32  adam
 * Added ill_get_APDU and ill_get_Cancel.
 *
 * Revision 1.4  2000/02/28 11:20:06  adam
 * Using autoconf. New definitions: YAZ_BEGIN_CDECL/YAZ_END_CDECL.
 *
 * Revision 1.3  2000/01/31 13:15:21  adam
 * Removed uses of assert(3). Cleanup of ODR. CCL parser update so
 * that some characters are not surrounded by spaces in resulting term.
 * ILL-code updates.
 *
 * Revision 1.2  2000/01/15 09:39:50  adam
 * Implemented ill_get_ILLRequest. More ILL testing for client.
 *
 * Revision 1.1  1999/12/16 23:36:19  adam
 * Implemented ILL protocol. Minor updates ASN.1 compiler.
 *
 */
#ifndef ILL_H
#define ILL_H

#include <yaz/ill-core.h>
#include <yaz/item-req.h>

YAZ_BEGIN_CDECL

struct ill_get_ctl {
    ODR odr;
    void *clientData;
    const char *(*f)(void *clientData, const char *element);
};
    
YAZ_EXPORT ILL_ItemRequest *ill_get_ItemRequest (
    struct ill_get_ctl *gs, const char *name, const char *sub);

YAZ_EXPORT ILL_Request *ill_get_ILLRequest (
    struct ill_get_ctl *gs, const char *name, const char *sub);

YAZ_EXPORT ILL_Cancel *ill_get_Cancel (
    struct ill_get_ctl *gc, const char *name, const char *sub);

YAZ_EXPORT ILL_APDU *ill_get_APDU (
    struct ill_get_ctl *gc, const char *name, const char *sub);

YAZ_END_CDECL

#endif
