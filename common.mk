# Makefile.am for RCOMage
#
# (C) Youness Alaoui (KaKaRoTo)
#
# Licensed under LGPL 2.1. See file COPYING.

ERROR_CFLAGS = \
	$(RCOMAGE_CFLAGS) \
	-fno-strict-aliasing \
	-Wextra \
	-Wundef \
	-Wnested-externs \
	-Wwrite-strings \
	-Wpointer-arith \
	-Wbad-function-cast \
	-Wmissing-declarations \
	-Wmissing-prototypes \
	-Wstrict-prototypes \
	-Wredundant-decls \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers
