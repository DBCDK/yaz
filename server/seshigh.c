/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: seshigh.c,v $
 * Revision 1.8  1995-03-22 10:13:21  quinn
 * Working on record packer
 *
 * Revision 1.7  1995/03/21  15:53:31  quinn
 * Little changes.
 *
 * Revision 1.6  1995/03/21  12:30:09  quinn
 * Beginning to add support for record packing.
 *
 * Revision 1.5  1995/03/17  10:44:13  quinn
 * Added catch of null-string in makediagrec
 *
 * Revision 1.4  1995/03/17  10:18:08  quinn
 * Added memory management.
 *
 * Revision 1.3  1995/03/16  17:42:39  quinn
 * Little changes
 *
 * Revision 1.2  1995/03/16  13:29:01  quinn
 * Partitioned server.
 *
 * Revision 1.1  1995/03/15  16:02:10  quinn
 * Modded session.c seshigh.c
 *
 * Revision 1.10  1995/03/15  15:18:51  quinn
 * Little changes to better support nonblocking I/O
 * Added backend.h
 *
 * Revision 1.9  1995/03/15  13:20:23  adam
 * Yet another bug fix in very dummy_database...
 *
 * Revision 1.8  1995/03/15  11:18:17  quinn
 * Smallish changes.
 *
 * Revision 1.7  1995/03/15  09:40:15  adam
 * Bug fixes in dummy_database_...
 *
 * Revision 1.6  1995/03/15  09:08:30  adam
 * Take care of preferredMessageSize.
 *
 * Revision 1.5  1995/03/15  08:37:44  quinn
 * Now we're pretty much set for nonblocking I/O.
 *
 * Revision 1.4  1995/03/15  08:27:20  adam
 * PresentRequest changed to return MARC records from file 'dummy-records'.
 *
 * Revision 1.3  1995/03/14  16:59:48  quinn
 * Bug-fixes
 *
 * Revision 1.2  1995/03/14  11:30:14  quinn
 * Works better now.
 *
 * Revision 1.1  1995/03/14  10:28:01  quinn
 * More work on demo server.
 *
 *
 */

#include <stdlib.h>
#include <assert.h>

#include <comstack.h>
#include <eventl.h>
#include <session.h>
#include <proto.h>

#include <backend.h>

#include <iso2709.h>

#define ENCODE_BUFFER_SIZE 10000

static int process_apdu(IOCHAN chan);
static int process_initRequest(IOCHAN client, Z_InitRequest *req);
static int process_searchRequest(IOCHAN client, Z_SearchRequest *req);
static int process_presentRequest(IOCHAN client, Z_PresentRequest *req);

association *create_association(IOCHAN channel, COMSTACK link)
{
    association *new;

    if (!(new = malloc(sizeof(*new))))
    	return 0;
    new->client_chan = channel;
    new->client_link = link;
    if (!(new->decode = odr_createmem(ODR_DECODE)) ||
    	!(new->encode = odr_createmem(ODR_ENCODE)))
	return 0;
    if (!(new->encode_buffer = malloc(ENCODE_BUFFER_SIZE)))
    	return 0;
    odr_setbuf(new->encode, new->encode_buffer, ENCODE_BUFFER_SIZE);
    new->state = ASSOC_UNINIT;
    new->input_buffer = 0;
    new->input_buffer_len = 0;
    return new;
}

void destroy_association(association *h)
{
    odr_destroy(h->decode);
    odr_destroy(h->encode);
    free(h->encode_buffer);
    if (h->input_buffer)
    	free(h->input_buffer);
    free(h);
}

void ir_session(IOCHAN h, int event)
{
    int res;
    association *assoc = iochan_getdata(h);
    COMSTACK conn = assoc->client_link;

    if (event == EVENT_INPUT)
    {
	assert(assoc && conn);
	res = cs_get(conn, &assoc->input_buffer, &assoc->input_buffer_len);
	switch (res)
	{
	    case 0: case -1: /* connection closed by peer */
	    	fprintf(stderr, "Closed connection\n");
		cs_close(conn);
		destroy_association(assoc);
		iochan_destroy(h);
		return;
	    case 1:  /* incomplete read */
		return;
	    default: /* data! */
		assoc->input_apdu_len = res;
		if (process_apdu(h) < 0)
		{
		    fprintf(stderr, "Operation failed\n");
		    cs_close(conn);
		    destroy_association(assoc);
		    iochan_destroy(h);
		}
		else if (cs_more(conn)) /* arrange to be called again */
		    iochan_setevent(h, EVENT_INPUT);
	}
    }
    else if (event == EVENT_OUTPUT)
    {
    	switch (res = cs_put(conn, assoc->encode_buffer, assoc->encoded_len))
	{
	    case -1:
	    	fprintf(stderr, "Closed connection\n");
		cs_close(conn);
		destroy_association(assoc);
		iochan_destroy(h);
	    case 0: /* all sent */
	    	iochan_setflags(h, EVENT_INPUT | EVENT_EXCEPT); /* reset */
		break;
	    case 1: /* partial send */
	    	break; /* we'll get called again */
	}
    }
    else if (event == EVENT_EXCEPT)
    {
	fprintf(stderr, "Exception on line\n");
	cs_close(conn);
	destroy_association(assoc);
	iochan_destroy(h);
    }
}

static int process_apdu(IOCHAN chan)
{
    Z_APDU *apdu;
    int res;
    association *assoc = iochan_getdata(chan);

    odr_setbuf(assoc->decode, assoc->input_buffer, assoc->input_apdu_len);
    if (!z_APDU(assoc->decode, &apdu, 0))
    {
    	odr_perror(assoc->decode, "Incoming APDU");
	return -1;
    }
    switch (apdu->which)
    {
    	case Z_APDU_initRequest:
	    res = process_initRequest(chan, apdu->u.initRequest); break;
	case Z_APDU_searchRequest:
	    res = process_searchRequest(chan, apdu->u.searchRequest); break;
	case Z_APDU_presentRequest:
	    res = process_presentRequest(chan, apdu->u.presentRequest); break;
	default:
	    fprintf(stderr, "Bad APDU\n");
	    return -1;
    }
    odr_reset(assoc->decode);
    return res;
}

static int process_initRequest(IOCHAN client, Z_InitRequest *req)
{
    Z_APDU apdu, *apdup;
    Z_InitResponse resp;
    bool_t result = 1;
    association *assoc = iochan_getdata(client);
    bend_initrequest binitreq;
    bend_initresult *binitres;

    fprintf(stderr, "Got initRequest.\n");
    if (req->implementationId)
    	fprintf(stderr, "Id:        %s\n", req->implementationId);
    if (req->implementationName)
    	fprintf(stderr, "Name:      %s\n", req->implementationName);
    if (req->implementationVersion)
    	fprintf(stderr, "Version:   %s\n", req->implementationVersion);

    binitreq.configname = "default-config";
    if (!(binitres = bend_init(&binitreq)) || binitres->errcode)
    {
    	fprintf(stderr, "Bad response from backend\n");
    	return -1;
    }

    apdup = &apdu;
    apdu.which = Z_APDU_initResponse;
    apdu.u.initResponse = &resp;
    resp.referenceId = req->referenceId;
    resp.options = req->options; /* should check these */
    resp.protocolVersion = req->protocolVersion;
    assoc->maximumRecordSize = *req->maximumRecordSize;
    if (assoc->maximumRecordSize > ENCODE_BUFFER_SIZE - 500)
    	assoc->maximumRecordSize = ENCODE_BUFFER_SIZE - 500;
    assoc->preferredMessageSize = *req->preferredMessageSize;
    if (assoc->preferredMessageSize > assoc->maximumRecordSize)
    	assoc->preferredMessageSize = assoc->maximumRecordSize;
    resp.preferredMessageSize = &assoc->preferredMessageSize;
    resp.maximumRecordSize = &assoc->maximumRecordSize;
    resp.result = &result;
    resp.implementationId = "YAZ";
    resp.implementationName = "YAZ/Simple asynchronous test server";
    resp.implementationVersion = "$Revision: 1.8 $";
    resp.userInformationField = 0;
    if (!z_APDU(assoc->encode, &apdup, 0))
    {
    	odr_perror(assoc->encode, "Encode initres");
	return -1;
    }
    odr_getbuf(assoc->encode, &assoc->encoded_len);
    odr_reset(assoc->encode);
    iochan_setflags(client, EVENT_OUTPUT | EVENT_EXCEPT);
    return 0;
}

static Z_Records *diagrec(int error, char *addinfo)
{
    static Z_Records rec;
    static Odr_oid bib1[] = { 1, 2, 3, 4, 5, -1 };
    static Z_DiagRec dr;
    static int err;

    fprintf(stderr, "Diagnostic: %d -- %s\n", error, addinfo ? addinfo :
	"NULL");
    err = error;
    rec.which = Z_Records_NSD;
    rec.u.nonSurrogateDiagnostic = &dr;
    dr.diagnosticSetId = bib1;
    dr.condition = &err;
    dr.addinfo = addinfo ? addinfo : "";
    return &rec;
}

static Z_NamePlusRecord *surrogatediagrec(char *dbname, int error,
					    char *addinfo)
{
    static Z_NamePlusRecord rec;
    static Z_DiagRec dr;
    static Odr_oid bib1[] = { 1, 2, 3, 4, 5, -1 };
    static int err;

    fprintf(stderr, "SurrogateDiagnotic: %d -- %s\n", error, addinfo);
    rec.databaseName = dbname;
    rec.which = Z_NamePlusRecord_surrogateDiagnostic;
    rec.u.surrogateDiagnostic = &dr;
    dr.diagnosticSetId = bib1;
    dr.condition = &err;
    dr.addinfo = addinfo ? addinfo : "";
    return &rec;
}

#define MAX_RECORDS 50

static Z_Records *pack_records(association *a, char *setname, int start,
				int *num, Z_ElementSetNames *esn,
				int *next, int *pres)
{
    int recno, total_length = 0, toget = *num;
    static Z_Records records;
    static Z_NamePlusRecordList reclist;
    static Z_NamePlusRecord *list[MAX_RECORDS];

    records.which = Z_Records_DBOSD;
    records.u.databaseOrSurDiagnostics = &reclist;
    reclist.num_records = 0;
    reclist.records = list;
    *pres = Z_PRES_SUCCESS;
    *num = 0;
    *next = 0;

    for (recno = start; recno < toget; recno++)
    {
    	bend_fetchrequest freq;
	bend_fetchresult *fres;
	Z_NamePlusRecord *thisrec;
	Z_DatabaseRecord *thisext;

	if (reclist.num_records == MAX_RECORDS - 1)
	{
	    *pres = Z_PRES_PARTIAL_2;
	    break;
	}
	freq.setname = setname;
	freq.number = recno;
	if (!(fres = bend_fetch(&freq)))
	{
	    *pres = Z_PRES_FAILURE;
	    return diagrec(2, "Backend interface problem");
	}
	/* backend should be able to signal whether error is system-wide
	   or only pertaining to current record */
	if (fres->errcode)
	{
	    *pres = Z_PRES_FAILURE;
	    return diagrec(fres->errcode, fres->errstring);
	}
	if (fres->len + total_length > a->preferredMessageSize)
	{
	    /* record is small enough, really */
	    if (fres->len <= a->preferredMessageSize)
	    {
		*pres = Z_PRES_PARTIAL_2;
		break;
	    }
	    /* record canonly be fetched by itself */
	    if (fres->len < a->maximumRecordSize)
	    {
	    	if (toget > 1)
		{
		    reclist.records[reclist.num_records] =
		   	 surrogatediagrec(fres->basename, 16, 0);
		    reclist.num_records++;
		    *pres = Z_PRES_PARTIAL_2;
		}
	    }
	    else /* too big entirely */
	    {
		reclist.records[reclist.num_records] =
		    surrogatediagrec(fres->basename,
		    17, 0);
		reclist.num_records++;
		*pres = Z_PRES_PARTIAL_2;
	    }
	}
	if (!(thisrec = odr_malloc(a->encode, sizeof(*thisrec))))
	    return 0;
	if (!(thisrec->databaseName = odr_malloc(a->encode,
	    strlen(fres->basename) + 1)))
	    return 0;
	strcpy(thisrec->databaseName, fres->basename);
	thisrec->which = Z_NamePlusRecord_databaseRecord;
	if (!(thisrec->u.databaseRecord = thisext =  odr_malloc(a->encode,
	    sizeof(Z_DatabaseRecord))))
	    return 0;
	thisext->direct_reference = 0; /* should be OID for current MARC */
	thisext->indirect_reference = 0;
	thisext->descriptor = 0;
	thisext->which = ODR_EXTERNAL_octet;
	if (!(thisext->u.octet_aligned = odr_malloc(a->encode,
	    sizeof(Odr_oct))))
	    return 0;
	if (!(thisext->u.octet_aligned->buf = odr_malloc(a->encode, fres->len)))
	    return 0;
	memcpy(thisext->u.octet_aligned->buf, fres->record, fres->len);
	thisext->u.octet_aligned->len = thisext->u.octet_aligned->size =
	    fres->len;
	reclist.records[reclist.num_records] = thisrec;
	reclist.num_records++;
	total_length = fres->len;
	(*num)++;
	*next = fres->last_in_set ? 0 : recno + 1;
    }
    return &records;
}

