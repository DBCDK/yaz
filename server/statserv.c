/*
 * Copyright (c) 1995, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: statserv.c,v $
 * Revision 1.35  1996-05-29 10:03:28  quinn
 * Options work
 *
 * Revision 1.34  1996/02/21  13:12:07  quinn
 * *** empty log message ***
 *
 * Revision 1.33  1996/02/10  12:23:49  quinn
 * Enable inetd operations fro TCP/IP stack
 *
 * Revision 1.32  1996/01/19  15:41:52  quinn
 * *** empty log message ***
 *
 * Revision 1.31  1995/11/17  11:09:39  adam
 * Added new option '-c' to specify configuration name in control block.
 *
 * Revision 1.30  1995/11/01  13:54:59  quinn
 * Minor adjustments
 *
 * Revision 1.29  1995/10/30  12:41:29  quinn
 * Added hostname lookup for server.
 *
 * Revision 1.28  1995/09/29  17:12:30  quinn
 * Smallish
 *
 * Revision 1.27  1995/09/27  15:03:02  quinn
 * Modified function heads & prototypes.
 *
 * Revision 1.26  1995/08/29  14:44:51  quinn
 * Reset timeouts.
 *
 * Revision 1.25  1995/08/29  11:18:02  quinn
 * Added code to receive close
 *
 * Revision 1.24  1995/06/16  10:31:39  quinn
 * Added session timeout.
 *
 * Revision 1.23  1995/06/15  12:30:48  quinn
 * Setuid-facility.
 *
 * Revision 1.22  1995/06/15  07:45:17  quinn
 * Moving to v3.
 *
 * Revision 1.21  1995/06/06  08:15:40  quinn
 * Cosmetic.
 *
 * Revision 1.20  1995/05/29  08:12:09  quinn
 * Moved oid to util
 *
 * Revision 1.19  1995/05/16  09:37:27  quinn
 * Fixed bug
 *
 * Revision 1.18  1995/05/16  08:51:09  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.17  1995/05/15  11:56:42  quinn
 * Asynchronous facilities. Restructuring of seshigh code.
 *
 * Revision 1.16  1995/04/10  10:23:40  quinn
 * Some work to add scan and other things.
 *
 * Revision 1.15  1995/03/31  10:16:51  quinn
 * Fixed logging.
 *
 * Revision 1.14  1995/03/31  09:18:58  quinn
 * Added logging.
 *
 * Revision 1.13  1995/03/30  16:08:39  quinn
 * Little mods.
 *
 * Revision 1.12  1995/03/30  13:29:02  quinn
 * Smallish
 *
 * Revision 1.11  1995/03/30  12:18:17  quinn
 * Fixed bug.
 *
 * Revision 1.10  1995/03/29  15:40:16  quinn
 * Ongoing work. Statserv is now dynamic by default
 *
 * Revision 1.9  1995/03/27  08:34:30  quinn
 * Added dynamic server functionality.
 * Released bindings to session.c (is now redundant)
 *
 * Revision 1.8  1995/03/20  09:46:26  quinn
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/time.h>

#include <options.h>
#include <eventl.h>
#include <session.h>
#include <eventl.h>
#include <comstack.h>
#include <tcpip.h>
#ifdef USE_XTIMOSI
#include <xmosi.h>
#endif
#include <log.h>
#include <statserv.h>

static char *me = "statserver";
/*
 * default behavior.
 */
static statserv_options_block control_block = {
    1,                          /* dynamic mode */
    LOG_DEFAULT_LEVEL,          /* log level */
    "",                         /* no PDUs */
    "",                         /* diagnostic output to stderr */
    "tcp:@:9999",               /* default listener port */
    PROTO_Z3950,                /* default application protocol */
    60,                         /* idle timeout (minutes) */
    1024*1024,                  /* maximum PDU size (approx.) to allow */
    "default-config",           /* configuration name to pass to backend */
    ""                          /* set user id */
};

/*
 * handle incoming connect requests.
 * The dynamic mode is a bit tricky mostly because we want to avoid
 * doing all of the listening and accepting in the parent - it's
 * safer that way.
 */
