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
/*
 * Europagate, 1995
 *
 * $Log: cclerrms.c,v $
 * Revision 1.8  1999-11-30 13:47:11  adam
 * Improved installation. Moved header files to include/yaz.
 *
 * Revision 1.7  1998/02/11 11:53:33  adam
 * Changed code so that it compiles as C++.
 *
 * Revision 1.6  1997/09/29 08:56:37  adam
 * Changed CCL parser to be thread safe. New type, CCL_parser, declared
 * and a create/destructers ccl_parser_create/ccl_parser/destory has
 * been added.
 *
 * Revision 1.5  1997/04/30 08:52:06  quinn
 * Null
 *
 * Revision 1.4  1996/10/11  15:00:24  adam
 * CCL parser from Europagate Email gateway 1.0.
 *
 * Revision 1.8  1995/05/16  09:39:25  adam
 * LICENSE.
 *
 * Revision 1.7  1995/04/17  09:31:40  adam
 * Improved handling of qualifiers. Aliases or reserved words.
 *
 * Revision 1.6  1995/02/23  08:31:59  adam
 * Changed header.
 *
 * Revision 1.4  1995/02/14  16:20:54  adam
 * Qualifiers are read from a file now.
 *
 * Revision 1.3  1995/02/14  10:25:56  adam
 * The constructions 'qualifier rel term ...' implemented.
 *
 * Revision 1.2  1995/02/13  15:15:06  adam
 * Added handling of qualifiers. Not finished yet.
 *
 * Revision 1.1  1995/02/13  12:35:20  adam
 * First version of CCL. Qualifiers aren't handled yet.
 *
 */

/* Chas: Required for the correct definition of ccl_err_msg */
#include <yaz/yaz-ccl.h>

static char *err_msg_array[] = {
    "Ok",
    "Search word expected",
    "')' expected",
    "Set name expected",
    "Operator expected",
    "Unbalanced ')'",
    "Unknown qualifier",
    "Qualifiers applied twice",
    "'=' expected",
    "Bad relation",
    "Left truncation not supported",
    "Both left - and right truncation not supported",
    "Right truncation not supported"
};

/*
 * ccl_err_msg: return name of CCL error
 * ccl_errno:   Error no.
 * return:      Name of error.
 */
const char *ccl_err_msg (int ccl_errno)
{
    return err_msg_array[ccl_errno];
}
