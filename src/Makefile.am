lib_LTLIBRARIES = libmmfradio.la

includelibmmfradiodir = $(includedir)/mmf
includelibmmfradio_HEADERS = include/mm_radio.h

libmmfradio_la_SOURCES = mm_radio.c \
			 mm_radio_asm.c

if ENABLE_EMULATOR
libmmfradio_la_SOURCES += mm_radio_priv_emulator.c
else
libmmfradio_la_SOURCES += mm_radio_priv.c
endif

libmmfradio_la_CFLAGS = -I. -I./include \
			$(GTHREAD_CFLAGS) \
			$(MMCOMMON_CFLAGS) \
			$(MMSESSION_CFLAGS) \
			$(AUDIOSESSIONMGR_CFLAGS) \
			$(MMSOUND_CFLAGS) \
			$(GST_CFLAGS) \
			$(GSTAPP_CFLAGS) \
			-DMMF_LOG_OWNER=0x200000 -DMMF_DEBUG_PREFIX=\"MM-RADIO\"

libmmfradio_la_LIBADD = $(GTHREAD_LIBS) \
			$(MMCOMMON_LIBS) \
			$(MMSESSION_LIBS) \
			$(AUDIOSESSIONMGR_LIBS) \
			$(MMSOUND_LIBS)  \
			$(GST_LIBS)  \
			$(GSTAPP_LIBS)