static void listener(IOCHAN h, int event)
{
    COMSTACK line = (COMSTACK) iochan_getdata(h);
    association *newas;
    static int hand[2];
    static int child = 0;
    int res;

    if (event == EVENT_INPUT)
    {
	if (control_block.dynamic && !child) 
	{
	    int res;

	    if (pipe(hand) < 0)
	    {
		logf(LOG_FATAL|LOG_ERRNO, "pipe");
		exit(1);
	    }
	    if ((res = fork()) < 0)
	    {
		logf(LOG_FATAL|LOG_ERRNO, "fork");
		exit(1);
	    }
	    else if (res == 0) /* child */
	    {
	    	char nbuf[100];
		IOCHAN pp;

		close(hand[0]);
		child = 1;
		for (pp = iochan_getchan(); pp; pp = iochan_getnext(pp))
		{
		    if (pp != h)
		    {
			COMSTACK l = iochan_getdata(pp);
			cs_close(l);
			iochan_destroy(pp);
		    }
		}
		sprintf(nbuf, "%s(%d)", me, getpid());
		log_init(control_block.loglevel, nbuf, 0);
	    }
	    else /* parent */
	    {
		close(hand[1]);
		/* wait for child to take the call */
		for (;;)
		{
		    char dummy[1];
		    int res;
		    
		    if ((res = read(hand[0], dummy, 1)) < 0 && errno != EINTR)
		    {
			logf(LOG_FATAL|LOG_ERRNO, "handshake read");
			exit(1);
		    }
		    else if (res >= 0)
		    	break;
		}
		logf(LOG_DEBUG, "P: Child has taken the call");
		close(hand[0]);
		return;
	    }
	}
	if ((res = cs_listen(line, 0, 0)) < 0)
	{
	    logf(LOG_FATAL, "cs_listen failed.");
	    return;
	}
	else if (res == 1)
	    return;
	logf(LOG_DEBUG, "listen ok");
	iochan_setevent(h, EVENT_OUTPUT);
	iochan_setflags(h, EVENT_OUTPUT | EVENT_EXCEPT); /* set up for acpt */
    }
    /* in dynamic mode, only the child ever comes down here */
    else if (event == EVENT_OUTPUT)
    {
    	COMSTACK new_line;
    	IOCHAN new_chan;
	char *a;

	if (!(new_line = cs_accept(line)))
	{
	    logf(LOG_FATAL, "Accept failed.");
	    iochan_setflags(h, EVENT_INPUT | EVENT_EXCEPT); /* reset listener */
	    return;
	}
	logf(LOG_DEBUG, "accept ok");
	if (control_block.dynamic)
	{
	    IOCHAN pp;
	    /* close our half of the listener socket */
	    for (pp = iochan_getchan(); pp; pp = iochan_getnext(pp))
	    {
		COMSTACK l = iochan_getdata(pp);
		cs_close(l);
		iochan_destroy(pp);
	    }
	    /* release dad */
	    logf(LOG_DEBUG, "Releasing parent");
	    close(hand[1]);
	}
	else
	    iochan_setflags(h, EVENT_INPUT | EVENT_EXCEPT); /* reset listener */

	if (!(new_chan = iochan_create(cs_fileno(new_line), ir_session,
	    EVENT_INPUT)))
	{
	    logf(LOG_FATAL, "Failed to create iochan");
	    exit(1);
	}
	if (!(newas = create_association(new_chan, new_line)))
	{
	    logf(LOG_FATAL, "Failed to create new assoc.");
	    exit(1);
	}
	iochan_setdata(new_chan, newas);
	iochan_settimeout(new_chan, control_block.idle_timeout * 60);
	a = cs_addrstr(new_line);
	logf(LOG_LOG, "Accepted connection from %s", a ? a : "[Unknown]");
    }
    else
    {
    	logf(LOG_FATAL, "Bad event on listener.");
    	exit(1);
    }
}

static void inetd_connection(int what)
{
    COMSTACK line;
    IOCHAN chan;
    association *assoc;
    char *addr;

    if (!(line = cs_createbysocket(0, tcpip_type, 0, what)))
    {
	logf(LOG_ERRNO|LOG_FATAL, "Failed to create comstack on socket 0");
	exit(1);
    }
    if (!(chan = iochan_create(cs_fileno(line), ir_session, EVENT_INPUT)))
    {
	logf(LOG_FATAL, "Failed to create iochan");
	exit(1);
    }
    if (!(assoc = create_association(chan, line)))
    {
	logf(LOG_FATAL, "Failed to create association structure");
	exit(1);
    }
    iochan_setdata(chan, assoc);
    iochan_settimeout(chan, control_block.idle_timeout * 60);
    addr = cs_addrstr(line);
    logf(LOG_LOG, "Inetd association from %s", addr ? addr : "[UNKNOWN]");
}

/*
 * Set up a listening endpoint, and give it to the event-handler.
 */
