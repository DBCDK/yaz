/*
 * Copyright (c) 2000-2003, Index Data
 * See the file LICENSE for details.
 *
 * $Id: zoom-c.c,v 1.15 2003-01-24 11:52:57 adam Exp $
 *
 * ZOOM layer for C, connections, result sets, queries.
 */
#include <assert.h>
#include <string.h>
#include "zoom-p.h"

#include <yaz/xmalloc.h>
#include <yaz/otherinfo.h>
#include <yaz/log.h>
#include <yaz/pquery.h>
#include <yaz/marcdisp.h>
#include <yaz/diagbib1.h>
#include <yaz/charneg.h>
#include <yaz/ill.h>


#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

typedef enum {
    zoom_pending,
    zoom_complete
} zoom_ret;

static zoom_ret ZOOM_connection_send_init (ZOOM_connection c);
static zoom_ret do_write_ex (ZOOM_connection c, char *buf_out, int len_out);

static ZOOM_Event ZOOM_Event_create (int kind)
{
    ZOOM_Event event = (ZOOM_Event) xmalloc (sizeof(*event));
    event->kind = kind;
    event->next = 0;
    event->prev = 0;
    return event;
}

static void ZOOM_Event_destroy (ZOOM_Event event)
{
    xfree (event);
}

static void ZOOM_connection_put_event (ZOOM_connection c, ZOOM_Event event)
{
    if (c->m_queue_back)
    {
	c->m_queue_back->prev = event;
	assert (c->m_queue_front);
    }
    else
    {
	assert (!c->m_queue_front);
	c->m_queue_front = event;
    }
    event->next = c->m_queue_back;
    event->prev = 0;
    c->m_queue_back = event;
}

static ZOOM_Event ZOOM_connection_get_event(ZOOM_connection c)
{
    ZOOM_Event event = c->m_queue_front;
    if (!event)
	return 0;
    assert (c->m_queue_back);
    c->m_queue_front = event->prev;
    if (c->m_queue_front)
    {
	assert (c->m_queue_back);
	c->m_queue_front->next = 0;
    }
    else
	c->m_queue_back = 0;
    c->last_event = event->kind;
    return event;
}

static void set_bib1_error (ZOOM_connection c, int error)
{
    xfree (c->addinfo);
    c->addinfo = 0;
    c->error = error;
    c->diagset = "Bib-1";
}

static void set_ZOOM_error (ZOOM_connection c, int error,
		            const char *addinfo)
{
    xfree (c->addinfo);
    c->addinfo = 0;
    c->error = error;
    c->diagset = "ZOOM";
    if (addinfo)
        c->addinfo = xstrdup(addinfo);
}

static void clear_error (ZOOM_connection c)
{

    switch (c->error)
    {
    case ZOOM_ERROR_CONNECT:
    case ZOOM_ERROR_MEMORY:
    case ZOOM_ERROR_DECODE:
    case ZOOM_ERROR_CONNECTION_LOST:
    case ZOOM_ERROR_INIT:
    case ZOOM_ERROR_INTERNAL:
    case ZOOM_ERROR_UNSUPPORTED_PROTOCOL:
        break;
    default:
        set_ZOOM_error(c, ZOOM_ERROR_NONE, 0);
    }
}

ZOOM_task ZOOM_connection_add_task (ZOOM_connection c, int which)
{
    ZOOM_task *taskp = &c->tasks;
    while (*taskp)
	taskp = &(*taskp)->next;
    *taskp = (ZOOM_task) xmalloc (sizeof(**taskp));
    (*taskp)->running = 0;
    (*taskp)->which = which;
    (*taskp)->next = 0;
    clear_error (c);
    return *taskp;
}

ZOOM_task ZOOM_connection_insert_task (ZOOM_connection c, int which)
{
    ZOOM_task task = (ZOOM_task) xmalloc (sizeof(*task));

    task->next = c->tasks;
    c->tasks = task;

    task->running = 0;
    task->which = which;
    clear_error (c);
    return task;
}

void ZOOM_connection_remove_task (ZOOM_connection c)
{
    ZOOM_task task = c->tasks;

    if (task)
    {
	c->tasks = task->next;
	switch (task->which)
	{
	case ZOOM_TASK_SEARCH:
	    ZOOM_resultset_destroy (task->u.search.resultset);
	    break;
	case ZOOM_TASK_RETRIEVE:
	    ZOOM_resultset_destroy (task->u.retrieve.resultset);
	    break;
        case ZOOM_TASK_CONNECT:
            break;
        case ZOOM_TASK_SCAN:
            ZOOM_scanset_destroy (task->u.scan.scan);
            break;
        case ZOOM_TASK_PACKAGE:
            ZOOM_package_destroy (task->u.package);
            break;
	default:
	    assert (0);
	}
	xfree (task);
    }
}

void ZOOM_connection_remove_tasks (ZOOM_connection c)
{
    while (c->tasks)
	ZOOM_connection_remove_task(c);
}

static ZOOM_record record_cache_lookup (ZOOM_resultset r, int pos);

ZOOM_API(ZOOM_connection)
ZOOM_connection_create (ZOOM_options options)
{
    ZOOM_connection c = (ZOOM_connection) xmalloc (sizeof(*c));

    c->soap = 0;
    c->cs = 0;
    c->mask = 0;
    c->reconnect_ok = 0;
    c->state = STATE_IDLE;
    c->addinfo = 0;
    set_ZOOM_error(c, ZOOM_ERROR_NONE, 0);
    c->buf_in = 0;
    c->len_in = 0;
    c->buf_out = 0;
    c->len_out = 0;
    c->resultsets = 0;

    c->options = ZOOM_options_create_with_parent(options);

    c->host_port = 0;
    c->proxy = 0;
    
    c->charset = c->lang = 0;

    c->cookie_out = 0;
    c->cookie_in = 0;
    c->client_IP = 0;
    c->tasks = 0;

    c->odr_in = odr_createmem (ODR_DECODE);
    c->odr_out = odr_createmem (ODR_ENCODE);

    c->async = 0;
    c->support_named_resultsets = 0;
    c->last_event = ZOOM_EVENT_NONE;

    c->m_queue_front = 0;
    c->m_queue_back = 0;
    return c;
}

/* set database names. Take local databases (if set); otherwise
   take databases given in ZURL (if set); otherwise use Default */
static char **set_DatabaseNames (ZOOM_connection con, ZOOM_options options,
                                 int *num)
{
    char **databaseNames;
    const char *c;
    int no = 2;
    const char *cp = ZOOM_options_get (options, "databaseName");
    
    if (!cp || !*cp)
    {
        if (strncmp (con->host_port, "unix:", 5) == 0)
	    cp = strchr (con->host_port+5, ':');
	else
	    cp = strchr (con->host_port, '/');
	if (cp)
	    cp++;
    }
    if (cp)
    {
	c = cp;
	while ((c = strchr(c, '+')))
	{
	    c++;
	    no++;
	}
    }
    else
	cp = "Default";
    databaseNames = (char**)
        odr_malloc (con->odr_out, no * sizeof(*databaseNames));
    no = 0;
    while (*cp)
    {
	c = strchr (cp, '+');
	if (!c)
	    c = cp + strlen(cp);
	else if (c == cp)
	{
	    cp++;
	    continue;
	}
	/* cp ptr to first char of db name, c is char
	   following db name */
	databaseNames[no] = (char*) odr_malloc (con->odr_out, 1+c-cp);
	memcpy (databaseNames[no], cp, c-cp);
	databaseNames[no++][c-cp] = '\0';
	cp = c;
	if (*cp)
	    cp++;
    }
    databaseNames[no] = NULL;
    *num = no;
    return databaseNames;
}

ZOOM_API(ZOOM_connection)
ZOOM_connection_new (const char *host, int portnum)
{
    ZOOM_connection c = ZOOM_connection_create (0);

    ZOOM_connection_connect (c, host, portnum);
    return c;
}

ZOOM_API(void)
ZOOM_connection_connect(ZOOM_connection c,
                        const char *host, int portnum)
{
    const char *val;
    ZOOM_task task;

    if (c->cs)
    {
        yaz_log (LOG_DEBUG, "reconnect");
        c->reconnect_ok = 1;
        return;
    }
    yaz_log(LOG_DEBUG, "connect");
    xfree (c->proxy);
    val = ZOOM_options_get (c->options, "proxy");
    if (val && *val)
	c->proxy = xstrdup (val);
    else
	c->proxy = 0;

    xfree (c->charset);
    val = ZOOM_options_get (c->options, "charset");
    if (val && *val)
	c->charset = xstrdup (val);
    else
	c->charset = 0;

    xfree (c->lang);
    val = ZOOM_options_get (c->options, "lang");
    if (val && *val)
	c->lang = xstrdup (val);
    else
	c->lang = 0;

    xfree (c->host_port);
    if (portnum)
    {
	char hostn[128];
	sprintf (hostn, "%.80s:%d", host, portnum);
	c->host_port = xstrdup(hostn);
    }
    else
	c->host_port = xstrdup(host);

    ZOOM_options_set(c->options, "host", c->host_port);

    val = ZOOM_options_get (c->options, "cookie");
    if (val && *val)
        c->cookie_out = xstrdup (val);

    val = ZOOM_options_get (c->options, "clientIP");
    if (val && *val)
        c->client_IP = xstrdup (val);

    c->async = ZOOM_options_get_bool (c->options, "async", 0);
 
    set_ZOOM_error(c, ZOOM_ERROR_NONE, 0);

    task = ZOOM_connection_add_task (c, ZOOM_TASK_CONNECT);

    if (!c->async)
    {
	while (ZOOM_event (1, &c))
	    ;
    }
}

ZOOM_API(ZOOM_query)
ZOOM_query_create(void)
{
    ZOOM_query s = (ZOOM_query) xmalloc (sizeof(*s));

    s->refcount = 1;
    s->z_query = 0;
    s->sort_spec = 0;
    s->odr = odr_createmem (ODR_ENCODE);

    return s;
}

ZOOM_API(void)
ZOOM_query_destroy(ZOOM_query s)
{
    if (!s)
	return;

    (s->refcount)--;
    yaz_log (LOG_DEBUG, "ZOOM_query_destroy count=%d", s->refcount);
    if (s->refcount == 0)
    {
	odr_destroy (s->odr);
	xfree (s);
    }
}

ZOOM_API(int)
ZOOM_query_prefix(ZOOM_query s, const char *str)
{
    s->z_query = (Z_Query *) odr_malloc (s->odr, sizeof(*s->z_query));
    s->z_query->which = Z_Query_type_1;
    s->z_query->u.type_1 =  p_query_rpn(s->odr, PROTO_Z3950, str);
    if (!s->z_query->u.type_1)
	return -1;
    return 0;
}

