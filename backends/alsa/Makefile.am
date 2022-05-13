NULL =

backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-alsa.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"libmatemixer-alsa\"			\
	$(GLIB_CFLAGS)						\
	$(UDEV_CFLAGS)						\
	$(ALSA_CFLAGS)						\
	$(NULL)

libmatemixer_alsa_la_CFLAGS =					\
	$(WARN_CFLAGS)						\
	$(NULL)

libmatemixer_alsa_la_SOURCES =                                  \
	alsa-backend.c                                          \
	alsa-backend.h                                          \
	alsa-compat.h                                           \
	alsa-constants.c                                        \
	alsa-constants.h                                        \
	alsa-device.c                                           \
	alsa-device.h                                           \
	alsa-element.c                                          \
	alsa-element.h                                          \
	alsa-stream.c                                           \
	alsa-stream.h                                           \
	alsa-stream-control.c                                   \
	alsa-stream-control.h                                   \
	alsa-stream-input-control.c                             \
	alsa-stream-input-control.h                             \
	alsa-stream-output-control.c                            \
	alsa-stream-output-control.h                            \
	alsa-switch.c                                           \
	alsa-switch.h                                           \
	alsa-switch-option.c                                    \
	alsa-switch-option.h                                    \
	alsa-toggle.c                                           \
	alsa-toggle.h                                           \
	alsa-types.h

libmatemixer_alsa_la_LIBADD =                                   \
	$(top_builddir)/libmatemixer/libmatemixer.la            \
	$(GLIB_LIBS)                                            \
	$(UDEV_LIBS)						\
	$(ALSA_LIBS)

libmatemixer_alsa_la_LDFLAGS =                                  \
	-avoid-version                                          \
	-no-undefined                                           \
	-export-dynamic                                         \
	-module

-include $(top_srcdir)/git.mk
