/*
 * Copyright (c) 1995, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: comstack.c,v $
 * Revision 1.2  1995-09-29 17:01:48  quinn
 * More Windows work
 *
 * Revision 1.1  1995/06/14  09:58:20  quinn
 * Renamed yazlib to comstack.
 *
 * Revision 1.2  1995/05/16  08:51:15  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.1  1995/03/14  10:28:34  quinn
 * Adding server-side support to tcpip.c and fixing bugs in nonblocking I/O
 *
 *
 */

const char *cs_errlist[] =
{
    "No error or unspecified error",
    "System (lower-layer) error",
    "Operation out of state",
    "No data (operation would block)",
    "New data while half of old buffer is on the line (flow control)"
};

const char *cs_errmsg(int n)
{
    return cs_errlist[n];
}
