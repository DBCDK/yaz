/*
 * $Id: zoomsh.c,v 1.5 2001-11-16 09:52:39 adam Exp $
 *
 * ZOOM-C Shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if HAVE_READLINE_READLINE_H
#include <readline/readline.h> 
#endif
#if HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif

#include <yaz/xmalloc.h>

#include <yaz/zoom.h>

#define MAX_CON 100

static int next_token (const char **cpp, const char **t_start)
{
    int len = 0;
    const char *cp = *cpp;
    while (*cp == ' ')
	cp++;
    *t_start = cp;
    while (*cp && *cp != ' ' && *cp != '\r' && *cp != '\n')
    {
	cp++;
	len++;
    }
    *cpp = cp;
    return len;
}

static int next_token_copy (const char **cpp, char *buf_out, int buf_max)
{
    const char *start;
    int len = next_token (cpp, &start);
    if (!len)
    {
	*buf_out = 0;
	return 0;
    }
    if (len >= buf_max)
	len = buf_max-1;
    memcpy (buf_out, start, len);
    buf_out[len] = '\0';
    return len;
}

static int is_command (const char *cmd_str, const char *this_str, int this_len)
{
    int cmd_len = strlen(cmd_str);
    if (cmd_len != this_len)
	return 0;
    if (memcmp (cmd_str, this_str, cmd_len))
	return 0;
    return 1;
}

static void cmd_set (Z3950_connection *c, Z3950_resultset *r,
		     Z3950_options options,
		     const char **args)
{
    char key[40], val[80];

    if (!next_token_copy (args, key, sizeof(key)))
    {
	printf ("missing argument for set\n");
	return ;
    }
    if (!next_token_copy (args, val, sizeof(val)))
    {
	const char *val = Z3950_options_get(options, key);
	printf ("%s = %s\n", key, val ? val : "<null>");
    }
    else
	Z3950_options_set(options, key, val);
}

static void cmd_close (Z3950_connection *c, Z3950_resultset *r,
		       Z3950_options options,
		       const char **args)
{
    char host[60];
    int i;
    next_token_copy (args, host, sizeof(host));
    for (i = 0; i<MAX_CON; i++)
    {
	const char *h;
	if (!c[i])
	    continue;
	if ((h = Z3950_connection_option_get(c[i], "host"))
            && !strcmp (h, host))
	{
	    Z3950_connection_destroy (c[i]);
	    c[i] = 0;
	}
	else if (*host == '\0')
	{
	    Z3950_connection_destroy (c[i]);
	    c[i] = 0;
	}
    }
}

static void display_records (Z3950_connection c,
			     Z3950_resultset r,
			     int start, int count)
{
    int i;
    for (i = 0; i<count; i++)
    {
	int pos = i + start;
        Z3950_record rec = Z3950_resultset_record (r, pos);
	const char *db = Z3950_record_get (rec, "database", 0);
	int len;
	const char *render = Z3950_record_get (rec, "render", &len);
	const char *syntax = Z3950_record_get (rec, "syntax", 0);
	/* if rec is non-null, we got a record for display */
	if (rec)
	{
	    printf ("%d %s %s\n", pos+1, (db ? db : "unknown"), syntax);
	    if (render)
		fwrite (render, 1, len, stdout);
	    putchar ('\n');
	}
    }
}

static void cmd_show (Z3950_connection *c, Z3950_resultset *r,
		      Z3950_options options,
		      const char **args)
{
    int i;
    char start_str[10], count_str[10];

    if (next_token_copy (args, start_str, sizeof(start_str)))
	Z3950_options_set (options, "start", start_str);

    if (next_token_copy (args, count_str, sizeof(count_str)))
	Z3950_options_set (options, "count", count_str);

    for (i = 0; i<MAX_CON; i++)
	Z3950_resultset_records (r[i], 0, atoi(start_str), atoi(count_str));
    while (Z3950_event (MAX_CON, c))
	;

    for (i = 0; i<MAX_CON; i++)
    {
	int error;
	const char *errmsg, *addinfo;
	/* display errors if any */
	if (!c[i])
	    continue;
	if ((error = Z3950_connection_error(c[i], &errmsg, &addinfo)))
	    fprintf (stderr, "%s error: %s (%d) %s\n",
		     Z3950_connection_option_get(c[i], "host"), errmsg,
		     error, addinfo);
	else if (r[i])
	{
	    /* OK, no major errors. Display records... */
	    int start = Z3950_options_get_int (options, "start", 0);
	    int count = Z3950_options_get_int (options, "count", 0);
	    display_records (c[i], r[i], start, count);
	}
    }
}

static void cmd_search (Z3950_connection *c, Z3950_resultset *r,
			Z3950_options options,
			const char **args)
{
    Z3950_query s;
    int i;
    
    s = Z3950_query_create ();
    if (Z3950_query_prefix (s, *args))
    {
	fprintf (stderr, "Bad PQF: %s\n", *args);
	return;
    }
    for (i = 0; i<MAX_CON; i++)
    {
	if (c[i])
	{
	    Z3950_resultset_destroy (r[i]);
	    r[i] = 0;
	}
	if (c[i])
	    r[i] = Z3950_connection_search (c[i], s);
    }

    while (Z3950_event (MAX_CON, c))
	;

    for (i = 0; i<MAX_CON; i++)
    {
	int error;
	const char *errmsg, *addinfo;
	/* display errors if any */
	if (!c[i])
	    continue;
	if ((error = Z3950_connection_error(c[i], &errmsg, &addinfo)))
	    fprintf (stderr, "%s error: %s (%d) %s\n",
		     Z3950_connection_option_get(c[i], "host"), errmsg,
		     error, addinfo);
	else if (r[i])
	{
	    /* OK, no major errors. Look at the result count */
	    int start = Z3950_options_get_int (options, "start", 0);
	    int count = Z3950_options_get_int (options, "count", 0);

	    printf ("%s: %d hits\n", Z3950_connection_option_get(c[i], "host"),
		    Z3950_resultset_size(r[i]));
	    /* and display */
	    display_records (c[i], r[i], start, count);
	}
    }
    Z3950_query_destroy (s);
}

