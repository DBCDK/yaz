/*
 * $Log: admin.c,v $
 * Revision 1.5  2000-03-17 12:47:02  adam
 * Minor changes to admin client.
 *
 * Revision 1.4  2000/03/16 13:55:49  ian
 * Added commands for sending shutdown and startup admin requests via the admin ES.
 *
 * Revision 1.3  2000/03/14 15:23:17  ian
 * Removed unwanted ifdef and include of zes-admin.h
 *
 * Revision 1.2  2000/03/14 14:06:04  ian
 * Minor change to order of debugging output for send_apdu,
 * fixed encoding of admin request.
 *
 * Revision 1.1  2000/03/14 09:27:07  ian
 * Added code to enable sending of admin extended service requests
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <yaz/yaz-util.h>

#include <yaz/tcpip.h>
#ifdef USE_XTIMOSI
#include <yaz/xmosi.h>
#endif

#include <yaz/proto.h>
#include <yaz/marcdisp.h>
#include <yaz/diagbib1.h>

#include <yaz/pquery.h>

/* Helper functions to get to various statics in the client */
ODR getODROutputStream();
void send_apdu(Z_APDU *a);



int sendAdminES(int type, char* dbname, char* param1)
{
    ODR out = getODROutputStream();

    /* Type: 1=reindex, 2=truncate, 3=delete, 4=create, 5=import, 6=refresh, 7=commit */
    Z_APDU *apdu = zget_APDU(out, Z_APDU_extendedServicesRequest );
    Z_ExtendedServicesRequest *req = apdu->u.extendedServicesRequest;
    Z_External *r;
    int oid[OID_SIZE];
    Z_ESAdminOriginPartToKeep  *toKeep;
    Z_ESAdminOriginPartNotToKeep  *notToKeep;
    oident update_oid;
    printf ("Admin request\n");
    fflush(stdout);

    /* Set up the OID for the external */
    update_oid.proto = PROTO_Z3950;
    update_oid.oclass = CLASS_EXTSERV;
    update_oid.value = VAL_ADMINSERVICE;

    oid_ent_to_oid (&update_oid, oid);
    req->packageType = odr_oiddup(out,oid);
    req->packageName = "1.Extendedserveq";

    /* Allocate the external */
    r = req->taskSpecificParameters = (Z_External *) odr_malloc (out, sizeof(*r));
    r->direct_reference = odr_oiddup(out,oid);
    r->indirect_reference = 0;
    r->descriptor = 0;
    r->which = Z_External_ESAdmin;
    r->u.adminService = (Z_Admin *) odr_malloc(out, sizeof(*r->u.adminService));
    r->u.adminService->which = Z_Admin_esRequest;
    r->u.adminService->u.esRequest = (Z_AdminEsRequest *) odr_malloc(out, sizeof(*r->u.adminService->u.esRequest));

    toKeep = r->u.adminService->u.esRequest->toKeep = (Z_ESAdminOriginPartToKeep *) 
                     odr_malloc(out, sizeof(*r->u.adminService->u.esRequest->toKeep));

    toKeep->which=type;

    switch ( type )
    {
        case Z_ESAdminOriginPartToKeep_reIndex:
	    toKeep->u.reIndex=odr_nullval();
            toKeep->databaseName = dbname;
	    break;

	case Z_ESAdminOriginPartToKeep_truncate:
	    toKeep->u.truncate=odr_nullval();
            toKeep->databaseName = dbname;
	    break;

	case Z_ESAdminOriginPartToKeep_delete:
	    toKeep->u.delete=odr_nullval();
            toKeep->databaseName = dbname;
	    break;

	case Z_ESAdminOriginPartToKeep_create:
	    toKeep->u.create=odr_nullval();
            toKeep->databaseName = dbname;
	    break;

        case Z_ESAdminOriginPartToKeep_import:
	    toKeep->u.import = (Z_ImportParameters*)odr_malloc(out, sizeof(*toKeep->u.import));
	    toKeep->u.import->recordType=param1;
            toKeep->databaseName = dbname;
	    /* Need to add additional setup of records here */
	    break;

        case Z_ESAdminOriginPartToKeep_refresh:
	    toKeep->u.refresh=odr_nullval();
            toKeep->databaseName = dbname;
	    break;

        case Z_ESAdminOriginPartToKeep_commit:
	    toKeep->u.commit=odr_nullval();
	    break;

	case Z_ESAdminOriginPartToKeep_shutdown:
	    toKeep->u.commit=odr_nullval();
	    break;
	    
	case Z_ESAdminOriginPartToKeep_start:
	    toKeep->u.commit=odr_nullval();
	    break;

        default:
	    /* Unknown admin service */
	    break;
    }

    notToKeep = r->u.adminService->u.esRequest->notToKeep = (Z_ESAdminOriginPartNotToKeep *)
	odr_malloc(out, sizeof(*r->u.adminService->u.esRequest->notToKeep));
    notToKeep->which=Z_ESAdminOriginPartNotToKeep_recordsWillFollow;
    notToKeep->u.recordsWillFollow=odr_nullval();
    
    send_apdu(apdu);

    return 0;
}

