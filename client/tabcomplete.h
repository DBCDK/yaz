/*
 * Copyright (c) 2002, Index Data
 * See the file LICENSE for details.
 *
 * $Id: tabcomplete.h,v 1.4 2002-08-29 19:34:44 ja7 Exp $
 */

/* 
   This file contains the compleaters for the different commands.
*/

char* complete_querytype(const char* text, int state);
char* complete_format(const char* text, int state);
char* complete_schema(const char* text, int state);
char* complete_attributeset(const char* text, int state);
char* default_completer(const char* text, int state);
char* complete_auto_reconnect(const char *text, int state);
 
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
