/*
 * Copyright (c) 1995-2003, Index Data
 * See the file LICENSE for details.
 *
 * $Id: admin.h,v 1.6 2003-03-11 11:07:47 adam Exp $
 */

int cmd_adm_reindex(const char* arg);
int cmd_adm_truncate(const char* arg);
int cmd_adm_create(const char* arg);
int cmd_adm_drop(const char* arg);
int cmd_adm_import(const char* arg);
int cmd_adm_refresh(const char* arg);
int cmd_adm_commit(const char* arg);
int cmd_adm_shutdown(const char* arg);
int cmd_adm_startup(const char* arg);

void send_apdu(Z_APDU *a);
/*
 * Local variables:
 * tab-width: 8
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=8 fdm=marker
 * vim<600: sw=4 ts=8
 */
