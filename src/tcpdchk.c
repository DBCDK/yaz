/*
 * Copyright (c) 1995-2004, Index Data
 * See the file LICENSE for details.
 *
 * $Id: tcpdchk.c,v 1.4 2004-12-13 14:21:56 heikki Exp $
 */
/**
 * \file tcpdchk.c
 * \brief Implements TCP WRAPPER check.
 */

#include <stdio.h>
#include <string.h>

#include <yaz/yconfig.h>
#include <yaz/comstack.h>
#include <yaz/statserv.h>
#include <yaz/log.h>


#if HAVE_TCPD_H
#include <syslog.h>
#include <tcpd.h>


int allow_severity = LOG_INFO;  /* not YLOG !! */
int deny_severity = LOG_WARNING;

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_WARN
#undef LOG_WARN
#endif

#endif

int check_ip_tcpd(void *cd, const char *addr, int len, int type)
{
    const char *daemon_name = (const char *) cd;

    if (type == AF_INET)
    {
	if (daemon_name && *daemon_name)
	{
#if HAVE_TCPD_H
	    struct request_info request_info;
	    int i;
#endif
	    char *host_name = 0, *host_addr = 0;
	    struct hostent *host;

	    struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	    
	    if ((host = gethostbyaddr((char*)&addr_in->sin_addr,
				      sizeof(addr_in->sin_addr),
				      AF_INET)))
		host_name = (char*) host->h_name;
	    host_addr = inet_ntoa(addr_in->sin_addr);
#if HAVE_TCPD_H
	    if (host_addr)
		request_init(&request_info, RQ_DAEMON, daemon_name,
			     RQ_CLIENT_NAME, host_name,
			     RQ_CLIENT_SIN, addr_in,
			     RQ_CLIENT_ADDR, host_addr, 0);
	    else
		request_init(&request_info, RQ_DAEMON, daemon_name,
			     RQ_CLIENT_SIN, addr_in,
			     RQ_CLIENT_ADDR, host_addr, 0);
	    i = hosts_access(&request_info);
	    if (!i)
	    {
		yaz_log (YLOG_DEBUG, "access denied from %s",
			 host_name ? host_name : host_addr);
		return 1;
	    }
	    yaz_log (YLOG_DEBUG, "access granted from %s",
		     host_name ? host_name : host_addr);
#endif
	}
    }
    return 0;
}