ZOOM_API(int)
ZOOM_query_cql(ZOOM_query s, const char *str)
{
    Z_External *ext;
    char *buf;
    int len;

    ext = (Z_External *) odr_malloc(s->odr, sizeof(*ext));
    ext->direct_reference = odr_getoidbystr(s->odr, "1.2.840.10003.16.2");
    ext->indirect_reference = 0;
    ext->descriptor = 0;
    ext->which = Z_External_CQL;
    ext->u.cql = odr_strdup(s->odr, str);
    
    s->z_query = (Z_Query *) odr_malloc (s->odr, sizeof(*s->z_query));
    s->z_query->which = Z_Query_type_104;
    s->z_query->u.type_104 =  ext;
    return 0;
}

ZOOM_API(int)
ZOOM_query_sortby(ZOOM_query s, const char *criteria)
{
    s->sort_spec = yaz_sort_spec (s->odr, criteria);
    if (!s->sort_spec)
	return -1;
    return 0;
}

static zoom_ret do_write(ZOOM_connection c);

ZOOM_API(void)
ZOOM_connection_destroy(ZOOM_connection c)
{
    ZOOM_resultset r;
    if (!c)
	return;
#if HAVE_GSOAP
    if (c->soap)
        soap_end(c->soap);
#endif
    if (c->cs)
	cs_close (c->cs);
    for (r = c->resultsets; r; r = r->next)
	r->connection = 0;

    xfree (c->buf_in);
    xfree (c->addinfo);
    odr_destroy (c->odr_in);
    odr_destroy (c->odr_out);
    ZOOM_options_destroy (c->options);
    ZOOM_connection_remove_tasks (c);
    xfree (c->host_port);
    xfree (c->proxy);
    xfree (c->charset);
    xfree (c->lang);
    xfree (c->cookie_out);
    xfree (c->cookie_in);
    xfree (c->client_IP);
    xfree (c);
}

void ZOOM_resultset_addref (ZOOM_resultset r)
{
    if (r)
    {
	(r->refcount)++;
        yaz_log (LOG_DEBUG, "ZOOM_resultset_addref r=%p count=%d",
                 r, r->refcount);
    }
}
ZOOM_resultset ZOOM_resultset_create ()
{
    ZOOM_resultset r = (ZOOM_resultset) xmalloc (sizeof(*r));

    yaz_log (LOG_DEBUG, "ZOOM_resultset_create r = %p", r);
    r->refcount = 1;
    r->size = 0;
    r->odr = odr_createmem (ODR_ENCODE);
    r->start = 0;
    r->piggyback = 1;
    r->setname = 0;
    r->count = 0;
    r->record_cache = 0;
    r->r_sort_spec = 0;
    r->z_query = 0;
    r->search = 0;
    r->connection = 0;
    r->next = 0;
    return r;
}

ZOOM_API(ZOOM_resultset)
ZOOM_connection_search_pqf(ZOOM_connection c, const char *q)
{
    ZOOM_resultset r;
    ZOOM_query s = ZOOM_query_create();

    ZOOM_query_prefix (s, q);

    r = ZOOM_connection_search (c, s);
    ZOOM_query_destroy (s);
    return r;
}

ZOOM_API(ZOOM_resultset)
ZOOM_connection_search(ZOOM_connection c, ZOOM_query q)
{
    ZOOM_resultset r = ZOOM_resultset_create ();
    ZOOM_task task;
    const char *cp;

    r->r_sort_spec = q->sort_spec;
    r->z_query = q->z_query;
    r->search = q;

    r->options = ZOOM_options_create_with_parent(c->options);

    r->start = ZOOM_options_get_int(r->options, "start", 0);
    r->count = ZOOM_options_get_int(r->options, "count", 0);
    r->piggyback = ZOOM_options_get_bool (r->options, "piggyback", 1);
    cp = ZOOM_options_get (r->options, "setname");
    if (cp)
        r->setname = xstrdup (cp);
    
    r->connection = c;

    r->next = c->resultsets;
    c->resultsets = r;

    task = ZOOM_connection_add_task (c, ZOOM_TASK_SEARCH);
    task->u.search.resultset = r;
    ZOOM_resultset_addref (r);  

    (q->refcount)++;

    if (!c->async)
    {
	while (ZOOM_event (1, &c))
	    ;
    }
    return r;
}

ZOOM_API(void)
ZOOM_resultset_destroy(ZOOM_resultset r)
{
    if (!r)
        return;
    (r->refcount)--;
    yaz_log (LOG_DEBUG, "ZOOM_resultset_destroy r = %p count=%d",
             r, r->refcount);
    if (r->refcount == 0)
    {
        ZOOM_record_cache rc;

        for (rc = r->record_cache; rc; rc = rc->next)
            if (rc->rec.wrbuf_marc)
                wrbuf_free (rc->rec.wrbuf_marc, 1);
	if (r->connection)
	{
	    /* remove ourselves from the resultsets in connection */
	    ZOOM_resultset *rp = &r->connection->resultsets;
	    while (1)
	    {
		assert (*rp);   /* we must be in this list!! */
		if (*rp == r)
		{   /* OK, we're here - take us out of it */
		    *rp = (*rp)->next;
		    break;
		}
		rp = &(*rp)->next;
	    }
	}
	ZOOM_query_destroy (r->search);
	ZOOM_options_destroy (r->options);
	odr_destroy (r->odr);
        xfree (r->setname);
	xfree (r);
    }
}

ZOOM_API(size_t)
ZOOM_resultset_size (ZOOM_resultset r)
{
    return r->size;
}

static void do_close (ZOOM_connection c)
{
    if (c->cs)
	cs_close(c->cs);
    c->cs = 0;
    c->mask = 0;
    c->state = STATE_IDLE;
}

static void ZOOM_resultset_retrieve (ZOOM_resultset r,
                                     int force_sync, int start, int count)
{
    ZOOM_task task;
    ZOOM_connection c;

    if (!r)
	return;
    c = r->connection;
    if (!c)
	return;
    task = ZOOM_connection_add_task (c, ZOOM_TASK_RETRIEVE);
    task->u.retrieve.resultset = r;
    task->u.retrieve.start = start;
    task->u.retrieve.count = count;

    ZOOM_resultset_addref (r);

    if (!r->connection->async || force_sync)
	while (r->connection && ZOOM_event (1, &r->connection))
	    ;
}

ZOOM_API(void)
ZOOM_resultset_records (ZOOM_resultset r, ZOOM_record *recs,
                        size_t start, size_t count)
{
    int force_present = 0;

    if (!r)
	return ;
    if (count && recs)
        force_present = 1;
    ZOOM_resultset_retrieve (r, force_present, start, count);
    if (force_present)
    {
        size_t i;
        for (i = 0; i< count; i++)
            recs[i] = ZOOM_resultset_record_immediate (r, i+start);
    }
}

static zoom_ret do_connect (ZOOM_connection c)
{
    void *add;
    const char *effective_host;

    if (c->proxy)
	effective_host = c->proxy;
    else
	effective_host = c->host_port;

    yaz_log (LOG_DEBUG, "do_connect host=%s", effective_host);

    assert (!c->cs);

    if (memcmp(c->host_port, "http:", 5) == 0)
    {
#if HAVE_GSOAP
        c->soap = soap_new();
        c->soap->namespaces = srw_namespaces;
#else
        c->state = STATE_IDLE;
        set_ZOOM_error(c, ZOOM_ERROR_UNSUPPORTED_PROTOCOL, "SRW");
#endif
    }
    else
    {
        c->cs = cs_create_host (effective_host, 0, &add);
        
        if (c->cs)
        {
            int ret = cs_connect (c->cs, add);
            if (ret == 0)
            {
                ZOOM_Event event = ZOOM_Event_create(ZOOM_EVENT_CONNECT);
                ZOOM_connection_put_event(c, event);
                ZOOM_connection_send_init(c);
                c->state = STATE_ESTABLISHED;
                return zoom_pending;
            }
            else if (ret > 0)
            {
                c->state = STATE_CONNECTING; 
                c->mask = ZOOM_SELECT_EXCEPT;
                if (c->cs->io_pending & CS_WANT_WRITE)
                    c->mask += ZOOM_SELECT_WRITE;
                if (c->cs->io_pending & CS_WANT_READ)
                    c->mask += ZOOM_SELECT_READ;
                return zoom_pending;
            }
        }
        c->state = STATE_IDLE;
        set_ZOOM_error(c, ZOOM_ERROR_CONNECT, effective_host);
    }
    return zoom_complete;
}

int z3950_connection_socket(ZOOM_connection c)
{
    if (c->cs)
	return cs_fileno(c->cs);
    return -1;
}

int z3950_connection_mask(ZOOM_connection c)
{
    if (c->cs)
	return c->mask;
    return 0;
}

static void otherInfo_attach (ZOOM_connection c, Z_APDU *a, ODR out)
{
    int i;
    for (i = 0; i<200; i++)
    {
        size_t len;
	Z_OtherInformation **oi;
        char buf[80];
        const char *val;
        const char *cp;
        int oidval;

        sprintf (buf, "otherInfo%d", i);
        val = ZOOM_options_get (c->options, buf);
        if (!val)
            break;
        cp = strchr (val, ':');
        if (!cp)
            continue;
        len = cp - val;
        if (len >= sizeof(buf))
            len = sizeof(buf)-1;
        memcpy (buf, val, len);
        buf[len] = '\0';
        oidval = oid_getvalbyname (buf);
        if (oidval == VAL_NONE)
            continue;
        
	yaz_oi_APDU(a, &oi);
	yaz_oi_set_string_oidval(oi, out, oidval, 1, cp+1);
    }
}

static int encode_APDU(ZOOM_connection c, Z_APDU *a, ODR out)
{
    assert (a);
    if (c->cookie_out)
    {
	Z_OtherInformation **oi;
	yaz_oi_APDU(a, &oi);
	yaz_oi_set_string_oidval(oi, out, VAL_COOKIE, 1, c->cookie_out);
    }
    if (c->client_IP)
    {
	Z_OtherInformation **oi;
	yaz_oi_APDU(a, &oi);
	yaz_oi_set_string_oidval(oi, out, VAL_CLIENT_IP, 1, c->client_IP);
    }
    otherInfo_attach (c, a, out);
    if (!z_APDU(out, &a, 0, 0))
    {
	FILE *outf = fopen("/tmp/apdu.txt", "a");
	if (a && outf)
	{
	    ODR odr_pr = odr_createmem(ODR_PRINT);
	    fprintf (outf, "a=%p\n", a);
	    odr_setprint(odr_pr, outf);
	    z_APDU(odr_pr, &a, 0, 0);
	    odr_destroy(odr_pr);
	}
        yaz_log (LOG_DEBUG, "encoding failed");
        set_ZOOM_error(c, ZOOM_ERROR_ENCODE, 0);
	odr_reset(out);
	return -1;
    }
    
    return 0;
}

static zoom_ret send_APDU (ZOOM_connection c, Z_APDU *a)
{
    ZOOM_Event event;
    assert (a);
    if (encode_APDU(c, a, c->odr_out))
	return zoom_complete;
    c->buf_out = odr_getbuf(c->odr_out, &c->len_out, 0);
    event = ZOOM_Event_create (ZOOM_EVENT_SEND_APDU);
    ZOOM_connection_put_event (c, event);
    odr_reset(c->odr_out);
    return do_write (c);
}

