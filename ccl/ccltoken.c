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
/* CCL - lexical analysis
 * Europagate, 1995
 *
 * $Log: ccltoken.c,v $
 * Revision 1.14  2000-03-14 09:06:11  adam
 * Added POSIX threads support for frontend server.
 *
 * Revision 1.13  2000/02/08 10:39:53  adam
 * Added a few functions to set name of operands, etc.
 *
 * Revision 1.12  2000/01/31 13:15:21  adam
 * Removed uses of assert(3). Cleanup of ODR. CCL parser update so
 * that some characters are not surrounded by spaces in resulting term.
 * ILL-code updates.
 *
 * Revision 1.11  1999/11/30 13:47:11  adam
 * Improved installation. Moved header files to include/yaz.
 *
 * Revision 1.10  1998/07/07 15:49:41  adam
 * Added braces to avoid warning.
 *
 * Revision 1.9  1998/02/11 11:53:33  adam
 * Changed code so that it compiles as C++.
 *
 * Revision 1.8  1997/09/29 08:56:38  adam
 * Changed CCL parser to be thread safe. New type, CCL_parser, declared
 * and a create/destructers ccl_parser_create/ccl_parser/destory has
 * been added.
 *
 * Revision 1.7  1997/09/01 08:48:12  adam
 * New windows NT/95 port using MSV5.0. Only a few changes made
 * to avoid warnings.
 *
 * Revision 1.6  1997/04/30 08:52:07  quinn
 * Null
 *
 * Revision 1.5  1996/10/11  15:00:26  adam
 * CCL parser from Europagate Email gateway 1.0.
 *
 * Revision 1.10  1995/07/11  12:28:31  adam
 * New function: ccl_token_simple (split into simple tokens) and
 *  ccl_token_del (delete tokens).
 *
 * Revision 1.9  1995/05/16  09:39:28  adam
 * LICENSE.
 *
 * Revision 1.8  1995/05/11  14:03:57  adam
 * Changes in the reading of qualifier(s). New function: ccl_qual_fitem.
 * New variable ccl_case_sensitive, which controls whether reserved
 * words and field names are case sensitive or not.
 *
 * Revision 1.7  1995/04/19  12:11:24  adam
 * Minor change.
 *
 * Revision 1.6  1995/04/17  09:31:48  adam
 * Improved handling of qualifiers. Aliases or reserved words.
 *
 * Revision 1.5  1995/02/23  08:32:00  adam
 * Changed header.
 *
 * Revision 1.3  1995/02/15  17:42:16  adam
 * Minor changes of the api of this module. FILE* argument added
 * to ccl_pr_tree.
 *
 * Revision 1.2  1995/02/14  19:55:13  adam
 * Header files ccl.h/cclp.h are gone! They have been merged an
 * moved to ../include/ccl.h.
 * Node kind(s) in ccl_rpn_node have changed names.
 *
 * Revision 1.1  1995/02/13  12:35:21  adam
 * First version of CCL. Qualifiers aren't handled yet.
 *
 */

#include <string.h>
#include <stdlib.h>

#include <yaz/ccl.h>

/*
 * token_cmp: Compare token with keyword(s)
 * kw:     Keyword list. Each keyword is separated by space.
 * token:  CCL token.
 * return: 1 if token string matches one of the keywords in list;
 *         0 otherwise.
 */
static int token_cmp (CCL_parser cclp, const char *kw, struct ccl_token *token)
{
    const char *cp1 = kw;
    const char *cp2;
    if (!kw)
        return 0;
    while ((cp2 = strchr (cp1, ' ')))
    {
        if (token->len == (size_t) (cp2-cp1))
        {
            if (cclp->ccl_case_sensitive)
            {
                if (!memcmp (cp1, token->name, token->len))
                    return 1;
            }
            else
            {
                if (!ccl_memicmp (cp1, token->name, token->len))
                    return 1;
            }
        }
	cp1 = cp2+1;
    }
    if (cclp->ccl_case_sensitive)
        return token->len == strlen(cp1) 
            && !memcmp (cp1, token->name, token->len);
    return token->len == strlen(cp1) &&
        !ccl_memicmp (cp1, token->name, token->len);
}

/*
 * ccl_token_simple: tokenize CCL raw tokens
 */
struct ccl_token *ccl_token_simple (const char *command)
{
    const char *cp = command;
    struct ccl_token *first = NULL;
    struct ccl_token *last = NULL;

    while (1)
    {
	while (*cp && strchr (" \t\r\n", *cp))
	{
	    cp++;
	    continue;
	}
	if (!first)
	{
	    first = last = (struct ccl_token *)malloc (sizeof (*first));
	    ccl_assert (first);
	    last->prev = NULL;
	}
	else
	{
	    last->next = (struct ccl_token *)malloc (sizeof(*first));
	    ccl_assert (last->next);
	    last->next->prev = last;
	    last = last->next;
	}
	last->next = NULL;
	last->name = cp;
	last->len = 1;
	switch (*cp++)
	{
        case '\0':
            last->kind = CCL_TOK_EOL;
            return first;
	case '\"':
	    last->kind = CCL_TOK_TERM;
	    last->name = cp;
	    last->len = 0;
	    while (*cp && *cp != '\"')
	    {
		cp++;
		++ last->len;
	    }
	    if (*cp == '\"')
		cp++;
	    break;
	default:
	    while (*cp && !strchr (" \t\n\r", *cp))
	    {
		cp++;
		++ last->len;
	    }
            last->kind = CCL_TOK_TERM;
	}
    }
    return first;
}