/* cmd_adm_reindex <dbname>
   Ask the specified database to fully reindex itself */
int cmd_adm_reindex(char* arg)
{
    sendAdminES(Z_ESAdminOriginPartToKeep_reIndex,arg,NULL);
    return 2;
}

/* cmd_adm_truncate <dbname>
   Truncate the specified database, removing all records and index entries, but leaving 
   the database & it's explain information intact ready for new records */
int cmd_adm_truncate(char* arg)
{
    if ( arg )
    {
        sendAdminES(Z_ESAdminOriginPartToKeep_truncate,arg,NULL);
	return 2;
    }
    return 0;
}

/* cmd_adm_create <dbname>
   Create a new database */
int cmd_adm_create(char* arg)
{
    if ( arg )
    {
        sendAdminES(Z_ESAdminOriginPartToKeep_create,arg,NULL);
	return 2;
    }
    return 0;
}

/* cmd_adm_delete <dbname>
   Delete a database */
int cmd_adm_delete(char* arg)
{
    if ( arg )
    {
        sendAdminES(Z_ESAdminOriginPartToKeep_delete,arg,NULL);
	return 2;
    }
    return 0;
}

/* cmd_adm_import <dbname> <rectype> <sourcefile>
   Import the specified updated into the database
   N.B. That in this case, the import may contain instructions to delete records as well as new or updates
   to existing records */
int cmd_adm_import(char* arg)
{
    /* Size of chunks we wish to read from import file */
    size_t chunk_size = 8192;

    /* Buffer for reading chunks of data from import file */
    char chunk_buffer[chunk_size];

    if ( arg )
    {
        char dbname_buff[32];
        char rectype_buff[32];
        char filename_buff[32];
	FILE* pImportFile = NULL;

        if (sscanf (arg, "%s %s %s", dbname_buff, rectype_buff, filename_buff) != 3)
	{
	    printf("Must specify database-name, record-type and filename for import\n");
	    return 0;
	}

        /* Attempt to open the file */

	pImportFile = fopen(filename_buff,"r");

        /* This chunk of code should move into client.c sometime soon for sending files via the update es */
	/* This function will then refer to the standard client.c one for uploading a file using es update */
	if ( pImportFile )
	{
	    int iTotalWritten = 0;

            /* We opened the import file without problems... So no we send the es request, ready to 
	       start sending fragments of the import file as segment messages */
            sendAdminES(Z_ESAdminOriginPartToKeep_import,arg,rectype_buff);

	    while ( ! feof(pImportFile ) )
	    {
	        /* Read buffer_size bytes from the file */
	        size_t num_items = fread((void*)chunk_buffer, 1, sizeof(chunk_buffer),  pImportFile);

		/* Write num_bytes of data to */

		if ( feof(pImportFile ) )
		{
		    /* This is the last chunk... Write it as the last fragment */
		    printf("Last segment of %d bytes\n", num_items);
		}
		else if ( iTotalWritten == 0 )
		{
		    printf("First segment of %d bytes\n",num_items);
		}
		else
		{
		    printf("Writing %d bytes\n", num_items);
		}

		iTotalWritten += num_items;
	    }
	}
	return 2;
    }
    return 0;
}

/* "Freshen" the specified database, by checking metadata records against the sources from which they were 
   generated, and creating a new record if the source has been touched since the last extraction */
int cmd_adm_refresh(char* arg)
{
    if ( arg )
    {
        sendAdminES(Z_ESAdminOriginPartToKeep_refresh,arg,NULL);
	return 2;
    }
    return 0;
}

/* cmd_adm_commit 
   Make imported records a permenant & visible to the live system */
int cmd_adm_commit(char* arg)
{
    sendAdminES(Z_ESAdminOriginPartToKeep_commit,NULL,NULL);
    return 2;
}

int cmd_adm_shutdown(char* arg)
{
    sendAdminES(Z_ESAdminOriginPartToKeep_shutdown,NULL,NULL);
    return 2;
}

int cmd_adm_startup(char* arg)
{
    sendAdminES(Z_ESAdminOriginPartToKeep_start,NULL,NULL);
    return 2;
}

