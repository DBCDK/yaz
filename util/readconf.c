/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: readconf.c,v $
 * Revision 1.1  1995-11-01 13:55:06  quinn
 * Minor adjustments
 *
 * Revision 1.2  1995/10/30  13:54:27  quinn
 * iRemoved fclose().
 *
 * Revision 1.1  1995/10/10  16:28:18  quinn
 * Initial revision
 *
 *
 */

#include <stdio.h>
#include <ctype.h>

#include <log.h>

int readconf_line(FILE *f, char *line, int len, char *argv[], int num)
{
    char *p;
    int argc;

    while ((p = fgets(line, len, f)))
    {
	while (*p && isspace(*p))
	    p++;
	if (*p && *p != '#')
	    break;
    }
    if (!p)
	return 0;

    for (argc = 0; *p ; argc++)
    {
	if (*p == '#')  /* trailing comment */
	    break;
	argv[argc] = p;
	while (*p && !isspace(*p))
	    p++;
	if (*p)
	{
	    *(p++) = '\0';
	    while (*p && isspace(*p))
		p++;
	}
    }
    return argc;
}

/*
 * Read lines of a configuration file.
 */
int readconf(char *name, void *private,
    int (*fun)(char *name, void *private, int argc, char *argv[]))
{
    FILE *f;
    char line[512], *m_argv[50];
    int m_argc;

    if (!(f = fopen(name, "r")))
    {
	logf(LOG_WARN|LOG_ERRNO, "readconf: %s", name);
	return -1;
    }
    for (;;)
    {
	int res;

	if (!(m_argc = readconf_line(f, line, 512, m_argv, 50)))
	{
	    fclose(f);
	    return 0;
	}

	if ((res = (*fun)(name, private, m_argc, m_argv)))
	{
	    fclose(f);
	    return res;
	}
    }
}