/* returns 1 if PDU was sent OK (still pending )
           0 if PDU was not sent OK (nothing to wait for) 
*/

static zoom_ret ZOOM_connection_send_init (ZOOM_connection c)
{
    const char *impname;
    Z_APDU *apdu = zget_APDU(c->odr_out, Z_APDU_initRequest);
    Z_InitRequest *ireq = apdu->u.initRequest;
    Z_IdAuthentication *auth = (Z_IdAuthentication *)
        odr_malloc(c->odr_out, sizeof(*auth));
    const char *auth_groupId = ZOOM_options_get (c->options, "group");
    const char *auth_userId = ZOOM_options_get (c->options, "user");
    const char *auth_password = ZOOM_options_get (c->options, "pass");
    
    ODR_MASK_SET(ireq->options, Z_Options_search);
    ODR_MASK_SET(ireq->options, Z_Options_present);
    ODR_MASK_SET(ireq->options, Z_Options_scan);
    ODR_MASK_SET(ireq->options, Z_Options_sort);
    ODR_MASK_SET(ireq->options, Z_Options_extendedServices);
    ODR_MASK_SET(ireq->options, Z_Options_namedResultSets);
    
    ODR_MASK_SET(ireq->protocolVersion, Z_ProtocolVersion_1);
    ODR_MASK_SET(ireq->protocolVersion, Z_ProtocolVersion_2);
    ODR_MASK_SET(ireq->protocolVersion, Z_ProtocolVersion_3);
    
    impname = ZOOM_options_get (c->options, "implementationName");
    ireq->implementationName =
	(char *) odr_malloc (c->odr_out, 15 + (impname ? strlen(impname) : 0));
    strcpy (ireq->implementationName, "");
    if (impname)
    {
	strcat (ireq->implementationName, impname);
	strcat (ireq->implementationName, "/");
    }					       
    strcat (ireq->implementationName, "ZOOM-C/YAZ");
    
    *ireq->maximumRecordSize =
	ZOOM_options_get_int (c->options, "maximumRecordSize", 1024*1024);
    *ireq->preferredMessageSize =
	ZOOM_options_get_int (c->options, "preferredMessageSize", 1024*1024);
    
    if (auth_groupId || auth_password)
    {
	Z_IdPass *pass = (Z_IdPass *) odr_malloc(c->odr_out, sizeof(*pass));
	int i = 0;
	pass->groupId = 0;
	if (auth_groupId && *auth_groupId)
	{
	    pass->groupId = (char *)
                odr_malloc(c->odr_out, strlen(auth_groupId)+1);
	    strcpy(pass->groupId, auth_groupId);
	    i++;
	}
	pass->userId = 0;
	if (auth_userId && *auth_userId)
	{
	    pass->userId = (char *)
                odr_malloc(c->odr_out, strlen(auth_userId)+1);
	    strcpy(pass->userId, auth_userId);
	    i++;
	}
	pass->password = 0;
	if (auth_password && *auth_password)
	{
	    pass->password = (char *)
                odr_malloc(c->odr_out, strlen(auth_password)+1);
	    strcpy(pass->password, auth_password);
	    i++;
	}
	if (i)
	{
	    auth->which = Z_IdAuthentication_idPass;
	    auth->u.idPass = pass;
	    ireq->idAuthentication = auth;
	}
    }
    else if (auth_userId)
    {
	auth->which = Z_IdAuthentication_open;
	auth->u.open = (char *)
            odr_malloc(c->odr_out, strlen(auth_userId)+1);
	strcpy(auth->u.open, auth_userId);
	ireq->idAuthentication = auth;
    }
    if (c->proxy)
	yaz_oi_set_string_oidval(&ireq->otherInfo, c->odr_out,
				 VAL_PROXY, 1, c->host_port);
    if (c->charset||c->lang)
    {
    	Z_OtherInformation **oi;
    	Z_OtherInformationUnit *oi_unit;
    	
    	yaz_oi_APDU(apdu, &oi);
    	
    	if ((oi_unit = yaz_oi_update(oi, c->odr_out, NULL, 0, 0)))
    	{
            ODR_MASK_SET(ireq->options, Z_Options_negotiationModel);
            
            oi_unit->which = Z_OtherInfo_externallyDefinedInfo;
            oi_unit->information.externallyDefinedInfo =
                yaz_set_proposal_charneg
                (c->odr_out,
                 (const char **)&c->charset, (c->charset) ? 1:0,
                 (const char **)&c->lang, (c->lang) ? 1:0, 1);
    	}
    }
    assert (apdu);
    return send_APDU (c, apdu);
}

static zoom_ret ZOOM_connection_send_search (ZOOM_connection c)
{
    ZOOM_resultset r;
    int lslb, ssub, mspn;
    const char *syntax;
    Z_APDU *apdu = zget_APDU(c->odr_out, Z_APDU_searchRequest);
    Z_SearchRequest *search_req = apdu->u.searchRequest;
    const char *elementSetName;
    const char *smallSetElementSetName;
    const char *mediumSetElementSetName;
    const char *schema;

    assert (c->tasks);
    assert (c->tasks->which == ZOOM_TASK_SEARCH);

    r = c->tasks->u.search.resultset;

    elementSetName =
	ZOOM_options_get (r->options, "elementSetName");
    smallSetElementSetName  =
	ZOOM_options_get (r->options, "smallSetElementSetName");
    mediumSetElementSetName =
	ZOOM_options_get (r->options, "mediumSetElementSetName");
    schema =
	ZOOM_options_get (r->options, "schema");

    if (!smallSetElementSetName)
	smallSetElementSetName = elementSetName;

    if (!mediumSetElementSetName)
	mediumSetElementSetName = elementSetName;

    assert (r);
    assert (r->z_query);

    /* prepare query for the search request */
    search_req->query = r->z_query;

    search_req->databaseNames =
	set_DatabaseNames (c, r->options, &search_req->num_databaseNames);

    /* get syntax (no need to provide unless piggyback is in effect) */
    syntax = ZOOM_options_get (r->options, "preferredRecordSyntax");

    lslb = ZOOM_options_get_int (r->options, "largeSetLowerBound", -1);
    ssub = ZOOM_options_get_int (r->options, "smallSetUpperBound", -1);
    mspn = ZOOM_options_get_int (r->options, "mediumSetPresentNumber", -1);
    if (lslb != -1 && ssub != -1 && mspn != -1)
    {
	/* So're a Z39.50 expert? Let's hope you don't do sort */
	*search_req->largeSetLowerBound = lslb;
	*search_req->smallSetUpperBound = ssub;
	*search_req->mediumSetPresentNumber = mspn;
    }
    else if (r->start == 0 && r->count > 0
	     && r->piggyback && !r->r_sort_spec && !schema)
    {
	/* Regular piggyback - do it unless we're going to do sort */
	*search_req->largeSetLowerBound = 2000000000;
	*search_req->smallSetUpperBound = r->count;
	*search_req->mediumSetPresentNumber = r->count;
    }
    else
    {
	/* non-piggyback. Need not provide elementsets or syntaxes .. */
	smallSetElementSetName = 0;
	mediumSetElementSetName = 0;
	syntax = 0;
    }
    if (smallSetElementSetName && *smallSetElementSetName)
    {
	Z_ElementSetNames *esn = (Z_ElementSetNames *)
            odr_malloc (c->odr_out, sizeof(*esn));
	
	esn->which = Z_ElementSetNames_generic;
	esn->u.generic = odr_strdup (c->odr_out, smallSetElementSetName);
	search_req->smallSetElementSetNames = esn;
    }
    if (mediumSetElementSetName && *mediumSetElementSetName)
    {
	Z_ElementSetNames *esn = (Z_ElementSetNames *)
            odr_malloc (c->odr_out, sizeof(*esn));
	
	esn->which = Z_ElementSetNames_generic;
	esn->u.generic = odr_strdup (c->odr_out, mediumSetElementSetName);
	search_req->mediumSetElementSetNames = esn;
    }
    if (syntax)
	search_req->preferredRecordSyntax =
	    yaz_str_to_z3950oid (c->odr_out, CLASS_RECSYN, syntax);
    
    if (!r->setname)
    {
        if (c->support_named_resultsets)
        {
            char setname[14];
            int ord;
            /* find the lowest unused ordinal so that we re-use
               result sets on the server. */
            for (ord = 1; ; ord++)
            {
                ZOOM_resultset rp;
                sprintf (setname, "%d", ord);
                for (rp = c->resultsets; rp; rp = rp->next)
                    if (rp->setname && !strcmp (rp->setname, setname))
                        break;
                if (!rp)
                    break;
            }
            r->setname = xstrdup (setname);
            yaz_log (LOG_DEBUG, "allocating set %s", r->setname);
        }
        else
            r->setname = xstrdup ("default");
        ZOOM_options_set (r->options, "setname", r->setname);
    }
    search_req->resultSetName = odr_strdup(c->odr_out, r->setname);
    /* send search request */
    return send_APDU (c, apdu);
}

static void response_diag (ZOOM_connection c, Z_DiagRec *p)
{
    int oclass;
    Z_DefaultDiagFormat *r;
    char *addinfo = 0;
    
    xfree (c->addinfo);
    c->addinfo = 0;
    if (p->which != Z_DiagRec_defaultFormat)
    {
        set_ZOOM_error(c, ZOOM_ERROR_DECODE, 0);
	return;
    }
    r = p->u.defaultFormat;
    c->diagset = yaz_z3950oid_to_str(r->diagnosticSetId, &oclass);

    switch (r->which)
    {
    case Z_DefaultDiagFormat_v2Addinfo:
	addinfo = r->u.v2Addinfo;
	break;
    case Z_DefaultDiagFormat_v3Addinfo:
	addinfo = r->u.v3Addinfo;
	break;
    }
    if (addinfo)
	c->addinfo = xstrdup (addinfo);
    c->error = *r->condition;
}

ZOOM_API(ZOOM_record)
ZOOM_record_clone (ZOOM_record srec)
{
    char *buf;
    int size;
    ODR odr_enc;
    ZOOM_record nrec;

    odr_enc = odr_createmem(ODR_ENCODE);
    if (!z_NamePlusRecord (odr_enc, &srec->npr, 0, 0))
	return 0;
    buf = odr_getbuf (odr_enc, &size, 0);
    
    nrec = (ZOOM_record) xmalloc (sizeof(*nrec));
    nrec->odr = odr_createmem(ODR_DECODE);
    nrec->wrbuf_marc = 0;
    odr_setbuf (nrec->odr, buf, size, 0);
    z_NamePlusRecord (nrec->odr, &nrec->npr, 0, 0);
    
    odr_destroy (odr_enc);
    return nrec;
}