/*
 * ccl_tokenize: tokenize CCL command string.
 * return: CCL token list.
 */
struct ccl_token *ccl_parser_tokenize (CCL_parser cclp, const char *command)
{
    const char *cp = command;
    struct ccl_token *first = NULL;
    struct ccl_token *last = NULL;

    while (1)
    {
	while (*cp && strchr (" \t\r\n", *cp))
	{
	    cp++;
	    continue;
	}
	if (!first)
	{
	    first = last = (struct ccl_token *)malloc (sizeof (*first));
	    ccl_assert (first);
	    last->prev = NULL;
	}
	else
	{
	    last->next = (struct ccl_token *)malloc (sizeof(*first));
	    ccl_assert (last->next);
	    last->next->prev = last;
	    last = last->next;
	}
	last->next = NULL;
	last->name = cp;
	last->len = 1;
	switch (*cp++)
	{
        case '\0':
            last->kind = CCL_TOK_EOL;
            return first;
	case '(':
	    last->kind = CCL_TOK_LP;
	    break;
	case ')':
	    last->kind = CCL_TOK_RP;
	    break;
	case ',':
	    last->kind = CCL_TOK_COMMA;
	    break;
	case '%':
	case '!':
	    last->kind = CCL_TOK_PROX;
	    while (*cp == '%' || *cp == '!')
	    {
		++ last->len;
		cp++;
	    }
	    break;
	case '>':
	case '<':
	case '=':
	    if (*cp == '=' || *cp == '<' || *cp == '>')
	    {
		cp++;
		last->kind = CCL_TOK_REL;
		++ last->len;
	    }
	    else if (cp[-1] == '=')
		last->kind = CCL_TOK_EQ;
	    else
		last->kind = CCL_TOK_REL;
	    break;
	case '-':
	    last->kind = CCL_TOK_MINUS;
	    break;
	case '\"':
	    last->kind = CCL_TOK_TERM;
	    last->name = cp;
	    last->len = 0;
	    while (*cp && *cp != '\"')
	    {
		cp++;
		++ last->len;
	    }
	    if (*cp == '\"')
		cp++;
	    break;
	default:
	    while (*cp && !strchr ("(),%!><=- \t\n\r", *cp))
	    {
		cp++;
		++ last->len;
	    }
	    if (token_cmp (cclp, cclp->ccl_token_and, last))
	        last->kind = CCL_TOK_AND;
	    else if (token_cmp (cclp, cclp->ccl_token_or, last))
	        last->kind = CCL_TOK_OR;
            else if (token_cmp (cclp, cclp->ccl_token_not, last))
	        last->kind = CCL_TOK_NOT;
	    else if (token_cmp (cclp, cclp->ccl_token_set, last))
	        last->kind = CCL_TOK_SET;
	    else
		last->kind = CCL_TOK_TERM;
	}
    }
    return first;
}

struct ccl_token *ccl_tokenize (const char *command)
{
    CCL_parser cclp = ccl_parser_create ();
    struct ccl_token *list;

    list = ccl_parser_tokenize (cclp, command);

    ccl_parser_destroy (cclp);
    return list;
}

/*
 * ccl_token_del: delete CCL tokens
 */
void ccl_token_del (struct ccl_token *list)
{
    struct ccl_token *list1;

    while (list) 
    {
        list1 = list->next;
        free (list);
        list = list1;
    }
}

static char *ccl_strdup (const char *str)
{
    int len = strlen(str);
    char *p = (char*) malloc (len+1);
    strcpy (p, str);
    return p;
}

CCL_parser ccl_parser_create (void)
{
    CCL_parser p = (CCL_parser)malloc (sizeof(*p));
    if (!p)
	return p;
    p->look_token = NULL;
    p->error_code = 0;
    p->error_pos = NULL;
    p->bibset = NULL;

    p->ccl_token_and = ccl_strdup("and");
    p->ccl_token_or = ccl_strdup("or");
    p->ccl_token_not = ccl_strdup("not andnot");
    p->ccl_token_set = ccl_strdup("set");
    p->ccl_case_sensitive = 1;

    return p;
}

void ccl_parser_destroy (CCL_parser p)
{
    if (!p)
	return;
    free (p->ccl_token_and);
    free (p->ccl_token_or);
    free (p->ccl_token_not);
    free (p->ccl_token_set);
    free (p);
}

void ccl_parser_set_op_and (CCL_parser p, const char *op)
{
    if (p && op)
	p->ccl_token_and = ccl_strdup (op);
}

void ccl_parser_set_op_or (CCL_parser p, const char *op)
{
    if (p && op)
	p->ccl_token_or = ccl_strdup (op);
}
void ccl_parser_set_op_not (CCL_parser p, const char *op)
{
    if (p && op)
	p->ccl_token_not = ccl_strdup (op);
}
void ccl_parser_set_op_set (CCL_parser p, const char *op)
{
    if (p && op)
	p->ccl_token_set = ccl_strdup (op);
}

void ccl_parser_set_case (CCL_parser p, int case_sensitivity_flag)
{
    if (p)
	p->ccl_case_sensitive = case_sensitivity_flag;
}
