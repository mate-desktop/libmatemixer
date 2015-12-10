backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-alsa.la

AM_CPPFLAGS =                                                   \
	-Wno-unknown-pragmas                                    \
	-I$(top_srcdir)                                         \
	-DG_LOG_DOMAIN=\"libmatemixer-alsa\"

libmatemixer_alsa_la_CFLAGS =                                   \
	$(GLIB_CFLAGS)                                          \
	$(ALSA_CFLAGS)

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
	$(GLIB_LIBS)                                            \
	$(ALSA_LIBS)

libmatemixer_alsa_la_LDFLAGS =                                  \
	-avoid-version                                          \
	-no-undefined                                           \
	-export-dynamic                                         \
	-module

-include $(top_srcdir)/git.mk
