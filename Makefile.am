ACLOCAL_AMFLAGS='-I m4' 

SUBDIRS = src test

pcfiles = mm-radio.pc

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = $(pcfiles) 

if ENABLE_EMULATOR
else
if ENABLE_SPRD
dist_udevrules_DATA = \
		rules/sprd/96-fm-radio.rules
else
dist_udevrules_DATA = \
		rules/brcm/96-fm-radio.rules
endif
endif
