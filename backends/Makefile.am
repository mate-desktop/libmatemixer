SUBDIRS =

if HAVE_NULL
SUBDIRS += null
endif

if HAVE_PULSEAUDIO
SUBDIRS += pulse
endif

if HAVE_ALSA
SUBDIRS += alsa
endif

if HAVE_OSS
SUBDIRS += oss
endif

-include $(top_srcdir)/git.mk