static int process_searchRequest(IOCHAN client, Z_SearchRequest *req)
{
    Z_APDU apdu, *apdup;
    Z_SearchResponse resp;
    association *assoc = iochan_getdata(client);
    int nulint = 0;
    bool_t sr = 1;
    int nrp;
    bend_searchrequest bsrq;
    bend_searchresult *bsrt;

    fprintf(stderr, "Got SearchRequest.\n");
    apdup = &apdu;
    apdu.which = Z_APDU_searchResponse;
    apdu.u.searchResponse = &resp;
    resp.referenceId = req->referenceId;

    bsrq.setname = req->resultSetName;
    bsrq.replace_set = *req->replaceIndicator;
    bsrq.num_bases = req->num_databaseNames;
    bsrq.basenames = req->databaseNames;
    bsrq.query = req->query;

    if (!(bsrt = bend_search(&bsrq)))
    	return -1;
    else if (bsrt->errcode)
	resp.records = diagrec(bsrt->errcode, bsrt->errstring);
    else
    	resp.records = 0;

    resp.resultCount = &bsrt->hits;
    resp.numberOfRecordsReturned = &nulint;
    nrp = bsrt->hits ? 1 : 0;
    resp.nextResultSetPosition = &nrp;
    resp.searchStatus = &sr;
    resp.resultSetStatus = &sr;
    resp.presentStatus = 0;

    if (!z_APDU(assoc->encode, &apdup, 0))
    {
    	odr_perror(assoc->encode, "Encode searchres");
	return -1;
    }
    odr_getbuf(assoc->encode, &assoc->encoded_len);
    odr_reset(assoc->encode);
    iochan_setflags(client, EVENT_OUTPUT | EVENT_EXCEPT);
    return 0;
}

static int process_presentRequest(IOCHAN client, Z_PresentRequest *req)
{
    Z_APDU apdu, *apdup;
    Z_PresentResponse resp;
    association *assoc = iochan_getdata(client);
    int presst, next, num;

    fprintf(stderr, "Got PresentRequest.\n");
    apdup = &apdu;
    apdu.which = Z_APDU_presentResponse;
    apdu.u.presentResponse = &resp;
    resp.referenceId = req->referenceId;

    num = *req->numberOfRecordsRequested;
    resp.records = pack_records(assoc, req->resultSetId,
	*req->resultSetStartPoint, &num, req->elementSetNames, &next, &presst);
    if (!resp.records)
    	return -1;
    resp.numberOfRecordsReturned = &num;
    resp.presentStatus = &presst;
    resp.nextResultSetPosition = &next;

    if (!z_APDU(assoc->encode, &apdup, 0))
    {
    	odr_perror(assoc->encode, "Encode presentres");
	return -1;
    }
    odr_getbuf(assoc->encode, &assoc->encoded_len);
    odr_reset(assoc->encode);
    iochan_setflags(client, EVENT_OUTPUT | EVENT_EXCEPT);
    return 0;
}
