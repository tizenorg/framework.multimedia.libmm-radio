bin_PROGRAMS = mm_radio_testsuite

mm_radio_testsuite_SOURCES = mm_radio_testsuite.c \
												mm_radio_rt_api_test.c

mm_radio_testsuite_CFLAGS = -I$(srcdir)/../src/include \
							     $(MMCOMMON_CFLAGS) \
							     -fPIC -pie

mm_radio_testsuite_DEPENDENCIES = $(top_builddir)/src/libmmfradio.la

mm_radio_testsuite_LDADD =	    $(top_builddir)/src/libmmfradio.la \
							    $(MMCOMMON_LIBS)
