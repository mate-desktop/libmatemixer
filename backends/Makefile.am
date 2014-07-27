SUBDIRS =

if HAVE_NULL
SUBDIRS += null
endif

if HAVE_PULSEAUDIO
SUBDIRS += pulse
endif

if HAVE_OSS
SUBDIRS += oss
endif

if HAVE_OSS4
SUBDIRS += oss4
endif

-include $(top_srcdir)/git.mk
