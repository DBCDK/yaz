/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: statserv.c,v $
 * Revision 1.8  1995-03-20 09:46:26  quinn
 * Added osi support.
 *
 * Revision 1.7  1995/03/16  13:29:04  quinn
 * Partitioned server.
 *
 * Revision 1.6  1995/03/15  15:18:52  quinn
 * Little changes to better support nonblocking I/O
 * Added backend.h
 *
 * Revision 1.5  1995/03/15  08:37:45  quinn
 * Now we're pretty much set for nonblocking I/O.
 *
 * Revision 1.4  1995/03/14  16:59:48  quinn
 * Bug-fixes
 *
 * Revision 1.3  1995/03/14  11:30:15  quinn
 * Works better now.
 *
 * Revision 1.2  1995/03/14  10:28:03  quinn
 * More work on demo server.
 *
 * Revision 1.1  1995/03/10  18:22:45  quinn
 * The rudiments of an asynchronous server.
 *
 */

/*
 * Simple, static server. I wouldn't advise a static server unless you
 * really have to, but it's great for debugging memory management.  :)
 */

#include <stdio.h>

#include <options.h>
#include <eventl.h>
#include <session.h>
#include <eventl.h>
#include <comstack.h>
#include <tcpip.h>
#include <xmosi.h>

#include <unistd.h>
#include <fcntl.h>

static char *me = "";

#define DEFAULT_LISTENER "tcp:localhost:9999"

/*
 * handle incoming connect requests.
 */
void listener(IOCHAN h, int event)
{
    COMSTACK line = (COMSTACK) iochan_getdata(h);
    association *newas;

    if (event == EVENT_INPUT)
    {
	if (cs_listen(line, 0, 0) < 0)
	{
	    if (cs_errno(line) == CSNODATA)
		return;
	    fprintf(stderr, "cs_listen failed.\n");
	    exit(1);
	}
	iochan_setevent(h, EVENT_OUTPUT);
	iochan_setflags(h, EVENT_OUTPUT | EVENT_EXCEPT); /* set up for acpt */
    }
    else if (event == EVENT_OUTPUT)
    {
    	COMSTACK new_line;
    	IOCHAN new_chan;

    	if (!(new_line = cs_accept(line)))
    	{
	    fprintf(stderr, "Accept failed.\n");
	    exit(1);
	}
	if (!(new_chan = iochan_create(cs_fileno(new_line), ir_session,
	    EVENT_INPUT)))
	{
	    fprintf(stderr, "Failed to create iochan\n");
	    exit(1);
	}
	if (!(newas = create_association(new_chan, new_line)))
	{
	    fprintf(stderr, "Failed to create new assoc.\n");
	    exit(1);
	}
	iochan_setdata(new_chan, newas);
    	iochan_setflags(h, EVENT_INPUT | EVENT_EXCEPT); /* reset for listen */
    }
    else
    {
    	fprintf(stderr, "Bad event on listener.\n");
    	exit(1);
    }
}

/*
 * Set up a listening endpoint, and give it to the event-handler.
 */
void add_listener(char *where)
{
    COMSTACK l;
    CS_TYPE type;
    char mode[100], addr[100];
    void *ap;
    IOCHAN lst;

    fprintf(stderr, "Adding listener on %s\n", where);
    if (!where || sscanf(where, "%[^:]:%s", mode, addr) != 2)
    {
    	fprintf(stderr, "%s: Address format: ('tcp'|'osi')':'<address>.\n",
	    me);
	exit(1);
    }
    if (!strcmp(mode, "tcp"))
    {
    	if (!(ap = tcpip_strtoaddr(addr)))
    	{
	    fprintf(stderr, "Address resolution failed for TCP.\n");
	    exit(1);
	}
	type = tcpip_type;
    }
    else if (!strcmp(mode, "osi"))
    {
    	if (!(ap = mosi_strtoaddr(addr)))
    	{
	    fprintf(stderr, "Address resolution failed for TCP.\n");
	    exit(1);
	}
	type = mosi_type;
    }
    else
    {
    	fprintf(stderr, "You must specify either 'osi:' or 'tcp:'.\n");
    	exit(1);
    }
    if (!(l = cs_create(type, 0)))
    {
    	fprintf(stderr, "Failed to create listener\n");
    	exit(1);
    }
    if (cs_bind(l, ap, CS_SERVER) < 0)
    {
    	fprintf(stderr, "Failed to bind.\n");
    	perror(where);
    	exit(1);
    }
    if (!(lst = iochan_create(cs_fileno(l), listener, EVENT_INPUT |
   	 EVENT_EXCEPT)))
    {
    	fprintf(stderr, "Failed to create IOCHAN-type\n");
    	exit(1);
    }
    iochan_setdata(lst, l);
}

int statserv_main(int argc, char **argv)
{
    int ret, listeners = 0;
    char *arg;

    me = argv[0];
    while ((ret = options("l:", argv, argc, &arg)) != -2)
    	switch (ret)
    	{
	    case 0: me = arg; break;
	    case 'l': add_listener(arg); listeners++; break;
	    default:
	    	fprintf(stderr, "Usage: %s [-l <listener-addr>]\n", me);
	    	exit(1);
	}
    if (!listeners)
	add_listener(DEFAULT_LISTENER);
    return event_loop();
}