static void add_listener(char *where, int what)
{
    COMSTACK l;
    CS_TYPE type;
    char mode[100], addr[100];
    void *ap;
    IOCHAN lst;

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
#ifdef USE_XTIMOSI
    	if (!(ap = mosi_strtoaddr(addr)))
    	{
	    fprintf(stderr, "Address resolution failed for TCP.\n");
	    exit(1);
	}
	type = mosi_type;
#else
	fprintf(stderr, "OSI Transport not allowed by configuration.\n");
	exit(1);
#endif
    }
    else
    {
    	fprintf(stderr, "You must specify either 'osi:' or 'tcp:'.\n");
    	exit(1);
    }
    logf(LOG_LOG, "Adding %s %s listener on %s",
        control_block.dynamic ? "dynamic" : "static",
    	what == PROTO_SR ? "SR" : "Z3950", where);
    if (!(l = cs_create(type, 0, what)))
    {
    	logf(LOG_FATAL|LOG_ERRNO, "Failed to create listener");
    	exit(1);
    }
    if (cs_bind(l, ap, CS_SERVER) < 0)
    {
    	logf(LOG_FATAL|LOG_ERRNO, "Failed to bind to %s", where);
    	exit(1);
    }
    if (!(lst = iochan_create(cs_fileno(l), listener, EVENT_INPUT |
   	 EVENT_EXCEPT)))
    {
    	logf(LOG_FATAL|LOG_ERRNO, "Failed to create IOCHAN-type");
    	exit(1);
    }
    iochan_setdata(lst, l);
}

static void catchchld(int num)
{
    while (waitpid(-1, 0, WNOHANG) > 0)
	;
    signal(SIGCHLD, catchchld);
}

statserv_options_block *statserv_getcontrol(void)
{
    static statserv_options_block cb;

    memcpy(&cb, &control_block, sizeof(cb));
    return &cb;
}

void statserv_setcontrol(statserv_options_block *block)
{
    memcpy(&control_block, block, sizeof(*block));
}

int statserv_main(int argc, char **argv)
{
    int ret, listeners = 0, inetd = 0, r;
    char *arg;
    int protocol = control_block.default_proto;

    me = argv[0];
    while ((ret = options("a:iszSl:v:u:c:w:t:k:", argv, argc, &arg)) != -2)
    {
    	switch (ret)
    	{
	    case 0:
		add_listener(arg, protocol);
		listeners++;
		break;
	    case 'z': protocol = PROTO_Z3950; break;
	    case 's': protocol = PROTO_SR; break;
	    case 'S': control_block.dynamic = 0; break;
	    case 'l':
	    	strcpy(control_block.logfile, arg ? arg : "");
		log_init(control_block.loglevel, me, control_block.logfile);
		break;
	    case 'v':
		control_block.loglevel = log_mask_str(arg);
		log_init(control_block.loglevel, me, control_block.logfile);
		break;
	    case 'a':
	    	strcpy(control_block.apdufile, arg ? arg : ""); break;
	    case 'u':
	    	strcpy(control_block.setuid, arg ? arg : ""); break;
            case 'c':
                strcpy(control_block.configname, arg ? arg : ""); break;
	    case 't':
	        if (!arg || !(r = atoi(arg)))
		{
		    fprintf(stderr, "%s: Specify positive timeout for -t.\n",
		        me);
		    exit(1);
		}
		control_block.idle_timeout = r;
		break;
	    case  'k':
	        if (!arg || !(r = atoi(arg)))
		{
		    fprintf(stderr, "%s: Specify positive timeout for -t.\n",
			me);
		    exit(1);
		}
		control_block.maxrecordsize = r * 1024;
		break;
	    case 'i':
	    	inetd = 1; break;
	    case 'w':
	        if (chdir(arg))
		{
		    perror(arg);
		    exit(1);
		}
		break;
	    default:
	    	fprintf(stderr, "Usage: %s [ -i -a <pdufile> -v <loglevel>"
                        " -l <logfile> -u <user> -c <config> -t <minutes>"
			" -k <kilobytes>"
                        " -zsS <listener-addr> -w <directory> ... ]\n", me);
	    	exit(1);
            }
    }
    if (inetd)
	inetd_connection(protocol);
    else
    {
	if (control_block.dynamic)
	    signal(SIGCHLD, catchchld);
	if (!listeners && *control_block.default_listen)
	    add_listener(control_block.default_listen, protocol);
    }
    if (*control_block.setuid)
    {
    	struct passwd *pw;
	
	if (!(pw = getpwnam(control_block.setuid)))
	{
	    logf(LOG_FATAL, "%s: Unknown user", control_block.setuid);
	    exit(1);
	}
	if (setuid(pw->pw_uid) < 0)
	{
	    logf(LOG_FATAL|LOG_ERRNO, "setuid");
	    exit(1);
	}
    }
    logf(LOG_LOG, "Entering event loop.");
	    
    return event_loop();
}
