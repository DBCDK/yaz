/* YC 0.2 Tue Feb 29 16:45:07 CET 2000 */
/* Module-C: AccessControlFormat-des-1 */

#include <yaz/z-accdes1.h>

int z_DES_RN_Object (ODR o, Z_DES_RN_Object **p, int opt, const char *name)
{
	static Odr_arm arm[] = {
		{ODR_IMPLICIT, ODR_CONTEXT, 1, Z_DES_RN_Object_challenge,
		(Odr_fun) z_DRNType, "challenge"},
		{ODR_IMPLICIT, ODR_CONTEXT, 2, Z_DES_RN_Object_response,
		(Odr_fun) z_DRNType, "response"},
		{-1, -1, -1, -1, (Odr_fun) 0, 0}
	};
	if (!odr_initmember(o, p, sizeof(**p)))
		return opt && odr_ok(o);
	if (odr_choice(o, arm, &(*p)->u, &(*p)->which, name))
		return 1;
	*p = 0;
	return opt && odr_ok(o);
}

int z_DRNType (ODR o, Z_DRNType **p, int opt, const char *name)
{
	if (!odr_sequence_begin (o, p, sizeof(**p), name))
		return opt && odr_ok (o);
	return
		odr_implicit_tag (o, odr_octetstring,
			&(*p)->userId, ODR_CONTEXT, 1, 1, "userId") &&
		odr_implicit_tag (o, odr_octetstring,
			&(*p)->salt, ODR_CONTEXT, 2, 1, "salt") &&
		odr_implicit_tag (o, odr_octetstring,
			&(*p)->randomNumber, ODR_CONTEXT, 3, 0, "randomNumber") &&
		odr_sequence_end (o);
}
