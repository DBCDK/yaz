# Copyright (C) 1994, Index Data I/S 
# All rights reserved.
# Sebastian Hammer, Adam Dickmeiss
# $Id: Makefile,v 1.17 1995-06-15 07:44:39 quinn Exp $

# Uncomment the lines below to enable mOSI communcation.
ODEFS=-DUSE_XTIMOSI
RFC1006=rfc1006
LIBMOSI=../../xtimosi/src/libmosi.a ../lib/librfc.a
XMOSI=xmosi.o

# Take out the Z_95 if you based your app on v.21b or earlier.
DEFS=$(ODEFS)
#CC=
SHELL=/bin/sh
MAKE=make
SUBDIR=util odr asn $(RFC1006) ccl comstack client server makelib

all:
	for i in $(SUBDIR); do cd $$i; if $(MAKE) CFLAGS="$(CFLAGS) $(DEFS)" LIBMOSI="$(LIBMOSI)" XMOSI="$(XMOSI)"; then cd ..; else exit 1; fi; done

dep depend:
	for i in $(SUBDIR); do cd $$i; if $(MAKE) depend; then cd ..; else exit 1; fi; done

clean:
	for i in $(SUBDIR); do (cd $$i; $(MAKE) clean); done
	-rm lib/*.a

cleanup:
	rm -f `find $(SUBDIR) -name "*.[oa]" -print`
	rm -f `find $(SUBDIR) -name "core" -print`
	rm -f `find $(SUBDIR) -name "errlist" -print`
	rm -f `find $(SUBDIR) -name "a.out" -print`

distclean: clean cleandepend

cleandepend: 
	for i in $(SUBDIR); do (cd $$i; \
		if sed '/^#Depend/q' <Makefile >Makefile.tmp; then \
		mv -f Makefile.tmp Makefile; fi; rm -f .depend); done

taildepend:
	for i in $(SUBDIR); do (cd $$i; \
		if sed 's/^if/#if/' <Makefile|sed 's/^include/#include/'| \
		sed 's/^endif/#endif/' | \
		sed 's/^depend: depend2/depend: depend1/g' | \
		sed '/^#Depend/q' >Makefile.tmp; then \
		mv -f Makefile.tmp Makefile; fi); done

gnudepend:
	for i in $(SUBDIR); do (cd $$i; \
		if sed '/^#Depend/q' <Makefile| \
		sed 's/^#if/if/' |sed 's/^#include/include/'| \
		sed 's/^#endif/endif/' | \
		sed 's/^depend: depend1/depend: depend2/g' >Makefile.tmp;then \
		mv -f Makefile.tmp Makefile; fi); done

wc:
	wc `find . -name '*.[ch]'`
	
