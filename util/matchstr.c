/*
 * Copyright (c) 1995-2000, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: matchstr.c,v $
 * Revision 1.5  2000-02-29 13:44:55  adam
 * Check for config.h (currently not generated).
 *
 * Revision 1.4  1999/11/30 13:47:12  adam
 * Improved installation. Moved header files to include/yaz.
 *
 * Revision 1.3  1999/10/19 12:35:42  adam
 * Minor bug fix (bug introduced by previous commit).
 *
 * Revision 1.2  1999/10/15 11:35:41  adam
 * Character '.' matches any single character.
 *
 * Revision 1.1  1999/06/08 10:10:16  adam
 * New sub directory zutil. Moved YAZ Compiler to be part of YAZ tree.
 *
 * Revision 1.7  1997/09/30 11:47:47  adam
 * Added function 'cause checkergcc doesn't include assert handler.
 *
 * Revision 1.6  1997/09/04 07:54:34  adam
 * Right hande side operand of yaz_matchstr may include a ? in
 * which case it returns "match ok".
 *
 * Revision 1.5  1997/07/21 12:48:11  adam
 * Removed windows DLL stubs.
 *
 * Revision 1.4  1997/05/01 15:07:55  adam
 * Added DLL entry point routines.
 *
 * Revision 1.3  1996/10/29 13:36:28  adam
 * Added header.
 *
 * Revision 1.2  1996/02/20 17:58:42  adam
 * Added const to yaz_matchstr.
 *
 * Revision 1.1  1996/02/20  16:33:06  quinn
 * Moved matchstr to global util
 *
 * Revision 1.1  1995/11/01  11:56:08  quinn
 * Added Retrieval (data management) functions en masse.
 *
 *
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <yaz/yaz-util.h>

/*
 * Match strings, independently of case and occurences of '-'.
 * fairly inefficient - will be replaced with an indexing scheme for
 * the various subsystems if we get a bottleneck here.
 */

int yaz_matchstr(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
	char c1 = *s1;
	char c2 = *s2;

        if (c2 == '?')
            return 0;
	if (c1 == '-')
	    c1 = *++s1;
	if (c2 == '-')
	    c2 = *++s2;
	if (!c1 || !c2)
	    break;
        if (c2 != '.')
        {
	    if (isupper(c1))
	        c1 = tolower(c1);
	    if (isupper(c2))
	        c2 = tolower(c2);
	    if (c1 != c2)
	        break;
        }
	s1++;
	s2++;
    }
    return *s1 || *s2;
}

#ifdef __GNUC__
#ifdef __CHECKER__
void __assert_fail (const char *assertion, const char *file, 
                    unsigned int line, const char *function)
{
    fprintf (stderr, "%s in file %s line %d func %s\n",
             assertion, file, line, function);
    abort ();
}
#endif
#endif
