lib_LTLIBRARIES = libmmfradio.la

includelibmmfradiodir = $(includedir)/mmf
includelibmmfradio_HEADERS = include/mm_radio.h

libmmfradio_la_SOURCES = mm_radio.c \
			 mm_radio_asm.c \
			 mm_radio_utils.c

if ENABLE_EMULATOR
libmmfradio_la_SOURCES += mm_radio_priv_emulator.c
else

if ENABLE_SPRD
libmmfradio_la_SOURCES += mm_radio_priv_sprd.c
else
#libmmfradio_la_SOURCES += mm_radio_priv.c
libmmfradio_la_SOURCES += mm_radio_priv_brcm.c
endif

endif

libmmfradio_la_CFLAGS = -I. -I./include \
			$(GTHREAD_CFLAGS) \
			$(MMCOMMON_CFLAGS) \
			$(MMSESSION_CFLAGS) \
			$(AUDIOSESSIONMGR_CFLAGS) \
			$(MMSOUND_CFLAGS) \
			$(GST_CFLAGS) \
			$(INIPARSER_CFLAGS) \
			$(GSTAPP_CFLAGS) \
			-DMMF_LOG_OWNER=0x200000 -DMMF_DEBUG_PREFIX=\"MM-RADIO\" \
			$(TTRACE_CFLAGS) \
			$(DBUS_CFLAGS)
if ENABLE_SPRD
libmmfradio_la_CFLAGS += -DENABLE_SPRD
endif

libmmfradio_la_LIBADD = $(GTHREAD_LIBS) \
			$(MMCOMMON_LIBS) \
			$(MMSESSION_LIBS) \
			$(AUDIOSESSIONMGR_LIBS) \
			$(MMSOUND_LIBS)  \
			$(GST_LIBS)  \
			$(INIPARSER_LIBS)  \
			$(GSTAPP_LIBS) \
			$(TTRACE_LIBS) \
			$(DBUS_LIBS)