ZOOM_API(ZOOM_record)
ZOOM_resultset_record_immediate (ZOOM_resultset s,size_t pos)
{
    return record_cache_lookup (s, pos);
}

ZOOM_API(ZOOM_record)
ZOOM_resultset_record (ZOOM_resultset r, size_t pos)
{
    ZOOM_resultset_retrieve (r, 1, pos, 1);
    return ZOOM_resultset_record_immediate (r, pos);
}

ZOOM_API(void)
ZOOM_record_destroy (ZOOM_record rec)
{
    if (!rec)
	return;
    if (rec->wrbuf_marc)
	wrbuf_free (rec->wrbuf_marc, 1);
    odr_destroy (rec->odr);
    xfree (rec);
}

ZOOM_API(const char *)
ZOOM_record_get (ZOOM_record rec, const char *type, int *len)
{
    Z_NamePlusRecord *npr;
    
    if (len)
    	*len = 0; /* default return */
    	
    if (!rec)
	return 0;
    npr = rec->npr;
    if (!npr)
	return 0;
    if (!strcmp (type, "database"))
    {
    	if (len)
            *len = (npr->databaseName ? strlen(npr->databaseName) : 0);
	return npr->databaseName;
    }
    else if (!strcmp (type, "syntax"))
    {
        const char *desc = 0;	
	if (npr->which == Z_NamePlusRecord_databaseRecord)
	{
	    Z_External *r = (Z_External *) npr->u.databaseRecord;
	    oident *ent = oid_getentbyoid(r->direct_reference);
	    if (ent)
	    	desc = ent->desc;
	}
	if (!desc)
            desc = "none";
    	if (len)
            *len = strlen(desc);
	return desc;
    }
    else if (!strcmp (type, "render") && 
             npr->which == Z_NamePlusRecord_databaseRecord)
    {
        Z_External *r = (Z_External *) npr->u.databaseRecord;
        oident *ent = oid_getentbyoid(r->direct_reference);
        
        if (r->which == Z_External_sutrs)
        {
            if (len) *len = r->u.sutrs->len;
            return (const char *) r->u.sutrs->buf;
        }
        else if (r->which == Z_External_octet)
        {
            yaz_marc_t mt;
            switch (ent->value)
            {
            case VAL_SOIF:
            case VAL_HTML:
            case VAL_SUTRS:
                break;
            case VAL_TEXT_XML:
            case VAL_APPLICATION_XML:
                break;
            default:
                if (!rec->wrbuf_marc)
                    rec->wrbuf_marc = wrbuf_alloc();

                mt = yaz_marc_create();
                wrbuf_rewind (rec->wrbuf_marc);
                if (yaz_marc_decode_wrbuf (
                        mt, (const char *) r->u.octet_aligned->buf,
                        r->u.octet_aligned->len,
                        rec->wrbuf_marc) > 0)
                {
                    if (len)
                        *len = wrbuf_len(rec->wrbuf_marc);
                    yaz_marc_destroy(mt);
                    return wrbuf_buf(rec->wrbuf_marc);
                }
                yaz_marc_destroy(mt);
            }
            if (len) 
                *len = r->u.octet_aligned->len;
            return (const char *) r->u.octet_aligned->buf;
        }
        else if (r->which == Z_External_grs1)
        {
            if (!rec->wrbuf_marc)
                rec->wrbuf_marc = wrbuf_alloc();
            wrbuf_rewind (rec->wrbuf_marc);
            yaz_display_grs1(rec->wrbuf_marc, r->u.grs1, 0);
            if (len) 
                *len = wrbuf_len(rec->wrbuf_marc);
            return wrbuf_buf(rec->wrbuf_marc);
        }
	return 0;
    }
    else if (npr->which == Z_NamePlusRecord_databaseRecord &&
             (!strcmp (type, "xml") || !strcmp(type, "oai")))
    {
        Z_External *r = (Z_External *) npr->u.databaseRecord;
        oident *ent = oid_getentbyoid(r->direct_reference);
        
        if (r->which == Z_External_sutrs)
        {
            if (len) *len = r->u.sutrs->len;
            return (const char *) r->u.sutrs->buf;
        }
        else if (r->which == Z_External_octet)
        {
            yaz_marc_t mt;
            int marc_decode_type = YAZ_MARC_MARCXML;

            if (!strcmp(type, "oai"))
                marc_decode_type = YAZ_MARC_OAIMARC;
            switch (ent->value)
            {
            case VAL_SOIF:
            case VAL_HTML:
            case VAL_SUTRS:
                break;
            case VAL_TEXT_XML:
            case VAL_APPLICATION_XML:
                break;
            default:
                if (!rec->wrbuf_marc)
                    rec->wrbuf_marc = wrbuf_alloc();
                wrbuf_rewind (rec->wrbuf_marc);
                mt = yaz_marc_create();

                yaz_marc_xml(mt, YAZ_MARC_MARCXML);
                if (yaz_marc_decode_wrbuf (
                        mt, (const char *) r->u.octet_aligned->buf,
                        r->u.octet_aligned->len,
                        rec->wrbuf_marc) > 0)
                {
                    if (len) 
                        *len = wrbuf_len(rec->wrbuf_marc);
                    yaz_marc_destroy(mt);
                    return wrbuf_buf(rec->wrbuf_marc);
                }
                yaz_marc_destroy(mt);
            }
            if (len) *len = r->u.octet_aligned->len;
            return (const char *) r->u.octet_aligned->buf;
        }
        else if (r->which == Z_External_grs1)
        {
            if (len) *len = 5;
            return "GRS-1";
        }
	return 0;
    }
    else if (!strcmp (type, "raw"))
    {
	if (npr->which == Z_NamePlusRecord_databaseRecord)
	{
	    Z_External *r = (Z_External *) npr->u.databaseRecord;
	    
	    if (r->which == Z_External_sutrs)
	    {
		if (len) *len = r->u.sutrs->len;
		return (const char *) r->u.sutrs->buf;
	    }
	    else if (r->which == Z_External_octet)
	    {
		if (len) *len = r->u.octet_aligned->len;
		return (const char *) r->u.octet_aligned->buf;
	    }
	    else /* grs-1, explain, ... */
	    {
		if (len) *len = -1;
                return (const char *) npr->u.databaseRecord;
	    }
	}
	return 0;
    }
    else if (!strcmp (type, "ext"))
    {
	if (npr->which == Z_NamePlusRecord_databaseRecord)
            return (const char *) npr->u.databaseRecord;
	return 0;
    }
    return 0;
}

static void record_cache_add (ZOOM_resultset r, Z_NamePlusRecord *npr, int pos)
{
    ZOOM_record_cache rc;
    const char *elementSetName =
        ZOOM_resultset_option_get (r, "elementSetName");
    const char *syntax = 
        ZOOM_resultset_option_get (r, "preferredRecordSyntax");
    
    for (rc = r->record_cache; rc; rc = rc->next)
    {
	if (pos == rc->pos)
	{
	    if ((!elementSetName && !rc->elementSetName)
		|| (elementSetName && rc->elementSetName &&
		    !strcmp (elementSetName, rc->elementSetName)))
	    {
                if ((!syntax && !rc->syntax)
                    || (syntax && rc->syntax &&
                        !strcmp (syntax, rc->syntax)))
                {
                    /* not destroying rc->npr (it's handled by nmem )*/
                    rc->rec.npr = npr;
                    /* keeping wrbuf_marc too */
                    return;
                }
	    }
	}
    }
    rc = (ZOOM_record_cache) odr_malloc (r->odr, sizeof(*rc));
    rc->rec.npr = npr; 
    rc->rec.odr = 0;
    rc->rec.wrbuf_marc = 0;
    if (elementSetName)
	rc->elementSetName = odr_strdup (r->odr, elementSetName);
    else
	rc->elementSetName = 0;

    if (syntax)
	rc->syntax = odr_strdup (r->odr, syntax);
    else
	rc->syntax = 0;

    rc->pos = pos;
    rc->next = r->record_cache;
    r->record_cache = rc;
}

static ZOOM_record record_cache_lookup (ZOOM_resultset r, int pos)
{
    ZOOM_record_cache rc;
    const char *elementSetName =
        ZOOM_resultset_option_get (r, "elementSetName");
    const char *syntax = 
        ZOOM_resultset_option_get (r, "preferredRecordSyntax");
    
    for (rc = r->record_cache; rc; rc = rc->next)
    {
	if (pos == rc->pos)
	{
	    if ((!elementSetName && !rc->elementSetName)
		|| (elementSetName && rc->elementSetName &&
		    !strcmp (elementSetName, rc->elementSetName)))
            {
                if ((!syntax && !rc->syntax)
                    || (syntax && rc->syntax &&
                        !strcmp (syntax, rc->syntax)))
                    return &rc->rec;
            }
	}
    }
    return 0;
}
					     
static void handle_records (ZOOM_connection c, Z_Records *sr,
			    int present_phase)
{
    ZOOM_resultset resultset;

    if (!c->tasks)
	return ;
    switch (c->tasks->which)
    {
    case ZOOM_TASK_SEARCH:
        resultset = c->tasks->u.search.resultset;
        break;
    case ZOOM_TASK_RETRIEVE:
        resultset = c->tasks->u.retrieve.resultset;        
	break;
    default:
        return;
    }
    if (sr && sr->which == Z_Records_NSD)
    {
	Z_DiagRec dr, *dr_p = &dr;
	dr.which = Z_DiagRec_defaultFormat;
	dr.u.defaultFormat = sr->u.nonSurrogateDiagnostic;
	
	response_diag (c, dr_p);
    }
    else if (sr && sr->which == Z_Records_multipleNSD)
    {
	if (sr->u.multipleNonSurDiagnostics->num_diagRecs >= 1)
	    response_diag(c, sr->u.multipleNonSurDiagnostics->diagRecs[0]);
	else
            set_ZOOM_error(c, ZOOM_ERROR_DECODE, 0);
    }
    else 
    {
	if (resultset->count + resultset->start > resultset->size)
	    resultset->count = resultset->size - resultset->start;
	if (resultset->count < 0)
	    resultset->count = 0;
	if (sr && sr->which == Z_Records_DBOSD)
	{
	    int i;
	    NMEM nmem = odr_extract_mem (c->odr_in);
	    Z_NamePlusRecordList *p =
		sr->u.databaseOrSurDiagnostics;
	    for (i = 0; i<p->num_records; i++)
	    {
		record_cache_add (resultset, p->records[i],
                                  i+ resultset->start);
	    }
	    /* transfer our response to search_nmem .. we need it later */
	    nmem_transfer (resultset->odr->mem, nmem);
	    nmem_destroy (nmem);
	    if (present_phase && p->num_records == 0)
	    {
		/* present response and we didn't get any records! */
                set_ZOOM_error(c, ZOOM_ERROR_DECODE, 0);
	    }
	}
	else if (present_phase)
	{
	    /* present response and we didn't get any records! */
            set_ZOOM_error(c, ZOOM_ERROR_DECODE, 0);
	}
    }
}