static void cmd_help (Z3950_connection *c, Z3950_resultset *r,
		      Z3950_options options,
		      const char **args)
{
    printf ("connect <zurl>\n");
    printf ("search <pqf>\n");
    printf ("show [<start> [<count>]\n");
    printf ("quit\n");
    printf ("close <zurl>\n");
    printf ("set <option> [<value>]]\n");
    printf ("\n");
    printf ("options:\n");
    printf (" start\n");
    printf (" count\n");
    printf (" databaseName\n");
    printf (" preferredRecordSyntax\n");
    printf (" proxy\n");
    printf (" elementSetName\n");
    printf (" maximumRecordSize\n");
    printf (" preferredRecordSize\n");
    printf (" async\n");
    printf (" piggyback\n");
    printf (" group\n");
    printf (" user\n");
    printf (" pass\n");
    printf (" implementationName\n");
}

static void cmd_connect (Z3950_connection *c, Z3950_resultset *r,
			 Z3950_options options,
			 const char **args)
{
    int error;
    const char *errmsg, *addinfo;
    char host[60];
    int j, i;
    if (!next_token_copy (args, host, sizeof(host)))
    {
	printf ("missing host after connect\n");
	return ;
    }
    for (j = -1, i = 0; i<MAX_CON; i++)
    {
	const char *h;
	if (c[i] && (h = Z3950_connection_option_get(c[i], "host")) &&
	    !strcmp (h, host))
	{
	    Z3950_connection_destroy (c[i]);
	    break;
	}
	else if (c[i] == 0 && j == -1)
	    j = i;
    }
    if (i == MAX_CON)  /* no match .. */
    {
	if (j == -1)
	{
	    printf ("no more connection available\n");
	    return;
	}
	i = j;   /* OK, use this one is available */
    }
    c[i] = Z3950_connection_create (options);
    Z3950_connection_connect (c[i], host, 0);

    if ((error = Z3950_connection_error(c[i], &errmsg, &addinfo)))
	printf ("%s error: %s (%d) %s\n",
                Z3950_connection_option_get(c[i], "host"),
		errmsg, error, addinfo);
    
}

static int cmd_parse (Z3950_connection *c, Z3950_resultset *r,
		      Z3950_options options, 
		      const char **buf)
{
    int cmd_len;
    const char *cmd_str;

    cmd_len = next_token (buf, &cmd_str);
    if (!cmd_len)
	return 1;
    if (is_command ("quit", cmd_str, cmd_len))
	return 0;
    else if (is_command ("set", cmd_str, cmd_len))
	cmd_set (c, r, options, buf);
    else if (is_command ("connect", cmd_str, cmd_len))
	cmd_connect (c, r, options, buf);
    else if (is_command ("search", cmd_str, cmd_len))
	cmd_search (c, r, options, buf);
    else if (is_command ("show", cmd_str, cmd_len))
	cmd_show (c, r, options, buf);
    else if (is_command ("close", cmd_str, cmd_len))
	cmd_close (c, r, options, buf);
    else if (is_command ("help", cmd_str, cmd_len))
	cmd_help(c, r, options, buf);
    else
	printf ("unknown command %.*s\n", cmd_len, cmd_str);
    return 2;
}

void shell(Z3950_connection *c, Z3950_resultset *r, Z3950_options options)
{
    while (1)
    {
        char buf[1000];
	const char *bp = buf;
#if HAVE_READLINE_READLINE_H
	char* line_in;
	line_in=readline("ZOOM>");
	if (!line_in)
	    break;
#if HAVE_READLINE_HISTORY_H
	if (*line_in)
	    add_history(line_in);
#endif
	if(strlen(line_in) > 999) {
	    fprintf(stderr,"Input line too long\n");
	    break;
	};
	strcpy(buf,line_in);
	free (line_in);
#else    
	printf ("ZOOM>"); fflush (stdout);
	if (!fgets (buf, 999, stdin))
	    break;
#endif 
	if (!cmd_parse (c, r, options, &bp))
	    break;
    }
}

int main (int argc, char **argv)
{
    Z3950_options options = Z3950_options_create();
    int i, res;
    Z3950_connection z39_con[MAX_CON];
    Z3950_resultset  z39_res[MAX_CON];
    for (i = 0; i<MAX_CON; i++)
    {
	z39_con[i] = 0;
	z39_res[i] = 0;
    }

    for (i = 0; i<MAX_CON; i++)
	z39_con[i] = 0;

    res = 1;
    for (i = 1; i<argc; i++)
    {
	const char *bp = argv[i];
	res = cmd_parse(z39_con, z39_res, options, &bp);
	if (res == 0)  /* received quit */
	    break;
    }
    if (res)  /* do cmdline shell only if not quitting */
	shell(z39_con, z39_res, options);
    Z3950_options_destroy(options);

    for (i = 0; i<MAX_CON; i++)
    {
	Z3950_connection_destroy(z39_con[i]);
	Z3950_resultset_destroy(z39_res[i]);
    }
    exit (0);
}
