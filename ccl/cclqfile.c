/*
 * Copyright (c) 1995, the EUROPAGATE consortium (see below).
 *
 * The EUROPAGATE consortium members are:
 *
 *    University College Dublin
 *    Danmarks Teknologiske Videnscenter
 *    An Chomhairle Leabharlanna
 *    Consejo Superior de Investigaciones Cientificas
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation, in whole or in part, for any purpose, is hereby granted,
 * provided that:
 *
 * 1. This copyright and permission notice appear in all copies of the
 * software and its documentation. Notices of copyright or attribution
 * which appear at the beginning of any file must remain unchanged.
 *
 * 2. The names of EUROPAGATE or the project partners may not be used to
 * endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * 3. Users of this software (implementors and gateway operators) agree to
 * inform the EUROPAGATE consortium of their use of the software. This
 * information will be used to evaluate the EUROPAGATE project and the
 * software, and to plan further developments. The consortium may use
 * the information in later publications.
 * 
 * 4. Users of this software agree to make their best efforts, when
 * documenting their use of the software, to acknowledge the EUROPAGATE
 * consortium, and the role played by the software in their work.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED, OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL THE EUROPAGATE CONSORTIUM OR ITS MEMBERS BE LIABLE
 * FOR ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF
 * ANY KIND, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND
 * ON ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* CCL qualifiers
 * Europagate, 1995
 *
 * $Log: cclqfile.c,v $
 * Revision 1.3  1999-11-30 13:47:11  adam
 * Improved installation. Moved header files to include/yaz.
 *
 * Revision 1.2  1997/04/30 08:52:06  quinn
 * Null
 *
 * Revision 1.1  1996/10/11  15:00:25  adam
 * CCL parser from Europagate Email gateway 1.0.
 *
 * Revision 1.3  1995/05/16  09:39:26  adam
 * LICENSE.
 *
 * Revision 1.2  1995/05/11  14:03:56  adam
 * Changes in the reading of qualifier(s). New function: ccl_qual_fitem.
 * New variable ccl_case_sensitive, which controls whether reserved
 * words and field names are case sensitive or not.
 *
 * Revision 1.1  1995/04/17  09:31:45  adam
 * Improved handling of qualifiers. Aliases or reserved words.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <yaz/ccl.h>

void ccl_qual_fitem (CCL_bibset bibset, const char *cp, const char *qual_name)
{
    char qual_type[128];
    int no_scan;
    int pair[128];
    int pair_no = 0;

    while (1)
    {
        char *qual_value;
        char *split;
        
        if (sscanf (cp, "%s%n", qual_type, &no_scan) != 1)
            break;
        
        if (!(split = strchr (qual_type, '=')))
            break;
        cp += no_scan;
        
        *split++ = '\0';
        while (1)
        {
            int type, value;

            qual_value = split;
            if ((split = strchr (qual_value, ',')))
                *split++ = '\0';
            value = atoi (qual_value);
            switch (qual_type[0])
            {
            case 'u':
            case 'U':
                type = CCL_BIB1_USE;
                break;
            case 'r':
            case 'R':
                type = CCL_BIB1_REL;
                if (!ccl_stricmp (qual_value, "o"))
                    value = CCL_BIB1_REL_ORDER;
                break;                
            case 'p':
            case 'P':
                type = CCL_BIB1_POS;
                break;
            case 's':
            case 'S':
                type = CCL_BIB1_STR;
                if (!ccl_stricmp (qual_value, "pw"))
                    value = CCL_BIB1_STR_WP;
                break;                
            case 't':
            case 'T':
                type = CCL_BIB1_TRU;
                if (!ccl_stricmp (qual_value, "l"))
                    value = CCL_BIB1_TRU_CAN_LEFT;
                else if (!ccl_stricmp (qual_value, "r"))
                    value = CCL_BIB1_TRU_CAN_RIGHT;
                else if (!ccl_stricmp (qual_value, "b"))
                    value = CCL_BIB1_TRU_CAN_BOTH;
                else if (!ccl_stricmp (qual_value, "n"))
                    value = CCL_BIB1_TRU_CAN_NONE;
                break;                
            case 'c':
            case 'C':
                type = CCL_BIB1_COM;
                break;                
            default:
                type = atoi (qual_type);
            }
            pair[pair_no*2] = type;
            pair[pair_no*2+1] = value;
            pair_no++;
            if (!split)
                break;
        }
    }
    ccl_qual_add (bibset, qual_name, pair_no, pair);
}

/*
 * ccl_qual_file: Read bibset definition from file.
 * bibset:  Bibset
 * inf:     FILE pointer.
 *
 * Each line format is:
 *  <name> <t>=<v> <t>=<v> ....
 *  Where <name> is name of qualifier;
 *  <t>=<v> is a attribute definition pair where <t> is one of: 
 *     u(use), r(relation), p(position), t(truncation), c(completeness) 
 *     or plain integer.
 *  <v> is an integer or special pseudo-value.
 */
void ccl_qual_file (CCL_bibset bibset, FILE *inf)
{
    char line[256];
    char *cp;
    char qual_name[128];
    int  no_scan;

    while (fgets (line, 255, inf))
    {
        cp = line;
        if (*cp == '#')
            continue;        /* ignore lines starting with # */
        if (sscanf (cp, "%s%n", qual_name, &no_scan) != 1)
            continue;        /* also ignore empty lines */
        cp += no_scan;
        ccl_qual_fitem (bibset, cp, qual_name);
    }
}