static void handle_present_response (ZOOM_connection c, Z_PresentResponse *pr)
{
    handle_records (c, pr->records, 1);
}

static void handle_search_response (ZOOM_connection c, Z_SearchResponse *sr)
{
    ZOOM_resultset resultset;

    yaz_log (LOG_DEBUG, "got search response");

    if (!c->tasks || c->tasks->which != ZOOM_TASK_SEARCH)
	return ;

    resultset = c->tasks->u.search.resultset;

    resultset->size = *sr->resultCount;
    handle_records (c, sr->records, 0);
}

static void sort_response (ZOOM_connection c, Z_SortResponse *res)
{
    if (res->diagnostics && res->num_diagnostics > 0)
	response_diag (c, res->diagnostics[0]);
}

static int scan_response (ZOOM_connection c, Z_ScanResponse *res)
{
    NMEM nmem = odr_extract_mem (c->odr_in);
    ZOOM_scanset scan;

    if (!c->tasks || c->tasks->which != ZOOM_TASK_SCAN)
        return 0;
    scan = c->tasks->u.scan.scan;

    if (res->entries && res->entries->nonsurrogateDiagnostics)
        response_diag(c, res->entries->nonsurrogateDiagnostics[0]);
    scan->scan_response = res;
    nmem_transfer (scan->odr->mem, nmem);
    if (res->stepSize)
        ZOOM_options_set_int (scan->options, "stepSize", *res->stepSize);
    if (res->positionOfTerm)
        ZOOM_options_set_int (scan->options, "position", *res->positionOfTerm);
    if (res->scanStatus)
        ZOOM_options_set_int (scan->options, "scanStatus", *res->scanStatus);
    if (res->numberOfEntriesReturned)
        ZOOM_options_set_int (scan->options, "number",
                              *res->numberOfEntriesReturned);
    nmem_destroy (nmem);
    return 1;
}

static zoom_ret send_sort (ZOOM_connection c)
{
    ZOOM_resultset  resultset;

    if (!c->tasks || c->tasks->which != ZOOM_TASK_SEARCH)
	return zoom_complete;

    resultset = c->tasks->u.search.resultset;

    if (c->error)
    {
	resultset->r_sort_spec = 0;
	return zoom_complete;
    }
    if (resultset->r_sort_spec)
    {
	Z_APDU *apdu = zget_APDU(c->odr_out, Z_APDU_sortRequest);
	Z_SortRequest *req = apdu->u.sortRequest;
	
	req->num_inputResultSetNames = 1;
	req->inputResultSetNames = (Z_InternationalString **)
	    odr_malloc (c->odr_out, sizeof(*req->inputResultSetNames));
	req->inputResultSetNames[0] =
            odr_strdup (c->odr_out, resultset->setname);
	req->sortedResultSetName = odr_strdup (c->odr_out, resultset->setname);
	req->sortSequence = resultset->r_sort_spec;
	resultset->r_sort_spec = 0;
	return send_APDU (c, apdu);
    }
    return zoom_complete;
}

static zoom_ret send_present (ZOOM_connection c)
{
    Z_APDU *apdu = 0;
    Z_PresentRequest *req = 0;
    int i = 0;
    const char *syntax = 0;
    const char *elementSetName = 0;
    const char *schema = 0;
    ZOOM_resultset  resultset;

    if (!c->tasks)
	return zoom_complete;

    switch (c->tasks->which)
    {
    case ZOOM_TASK_SEARCH:
        resultset = c->tasks->u.search.resultset;
        break;
    case ZOOM_TASK_RETRIEVE:
        resultset = c->tasks->u.retrieve.resultset;
        resultset->start = c->tasks->u.retrieve.start;
        resultset->count = c->tasks->u.retrieve.count;

        if (resultset->start >= resultset->size)
            return zoom_complete;
        if (resultset->start + resultset->count > resultset->size)
            resultset->count = resultset->size - resultset->start;
	break;
    default:
        return zoom_complete;
    }

    syntax = ZOOM_resultset_option_get (resultset, "preferredRecordSyntax");
    elementSetName = ZOOM_resultset_option_get (resultset, "elementSetName");
    schema = ZOOM_resultset_option_get (resultset, "schema");

    if (c->error)                  /* don't continue on error */
	return zoom_complete;
    if (resultset->start < 0)
	return zoom_complete;
    for (i = 0; i<resultset->count; i++)
    {
	ZOOM_record rec =
	    record_cache_lookup (resultset, i + resultset->start);
	if (!rec)
	    break;
    }
    if (i == resultset->count)
	return zoom_complete;

    apdu = zget_APDU(c->odr_out, Z_APDU_presentRequest);
    req = apdu->u.presentRequest;

    resultset->start += i;
    resultset->count -= i;
    *req->resultSetStartPoint = resultset->start + 1;
    *req->numberOfRecordsRequested = resultset->count;
    assert (*req->numberOfRecordsRequested > 0);

    if (syntax && *syntax)
	req->preferredRecordSyntax =
	    yaz_str_to_z3950oid (c->odr_out, CLASS_RECSYN, syntax);

    if (schema && *schema)
    {
	Z_RecordComposition *compo = (Z_RecordComposition *)
            odr_malloc (c->odr_out, sizeof(*compo));

        req->recordComposition = compo;
        compo->which = Z_RecordComp_complex;
        compo->u.complex = (Z_CompSpec *)
            odr_malloc(c->odr_out, sizeof(*compo->u.complex));
        compo->u.complex->selectAlternativeSyntax = (bool_t *) 
            odr_malloc(c->odr_out, sizeof(bool_t));
        *compo->u.complex->selectAlternativeSyntax = 0;

        compo->u.complex->generic = (Z_Specification *)
            odr_malloc(c->odr_out, sizeof(*compo->u.complex->generic));

        compo->u.complex->generic->schema = (Odr_oid *)
            yaz_str_to_z3950oid (c->odr_out, CLASS_SCHEMA, schema);

        if (!compo->u.complex->generic->schema)
        {
            /* OID wasn't a schema! Try record syntax instead. */

            compo->u.complex->generic->schema = (Odr_oid *)
                yaz_str_to_z3950oid (c->odr_out, CLASS_RECSYN, schema);
        }
        if (elementSetName && *elementSetName)
        {
            compo->u.complex->generic->elementSpec = (Z_ElementSpec *)
                odr_malloc(c->odr_out, sizeof(Z_ElementSpec));
            compo->u.complex->generic->elementSpec->which =
                Z_ElementSpec_elementSetName;
            compo->u.complex->generic->elementSpec->u.elementSetName =
                odr_strdup (c->odr_out, elementSetName);
        }
        else
            compo->u.complex->generic->elementSpec = 0;
        compo->u.complex->num_dbSpecific = 0;
        compo->u.complex->dbSpecific = 0;
        compo->u.complex->num_recordSyntax = 0;
        compo->u.complex->recordSyntax = 0;
    }
    else if (elementSetName && *elementSetName)
    {
	Z_ElementSetNames *esn = (Z_ElementSetNames *)
            odr_malloc (c->odr_out, sizeof(*esn));
	Z_RecordComposition *compo = (Z_RecordComposition *)
            odr_malloc (c->odr_out, sizeof(*compo));
	
	esn->which = Z_ElementSetNames_generic;
	esn->u.generic = odr_strdup (c->odr_out, elementSetName);
	compo->which = Z_RecordComp_simple;
	compo->u.simple = esn;
	req->recordComposition = compo;
    }
    req->resultSetId = odr_strdup(c->odr_out, resultset->setname);
    return send_APDU (c, apdu);
}

ZOOM_API(ZOOM_scanset)
ZOOM_connection_scan (ZOOM_connection c, const char *start)
{
    ZOOM_scanset scan = (ZOOM_scanset) xmalloc (sizeof(*scan));

    scan->connection = c;
    scan->odr = odr_createmem (ODR_DECODE);
    scan->options = ZOOM_options_create_with_parent (c->options);
    scan->refcount = 1;
    scan->scan_response = 0;

    if ((scan->termListAndStartPoint =
         p_query_scan(scan->odr, PROTO_Z3950, &scan->attributeSet,
                      start)))
    {
        ZOOM_task task = ZOOM_connection_add_task (c, ZOOM_TASK_SCAN);
        task->u.scan.scan = scan;
        
        (scan->refcount)++;
        if (!c->async)
        {
            while (ZOOM_event (1, &c))
                ;
        }
    }
    return scan;
}

ZOOM_API(void)
ZOOM_scanset_destroy (ZOOM_scanset scan)
{
    if (!scan)
        return;
    (scan->refcount)--;
    if (scan->refcount == 0)
    {
        odr_destroy (scan->odr);
        
        ZOOM_options_destroy (scan->options);
        xfree (scan);
    }
}

static zoom_ret send_package (ZOOM_connection c)
{
    ZOOM_Event event;
    if (!c->tasks)
        return zoom_complete;
    assert (c->tasks->which == ZOOM_TASK_PACKAGE);
    
    event = ZOOM_Event_create (ZOOM_EVENT_SEND_APDU);
    ZOOM_connection_put_event (c, event);
    
    return do_write_ex (c, c->tasks->u.package->buf_out,
                        c->tasks->u.package->len_out);
}

static zoom_ret send_scan (ZOOM_connection c)
{
    ZOOM_scanset scan;
    Z_APDU *apdu = zget_APDU(c->odr_out, Z_APDU_scanRequest);
    Z_ScanRequest *req = apdu->u.scanRequest;
    if (!c->tasks)
        return zoom_complete;
    assert (c->tasks->which == ZOOM_TASK_SCAN);
    scan = c->tasks->u.scan.scan;

    req->termListAndStartPoint = scan->termListAndStartPoint;
    req->attributeSet = scan->attributeSet;

    *req->numberOfTermsRequested =
        ZOOM_options_get_int(scan->options, "number", 10);

    req->preferredPositionInResponse =
        odr_intdup (c->odr_out,
                    ZOOM_options_get_int(scan->options, "position", 1));

    req->stepSize =
        odr_intdup (c->odr_out,
                    ZOOM_options_get_int(scan->options, "stepSize", 0));
    
    req->databaseNames = set_DatabaseNames (c, scan->options, 
                                            &req->num_databaseNames);

    return send_APDU (c, apdu);
}

ZOOM_API(size_t)
ZOOM_scanset_size (ZOOM_scanset scan)
{
    if (!scan || !scan->scan_response || !scan->scan_response->entries)
        return 0;
    return scan->scan_response->entries->num_entries;
}

ZOOM_API(const char *)
ZOOM_scanset_term (ZOOM_scanset scan, size_t pos,
                               int *occ, int *len)
{
    const char *term = 0;
    size_t noent = ZOOM_scanset_size (scan);
    Z_ScanResponse *res = scan->scan_response;
    
    *len = 0;
    *occ = 0;
    if (pos >= noent)
        return 0;
    if (res->entries->entries[pos]->which == Z_Entry_termInfo)
    {
        Z_TermInfo *t = res->entries->entries[pos]->u.termInfo;
        
        if (t->term->which == Z_Term_general)
        {
            term = (const char *) t->term->u.general->buf;
            *len = t->term->u.general->len;
        }
        *occ = t->globalOccurrences ? *t->globalOccurrences : 0;
    }
    return term;
}

