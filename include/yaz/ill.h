/*
 * Copyright (c) 1999-2003, Index Data
 * See the file LICENSE for details.
 *
 * $Id: ill.h,v 1.6 2003-03-03 19:57:35 adam Exp $
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
