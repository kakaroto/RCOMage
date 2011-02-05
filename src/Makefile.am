# Makefile.am for RCOMage
#
# (C) Youness Alaoui (KaKaRoTo)
#
# Licensed under LGPL 2.1. See file COPYING.
#

include $(top_srcdir)/common.mk

AM_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(ZLIB_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	-DDATADIR='"$(pkgdatadir)"'

bin_PROGRAMS = rcomage

rcomage_SOURCES = \
		configscan.c \
		configscan.h \
		general.c \
		general.h \
		globdefs.c \
		main.c \
		rcodump.c \
		rcodump.h \
		rcomain.c \
		rcomain.h \
		rcoreader.c \
		rcowriter.c \
		rcofile.h \
		rlzpack.c \
		vaghandler.c \
		vaghandler.h \
		xmlread.c \
		xmlwrite.c \
		xml.h \
		vsmx.c \
		vsmx.h \
		rlzpack.h \
		strfuncs.h


rcomage_LDADD = $(top_builddir)/7z/lib7z.la \
	$(ZLIB_LIBS) $(LIBXML2_LIBS) -lstdc++