ZOOM_API(const char *)
ZOOM_scanset_option_get (ZOOM_scanset scan, const char *key)
{
    return ZOOM_options_get (scan->options, key);
}

ZOOM_API(void)
ZOOM_scanset_option_set (ZOOM_scanset scan, const char *key,
                              const char *val)
{
    ZOOM_options_set (scan->options, key, val);
}

static Z_APDU *create_es_package (ZOOM_package p, int type)
{
    const char *str;
    Z_APDU *apdu = zget_APDU(p->odr_out, Z_APDU_extendedServicesRequest);
    Z_ExtendedServicesRequest *req = apdu->u.extendedServicesRequest;
    
    *req->function = Z_ExtendedServicesRequest_create;
    
    str = ZOOM_options_get(p->options, "package-name");
    if (str && *str)
        req->packageName = nmem_strdup (p->odr_out->mem, str);
    
    str = ZOOM_options_get(p->options, "user-id");
    if (str)
        req->userId = nmem_strdup (p->odr_out->mem, str);
    
    req->packageType = yaz_oidval_to_z3950oid(p->odr_out, CLASS_EXTSERV,
                                              type);

    str = ZOOM_options_get(p->options, "function");
    if (str)
    {
        if (!strcmp (str, "create"))
            *req->function = 1;
        if (!strcmp (str, "delete"))
            *req->function = 2;
        if (!strcmp (str, "modify"))
            *req->function = 3;
    }
    return apdu;
}

static const char *ill_array_lookup (void *clientData, const char *idx)
{
    ZOOM_package p = (ZOOM_package) clientData;
    return ZOOM_options_get (p->options, idx+4);
}

static Z_External *encode_ill_request (ZOOM_package p)
{
    ODR out = p->odr_out;
    ILL_Request *req;
    Z_External *r = 0;
    struct ill_get_ctl ctl;
	
    ctl.odr = p->odr_out;
    ctl.clientData = p;
    ctl.f = ill_array_lookup;
	
    req = ill_get_ILLRequest(&ctl, "ill", 0);
	
    if (!ill_Request (out, &req, 0, 0))
    {
        int ill_request_size;
        char *ill_request_buf = odr_getbuf (out, &ill_request_size, 0);
        if (ill_request_buf)
            odr_setbuf (out, ill_request_buf, ill_request_size, 1);
        return 0;
    }
    else
    {
        oident oid;
        int illRequest_size = 0;
        char *illRequest_buf = odr_getbuf (out, &illRequest_size, 0);
		
        oid.proto = PROTO_GENERAL;
        oid.oclass = CLASS_GENERAL;
        oid.value = VAL_ISO_ILL_1;
		
        r = (Z_External *) odr_malloc (out, sizeof(*r));
        r->direct_reference = odr_oiddup(out,oid_getoidbyent(&oid)); 
        r->indirect_reference = 0;
        r->descriptor = 0;
        r->which = Z_External_single;
		
        r->u.single_ASN1_type = (Odr_oct *)
            odr_malloc (out, sizeof(*r->u.single_ASN1_type));
        r->u.single_ASN1_type->buf = (unsigned char*)
            odr_malloc (out, illRequest_size);
        r->u.single_ASN1_type->len = illRequest_size;
        r->u.single_ASN1_type->size = illRequest_size;
        memcpy (r->u.single_ASN1_type->buf, illRequest_buf, illRequest_size);
    }
    return r;
}

static Z_ItemOrder *encode_item_order(ZOOM_package p)
{
    Z_ItemOrder *req = (Z_ItemOrder *) odr_malloc (p->odr_out, sizeof(*req));
    const char *str;
    
    req->which=Z_IOItemOrder_esRequest;
    req->u.esRequest = (Z_IORequest *) 
        odr_malloc(p->odr_out,sizeof(Z_IORequest));

    /* to keep part ... */
    req->u.esRequest->toKeep = (Z_IOOriginPartToKeep *)
	odr_malloc(p->odr_out,sizeof(Z_IOOriginPartToKeep));
    req->u.esRequest->toKeep->supplDescription = 0;
    req->u.esRequest->toKeep->contact = (Z_IOContact *)
        odr_malloc (p->odr_out, sizeof(*req->u.esRequest->toKeep->contact));
	
    str = ZOOM_options_get(p->options, "contact-name");
    req->u.esRequest->toKeep->contact->name = str ?
        nmem_strdup (p->odr_out->mem, str) : 0;
	
    str = ZOOM_options_get(p->options, "contact-phone");
    req->u.esRequest->toKeep->contact->phone = str ?
        nmem_strdup (p->odr_out->mem, str) : 0;
	
    str = ZOOM_options_get(p->options, "contact-email");
    req->u.esRequest->toKeep->contact->email = str ?
        nmem_strdup (p->odr_out->mem, str) : 0;
	
    req->u.esRequest->toKeep->addlBilling = 0;
	
    /* not to keep part ... */
    req->u.esRequest->notToKeep = (Z_IOOriginPartNotToKeep *)
        odr_malloc(p->odr_out,sizeof(Z_IOOriginPartNotToKeep));
	
    req->u.esRequest->notToKeep->resultSetItem = (Z_IOResultSetItem *)
        odr_malloc(p->odr_out, sizeof(Z_IOResultSetItem));

    str = ZOOM_options_get(p->options, "itemorder-setname");
    if (!str)
        str = "default";
    req->u.esRequest->notToKeep->resultSetItem->resultSetId =
        nmem_strdup (p->odr_out->mem, str);
    req->u.esRequest->notToKeep->resultSetItem->item =
        (int *) odr_malloc(p->odr_out, sizeof(int));
	
    str = ZOOM_options_get(p->options, "itemorder-item");
    *req->u.esRequest->notToKeep->resultSetItem->item =
        (str ? atoi(str) : 1);
    
    req->u.esRequest->notToKeep->itemRequest = encode_ill_request(p);
    
    return req;
}

ZOOM_API(void)
    ZOOM_package_send (ZOOM_package p, const char *type)
{
    Z_APDU *apdu = 0;
    ZOOM_connection c;
    if (!p)
        return;
    c = p->connection;
    odr_reset (p->odr_out);
    xfree (p->buf_out);
    p->buf_out = 0;
    if (!strcmp(type, "itemorder"))
    {
        Z_External *r;
        apdu = create_es_package (p, VAL_ITEMORDER);
        if (apdu)
        {
            r = (Z_External *) odr_malloc (p->odr_out, sizeof(*r));
            
            r->direct_reference =
                yaz_oidval_to_z3950oid(p->odr_out, CLASS_EXTSERV,
                                       VAL_ITEMORDER);
            r->descriptor = 0;
            r->which = Z_External_itemOrder;
            r->indirect_reference = 0;
            r->u.itemOrder = encode_item_order (p);

            apdu->u.extendedServicesRequest->taskSpecificParameters = r;
        }
    }
    if (apdu)
    {
        if (encode_APDU(p->connection, apdu, p->odr_out) == 0)
        {
            char *buf;

            ZOOM_task task = ZOOM_connection_add_task (c, ZOOM_TASK_PACKAGE);
            task->u.package = p;
            buf = odr_getbuf(p->odr_out, &p->len_out, 0);
            p->buf_out = (char *) xmalloc (p->len_out);
            memcpy (p->buf_out, buf, p->len_out);
            
            (p->refcount)++;
            if (!c->async)
            {
                while (ZOOM_event (1, &c))
                    ;
            }
        }
    }
}

ZOOM_API(ZOOM_package)
    ZOOM_connection_package (ZOOM_connection c, ZOOM_options options)
{
    ZOOM_package p = (ZOOM_package) xmalloc (sizeof(*p));

    p->connection = c;
    p->odr_out = odr_createmem (ODR_ENCODE);
    p->options = ZOOM_options_create_with_parent2 (options, c->options);
    p->refcount = 1;
    p->buf_out = 0;
    p->len_out = 0;
    return p;
}

ZOOM_API(void)
    ZOOM_package_destroy(ZOOM_package p)
{
    if (!p)
        return;
    (p->refcount)--;
    if (p->refcount == 0)
    {
        odr_destroy (p->odr_out);
        xfree (p->buf_out);
        
        ZOOM_options_destroy (p->options);
        xfree (p);
    }
}

ZOOM_API(const char *)
ZOOM_package_option_get (ZOOM_package p, const char *key)
{
    return ZOOM_options_get (p->options, key);
}

#if HAVE_GSOAP
static zoom_ret ZOOM_srw_search(ZOOM_connection c, ZOOM_resultset r,
                                const char *cql)
{
    int ret;
    struct xcql__operandType *xQuery = 0;
    char *action = 0;
    xsd__integer startRecord = r->start + 1;
    xsd__integer maximumRecord = r->count;
    const char *schema = ZOOM_resultset_option_get (r, "schema");
    struct zs__searchRetrieveResponse res;
    xsd__string recordPacking = 0;
    

    if (!schema)
        schema = "http://www.loc.gov/marcxml/";

    ret = soap_call_zs__searchRetrieveRequest(c->soap, c->host_port,
                                              action,
                                              &r->z_query->u.type_104->u.cql,
                                              xQuery, 
                                              0, 0,
                                              &startRecord, &maximumRecord,
                                              (char **) &schema,
                                              &recordPacking,
                                              &res);
    if (ret != SOAP_OK)
    {
        const char **s = soap_faultdetail(c->soap);
        xfree (c->addinfo);
        c->addinfo = 0;
        if (s && *s)
            c->addinfo = xstrdup(*s);
        c->diagset = "SOAP";
        c->error = ret;
    }
    else
    {
        if (res.diagnostics.__sizeDiagnostics > 0)
        {
            int i = 0;
            xfree (c->addinfo);
            c->addinfo = 0;
            c->diagset = "SRW";
            c->error = res.diagnostics.diagnostic[i]->code;
            if (res.diagnostics.diagnostic[i]->details)
                c->addinfo =
                    xstrdup(res.diagnostics.diagnostic[i]->details);
            
        }
        else
        {
            int i;
            r->size = res.numberOfRecords;
            if (res.resultSetId)
                r->setname = xstrdup(res.resultSetId);
            for (i = 0; i < res.records.__sizeRecords; i++)
            {
                char *rdata = res.records.record[i]->recordData;
                if (rdata)
                {
                    Z_NamePlusRecord *npr =
                        odr_malloc(r->odr, sizeof(*npr));
                    Z_External *ext =
                        z_ext_record(r->odr, VAL_TEXT_XML,
                                     rdata, strlen(rdata));
                    npr->databaseName = 0;
                    npr->which = Z_NamePlusRecord_databaseRecord;
                    npr->u.databaseRecord = ext;
                    record_cache_add (r, npr, r->start + i);
                }
            }
        }
    }
    return zoom_complete;
}
#endif

