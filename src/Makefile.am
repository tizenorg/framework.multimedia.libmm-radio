lib_LTLIBRARIES = libmmfradio.la

includelibmmfradiodir = $(includedir)/mmf
includelibmmfradio_HEADERS = include/mm_radio.h

libmmfradio_la_SOURCES = mm_radio.c \
			 mm_radio_priv.c \
			 mm_radio_asm.c

libmmfradio_la_CFLAGS = -I. -I./include \
			$(GTHREAD_CFLAGS) \
			$(MMCOMMON_CFLAGS) \
			$(MMTA_CFLAGS) \
			$(MMSESSION_CFLAGS) \
			$(AUDIOSESSIONMGR_CFLAGS) \
			$(MMSOUND_CFLAGS) \
			$(GST_CFLAGS) \
			$(GSTAPP_CFLAGS) \
			-DMMF_LOG_OWNER=0x200000 -DMMF_DEBUG_PREFIX=\"MM-RADIO\" \
			-DGST_EXT_TIME_ANALYSIS
					
libmmfradio_la_LIBADD = $(GTHREAD_LIBS) \
			$(MMCOMMON_LIBS) \
			$(MMTA_LIBS)\
			$(MMSESSION_LIBS) \
			$(AUDIOSESSIONMGR_LIBS) \
			$(MMSOUND_LIBS)  \
			$(GST_LIBS)  \
			$(GSTAPP_LIBS)