ZOOM_API(void)
ZOOM_package_option_set (ZOOM_package p, const char *key,
                              const char *val)
{
    ZOOM_options_set (p->options, key, val);
}

static int ZOOM_connection_exec_task (ZOOM_connection c)
{
    ZOOM_task task = c->tasks;
    zoom_ret ret = zoom_complete;

    if (!task)
    {
        yaz_log (LOG_DEBUG, "ZOOM_connection_exec_task task=<null>");
	return 0;
    }
    yaz_log (LOG_DEBUG, "ZOOM_connection_exec_task type=%d run=%d",
             task->which, task->running);
    if (c->error != ZOOM_ERROR_NONE)
    {
        yaz_log (LOG_DEBUG, "remove tasks because of error = %d", c->error);
        ZOOM_connection_remove_tasks (c);
        return 0;
    }
    if (task->running)
    {
        yaz_log (LOG_DEBUG, "task already running");
	return 0;
    }
    task->running = 1;
    ret = zoom_complete;
    if (c->soap)
    {
#if HAVE_GSOAP
        ZOOM_resultset resultset;
        switch (task->which)
        {
        case ZOOM_TASK_SEARCH:
            resultset = c->tasks->u.search.resultset;
            if (resultset->z_query && 
                resultset->z_query->which == Z_Query_type_104
                && resultset->z_query->u.type_104->which == Z_External_CQL)
                ret = ZOOM_srw_search(c, resultset, 
                                      resultset->z_query->u.type_104->u.cql);
            break;
        case ZOOM_TASK_RETRIEVE:
            resultset = c->tasks->u.retrieve.resultset;
            resultset->start = c->tasks->u.retrieve.start;
            resultset->count = c->tasks->u.retrieve.count;

            if (resultset->start >= resultset->size)
                return zoom_complete;
            if (resultset->start + resultset->count > resultset->size)
                resultset->count = resultset->size - resultset->start;

            if (resultset->z_query && 
                resultset->z_query->which == Z_Query_type_104
                && resultset->z_query->u.type_104->which == Z_External_CQL)
                ret = ZOOM_srw_search(c, resultset, 
                                      resultset->z_query->u.type_104->u.cql);
            break;
        }
#else
        ;
#endif
    }
    else if (c->cs || task->which == ZOOM_TASK_CONNECT)
    {
        switch (task->which)
        {
        case ZOOM_TASK_SEARCH:
            ret = ZOOM_connection_send_search(c);
            break;
        case ZOOM_TASK_RETRIEVE:
            ret = send_present (c);
            break;
        case ZOOM_TASK_CONNECT:
            ret = do_connect(c);
            break;
        case ZOOM_TASK_SCAN:
            ret = send_scan(c);
            break;
        case ZOOM_TASK_PACKAGE:
            ret = send_package(c);
            break;
        }
    }
    else
    {
        yaz_log (LOG_DEBUG, "remove tasks because no connection exist");
        ZOOM_connection_remove_tasks (c);
    }
    if (ret == zoom_complete)
    {
        yaz_log (LOG_DEBUG, "task removed (complete)");
        ZOOM_connection_remove_task (c);
        return 0;
    }
    yaz_log (LOG_DEBUG, "task pending");
    return 1;
}

static zoom_ret send_sort_present (ZOOM_connection c)
{
    zoom_ret r = send_sort (c);
    if (r == zoom_complete)
	r = send_present (c);
    return r;
}

static int es_response (ZOOM_connection c,
                        Z_ExtendedServicesResponse *res)
{
    if (!c->tasks || c->tasks->which != ZOOM_TASK_PACKAGE)
        return 0;
    if (res->diagnostics && res->num_diagnostics > 0)
        response_diag(c, res->diagnostics[0]);
    if (res->taskPackage &&
        res->taskPackage->which == Z_External_extendedService)
    {
        Z_TaskPackage *taskPackage = res->taskPackage->u.extendedService;
        Odr_oct *id = taskPackage->targetReference;
        
        if (id)
            ZOOM_options_setl (c->tasks->u.package->options,
                               "targetReference", (char*) id->buf, id->len);
    }
    return 1;
}


static void handle_apdu (ZOOM_connection c, Z_APDU *apdu)
{
    Z_InitResponse *initrs;
    
    c->mask = 0;
    yaz_log (LOG_DEBUG, "handle_apdu type=%d", apdu->which);
    switch(apdu->which)
    {
    case Z_APDU_initResponse:
	initrs = apdu->u.initResponse;
        ZOOM_connection_option_set(c, "targetImplementationId",
                                   initrs->implementationId ?
                                   initrs->implementationId : "");
        ZOOM_connection_option_set(c, "targetImplementationName",
                                   initrs->implementationName ?
                                   initrs->implementationName : "");
        ZOOM_connection_option_set(c, "targetImplementationVersion",
                                   initrs->implementationVersion ?
                                   initrs->implementationVersion : "");
	if (!*initrs->result)
	{
            set_ZOOM_error(c, ZOOM_ERROR_INIT, 0);
	}
	else
	{
	    char *cookie =
		yaz_oi_get_string_oidval (&apdu->u.initResponse->otherInfo,
					  VAL_COOKIE, 1, 0);
	    xfree (c->cookie_in);
	    c->cookie_in = 0;
	    if (cookie)
		c->cookie_in = xstrdup(cookie);
            if (ODR_MASK_GET(initrs->options, Z_Options_namedResultSets) &&
                ODR_MASK_GET(initrs->protocolVersion, Z_ProtocolVersion_3))
                c->support_named_resultsets = 1;
            if (c->tasks)
            {
                assert (c->tasks->which == ZOOM_TASK_CONNECT);
                ZOOM_connection_remove_task (c);
            }
	    ZOOM_connection_exec_task (c);
	}
	if (ODR_MASK_GET(initrs->options, Z_Options_negotiationModel))
	{
            NMEM tmpmem = nmem_create();
            Z_CharSetandLanguageNegotiation *p =
                yaz_get_charneg_record(initrs->otherInfo);
            
            if (p)
            {
                char *charset=NULL, *lang=NULL;
                int sel;
                
                yaz_get_response_charneg(tmpmem, p, &charset, &lang, &sel);
                yaz_log(LOG_DEBUG, "Target accepted: charset %s, "
                        "language %s, select %d",
                        charset ? charset : "none", lang ? lang : "none", sel);
                if (charset)
                    ZOOM_connection_option_set (c, "negotiation-charset",
                                                charset);
                if (lang)
                    ZOOM_connection_option_set (c, "negotiation-lang",
                                                lang);
                nmem_destroy(tmpmem);
            }
	}	
	break;
    case Z_APDU_searchResponse:
	handle_search_response (c, apdu->u.searchResponse);
	if (send_sort_present (c) == zoom_complete)
	    ZOOM_connection_remove_task (c);
	break;
    case Z_APDU_presentResponse:
	handle_present_response (c, apdu->u.presentResponse);
	if (send_present (c) == zoom_complete)
	    ZOOM_connection_remove_task (c);
	break;
    case Z_APDU_sortResponse:
	sort_response (c, apdu->u.sortResponse);
	if (send_present (c) == zoom_complete)
	    ZOOM_connection_remove_task (c);
        break;
    case Z_APDU_scanResponse:
        scan_response (c, apdu->u.scanResponse);
        ZOOM_connection_remove_task (c);
        break;
    case Z_APDU_extendedServicesResponse:
        es_response (c, apdu->u.extendedServicesResponse);
        ZOOM_connection_remove_task (c);
        break;
    case Z_APDU_close:
        if (c->reconnect_ok)
        {
            do_close(c);
            c->tasks->running = 0;
            ZOOM_connection_insert_task (c, ZOOM_TASK_CONNECT);
        }
        else
        {
            set_ZOOM_error(c, ZOOM_ERROR_CONNECTION_LOST, 0);
            do_close(c);
        }
        break;
    default:
        set_ZOOM_error(c, ZOOM_ERROR_DECODE, 0);
        do_close(c);
    }
}

static int do_read (ZOOM_connection c)
{
    int r;
    Z_APDU *apdu;
    ZOOM_Event event;
    
    event = ZOOM_Event_create (ZOOM_EVENT_RECV_DATA);
    ZOOM_connection_put_event (c, event);
    
    yaz_log (LOG_DEBUG, "do_read len=%d", c->len_in);

    r = cs_get (c->cs, &c->buf_in, &c->len_in);
    if (r == 1)
	return 0;
    if (r <= 0)
    {
        if (c->reconnect_ok)
        {
            do_close (c);
            c->reconnect_ok = 0;
            yaz_log (LOG_DEBUG, "reconnect read");
            c->tasks->running = 0;
            ZOOM_connection_insert_task (c, ZOOM_TASK_CONNECT);
        }
        else
        {
            set_ZOOM_error(c, ZOOM_ERROR_CONNECTION_LOST, 0);
            do_close (c);
        }
    }
    else
    {
        ZOOM_Event event;
	odr_reset (c->odr_in);
	odr_setbuf (c->odr_in, c->buf_in, r, 0);
        event = ZOOM_Event_create (ZOOM_EVENT_RECV_APDU);
        ZOOM_connection_put_event (c, event);
	if (!z_APDU (c->odr_in, &apdu, 0, 0))
	{
            set_ZOOM_error(c, ZOOM_ERROR_DECODE, 0);
	    do_close (c);
	}
	else
	    handle_apdu (c, apdu);
        c->reconnect_ok = 0;
    }
    return 1;
}

static zoom_ret do_write_ex (ZOOM_connection c, char *buf_out, int len_out)
{
    int r;
    ZOOM_Event event;
    
    event = ZOOM_Event_create(ZOOM_EVENT_SEND_DATA);
    ZOOM_connection_put_event (c, event);

    yaz_log (LOG_DEBUG, "do_write_ex len=%d", len_out);
    if ((r=cs_put (c->cs, buf_out, len_out)) < 0)
    {
        if (c->reconnect_ok)
        {
            do_close (c);
            c->reconnect_ok = 0;
            yaz_log (LOG_DEBUG, "reconnect write");
            c->tasks->running = 0;
            ZOOM_connection_insert_task (c, ZOOM_TASK_CONNECT);
            return zoom_complete;
        }
	if (c->state == STATE_CONNECTING)
	    set_ZOOM_error(c, ZOOM_ERROR_CONNECT, 0);
	else
            set_ZOOM_error(c, ZOOM_ERROR_CONNECTION_LOST, 0);
	do_close (c);
	return zoom_complete;
    }
    else if (r == 1)
    {    
        c->mask = ZOOM_SELECT_EXCEPT;
        if (c->cs->io_pending & CS_WANT_WRITE)
            c->mask += ZOOM_SELECT_WRITE;
        if (c->cs->io_pending & CS_WANT_READ)
            c->mask += ZOOM_SELECT_READ;
        yaz_log (LOG_DEBUG, "do_write_ex 1 mask=%d", c->mask);
    }
    else
    {
        c->mask = ZOOM_SELECT_READ|ZOOM_SELECT_EXCEPT;
        yaz_log (LOG_DEBUG, "do_write_ex 2 mask=%d", c->mask);
    }
    return zoom_pending;
}

static zoom_ret do_write(ZOOM_connection c)
{
    return do_write_ex (c, c->buf_out, c->len_out);
}


ZOOM_API(const char *)
ZOOM_connection_option_get (ZOOM_connection c, const char *key)
{
    return ZOOM_options_get (c->options, key);
}

ZOOM_API(void)
ZOOM_connection_option_set (ZOOM_connection c, const char *key,
                                  const char *val)
{
    ZOOM_options_set (c->options, key, val);
}

ZOOM_API(const char *)
ZOOM_resultset_option_get (ZOOM_resultset r, const char *key)
{
    return ZOOM_options_get (r->options, key);
}

ZOOM_API(void)
ZOOM_resultset_option_set (ZOOM_resultset r, const char *key,
                                  const char *val)
{
    ZOOM_options_set (r->options, key, val);
}


ZOOM_API(int)
ZOOM_connection_errcode (ZOOM_connection c)
{
    return ZOOM_connection_error (c, 0, 0);
}

ZOOM_API(const char *)
ZOOM_connection_errmsg (ZOOM_connection c)
{
    const char *msg;
    ZOOM_connection_error (c, &msg, 0);
    return msg;
}

ZOOM_API(const char *)
ZOOM_connection_addinfo (ZOOM_connection c)
{
    const char *addinfo;
    ZOOM_connection_error (c, 0, &addinfo);
    return addinfo;
}

ZOOM_API(const char *)
ZOOM_diag_str (int error)
{
    switch (error)
    {
    case ZOOM_ERROR_NONE:
	return "No error";
    case ZOOM_ERROR_CONNECT:
	return "Connect failed";
    case ZOOM_ERROR_MEMORY:
	return "Out of memory";
    case ZOOM_ERROR_ENCODE:
	return "Encoding failed";
    case ZOOM_ERROR_DECODE:
	return "Decoding failed";
    case ZOOM_ERROR_CONNECTION_LOST:
	return "Connection lost";
    case ZOOM_ERROR_INIT:
	return "Init rejected";
    case ZOOM_ERROR_INTERNAL:
	return "Internal failure";
    case ZOOM_ERROR_TIMEOUT:
	return "Timeout";
    case ZOOM_ERROR_UNSUPPORTED_PROTOCOL:
	return "Unsupported protocol";
    default:
	return diagbib1_str (error);
    }
}

ZOOM_API(int)
ZOOM_connection_error_x (ZOOM_connection c, const char **cp,
                         const char **addinfo, const char **diagset)
{
    int error = c->error;
    if (cp)
    {
        if (!c->diagset)
            *cp = ZOOM_diag_str(error);
#if HAVE_GSOAP
        else if (!strcmp(c->diagset, "SRW"))
            *cp = yaz_srw_diag_str(error);
        else if (c->soap && !strcmp(c->diagset, "SOAP"))
        {
            const char **s = soap_faultstring(c->soap);
            if (s && *s)
                *cp = *s;
            else
                *cp = "unknown";
        }
#endif
        else
            *cp = ZOOM_diag_str(error);
    }
    if (addinfo)
        *addinfo = c->addinfo ? c->addinfo : "";
    if (diagset)
        *diagset = c->diagset ? c->diagset : "";
    return c->error;
}

ZOOM_API(int)
ZOOM_connection_error (ZOOM_connection c, const char **cp,
                       const char **addinfo)
{
    return ZOOM_connection_error_x(c, cp, addinfo, 0);
}

static int ZOOM_connection_do_io(ZOOM_connection c, int mask)
{
    ZOOM_Event event = 0;
    int r = cs_look(c->cs);
    yaz_log (LOG_DEBUG, "ZOOM_connection_do_io c=%p mask=%d cs_look=%d",
	     c, mask, r);
    
    if (r == CS_NONE)
    {
        event = ZOOM_Event_create (ZOOM_EVENT_CONNECT);
	set_ZOOM_error(c, ZOOM_ERROR_CONNECT, 0);
	do_close (c);
        ZOOM_connection_put_event (c, event);
    }
    else if (r == CS_CONNECT)
    {
        int ret;
        event = ZOOM_Event_create (ZOOM_EVENT_CONNECT);

        ret = cs_rcvconnect (c->cs);
        yaz_log (LOG_DEBUG, "cs_rcvconnect returned %d", ret);
        if (ret == 1)
        {
            c->mask = ZOOM_SELECT_EXCEPT;
            if (c->cs->io_pending & CS_WANT_WRITE)
                c->mask += ZOOM_SELECT_WRITE;
            if (c->cs->io_pending & CS_WANT_READ)
                c->mask += ZOOM_SELECT_READ;
            ZOOM_connection_put_event (c, event);
        }
        else if (ret == 0)
        {
            ZOOM_connection_put_event (c, event);
            ZOOM_connection_send_init (c);
            c->state = STATE_ESTABLISHED;
        }
        else
        {
            set_ZOOM_error(c, ZOOM_ERROR_CONNECT, 0);
            do_close (c);
            ZOOM_connection_put_event (c, event);
        }
    }
    else
    {
        if (mask & ZOOM_SELECT_READ)
            do_read (c);
        if (c->cs && (mask & ZOOM_SELECT_WRITE))
            do_write (c);
    }
    return 1;
}

ZOOM_API(int)
ZOOM_connection_last_event(ZOOM_connection cs)
{
    if (!cs)
        return ZOOM_EVENT_NONE;
    return cs->last_event;
}

ZOOM_API(int)
ZOOM_event (int no, ZOOM_connection *cs)
{
    int timeout = 5000;
#if HAVE_SYS_POLL_H
    struct pollfd pollfds[1024];
    ZOOM_connection poll_cs[1024];
#else
    struct timeval tv;
    fd_set input, output, except;
#endif
    int i, r, nfds;
    int max_fd = 0;

    for (i = 0; i<no; i++)
    {
	ZOOM_connection c = cs[i];
        ZOOM_Event event;
	if (c && (event = ZOOM_connection_get_event(c)))
        {
            ZOOM_Event_destroy (event);
	    return i+1;
        }
    }
    for (i = 0; i<no; i++)
    {
        ZOOM_connection c = cs[i];
        ZOOM_Event event;
        if (c && ZOOM_connection_exec_task (c))
        {
            if ((event = ZOOM_connection_get_event(c)))
            {
                ZOOM_Event_destroy (event);
                return i+1;
            }
        }
    }
#if HAVE_SYS_POLL_H

#else
    FD_ZERO (&input);
    FD_ZERO (&output);
    FD_ZERO (&except);
#endif
    nfds = 0;
    for (i = 0; i<no; i++)
    {
	ZOOM_connection c = cs[i];
	int fd, mask;
        int this_timeout;
	
	if (!c)
	    continue;
	fd = z3950_connection_socket(c);
	mask = z3950_connection_mask(c);

	if (fd == -1)
	    continue;
	if (max_fd < fd)
	    max_fd = fd;

        this_timeout = ZOOM_options_get_int (c->options, "timeout", -1);
        if (this_timeout != -1 && this_timeout < timeout)
            timeout = this_timeout;
#if HAVE_SYS_POLL_H
        if (mask)
        {
            short poll_events = 0;

            if (mask & ZOOM_SELECT_READ)
                poll_events += POLLIN;
            if (mask & ZOOM_SELECT_WRITE)
                poll_events += POLLOUT;
            if (mask & ZOOM_SELECT_EXCEPT)
                poll_events += POLLERR;
            pollfds[nfds].fd = fd;
            pollfds[nfds].events = poll_events;
            pollfds[nfds].revents = 0;
            poll_cs[nfds] = c;
            nfds++;
        }
#else
	if (mask & ZOOM_SELECT_READ)
	{
	    FD_SET (fd, &input);
	    nfds++;
	}
	if (mask & ZOOM_SELECT_WRITE)
	{
	    FD_SET (fd, &output);
	    nfds++;
	}
	if (mask & ZOOM_SELECT_EXCEPT)
	{
	    FD_SET (fd, &except);
	    nfds++;
	}
#endif
    }
    if (timeout >= 5000)
        timeout = 30;

    if (!nfds)
        return 0;

#if HAVE_SYS_POLL_H
    r = poll (pollfds, nfds, timeout * 1000);
    for (i = 0; i<nfds; i++)
    {
        ZOOM_connection c = poll_cs[i];
        if (r && c->mask)
        {
            int mask = 0;
            if (pollfds[i].revents & POLLIN)
                mask += ZOOM_SELECT_READ;
            if (pollfds[i].revents & POLLOUT)
                mask += ZOOM_SELECT_WRITE;
            if (pollfds[i].revents & POLLERR)
                mask += ZOOM_SELECT_EXCEPT;
            if (mask)
                ZOOM_connection_do_io(c, mask);
        }
        else if (r == 0 && c->mask)
        {
            ZOOM_Event event = ZOOM_Event_create(ZOOM_EVENT_TIMEOUT);
	    /* timeout and this connection was waiting */
	    set_ZOOM_error(c, ZOOM_ERROR_TIMEOUT, 0);
            do_close (c);
            ZOOM_connection_put_event(c, event);
        }
    }
#else
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    yaz_log (LOG_DEBUG, "select start");
    r = select (max_fd+1, &input, &output, &except, &tv);
    yaz_log (LOG_DEBUG, "select stop, returned r=%d", r);
    for (i = 0; i<no; i++)
    {
	ZOOM_connection c = cs[i];
	int fd, mask;

	if (!c)
	    continue;
	fd = z3950_connection_socket(c);
	mask = 0;
	if (r && c->mask)
	{
	    /* no timeout and real socket */
	    if (FD_ISSET(fd, &input))
		mask += ZOOM_SELECT_READ;
	    if (FD_ISSET(fd, &output))
		mask += ZOOM_SELECT_WRITE;
	    if (FD_ISSET(fd, &except))
		mask += ZOOM_SELECT_EXCEPT;
	    if (mask)
		ZOOM_connection_do_io(c, mask);
	}
	if (r == 0 && c->mask)
	{
            ZOOM_Event event = ZOOM_Event_create(ZOOM_EVENT_TIMEOUT);
	    /* timeout and this connection was waiting */
	    set_ZOOM_error(c, ZOOM_ERROR_TIMEOUT, 0);
            do_close (c);
            yaz_log (LOG_DEBUG, "timeout");
            ZOOM_connection_put_event(c, event);
	}
    }
#endif
    for (i = 0; i<no; i++)
    {
	ZOOM_connection c = cs[i];
        ZOOM_Event event;
	if (c && (event = ZOOM_connection_get_event(c)))
        {
            ZOOM_Event_destroy (event);
	    return i+1;
        }
    }
    return 0;
}
